#include "xc001_net.h"
#include "xc001_config.h"
#include "xc001_storage.h"
#include "xc001_scpi.h"
#include "xc001_utils.h"
#include "xc001_board.h"
#include "xc001_multipart.h"
#include "cmsis_os2.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lan8742.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern struct netif gnetif;
extern ETH_HandleTypeDef heth;
extern lan8742_Object_t LAN8742;

static osThreadId_t s_udp_thread;
static osThreadId_t s_http_thread;
static osThreadId_t s_fw_upload_thread;
static uint16_t s_bound_udp_port;
static volatile uint8_t s_start_services_req;
static volatile uint8_t s_http_ready;
static volatile uint32_t s_http_restart_count;
static volatile uint32_t s_http_accept_count;
static volatile uint32_t s_http_request_count;
static volatile uint32_t s_http_send_fail_count;
static volatile uint32_t s_http_last_ms;
static volatile uint32_t s_http_max_ms;
static volatile uint32_t s_http_last_bytes;
static volatile uint8_t s_reboot_requested;
static volatile uint32_t s_reboot_tick;
static char s_http_page[12288];

typedef enum
{
  XC001_FW_IDLE = 0,
  XC001_FW_ERASING,
  XC001_FW_RECEIVING,
  XC001_FW_VERIFYING,
  XC001_FW_READY,
  XC001_FW_REBOOTING,
  XC001_FW_ERROR
} XC001_FirmwareState;

typedef struct
{
  int fd;
  int request_length;
  char request[XC001_HTTP_RX_SIZE];
} XC001_FirmwareUploadContext;

typedef struct
{
  uint32_t expected_size;
  uint32_t written;
  uint8_t storage_started;
  uint8_t file_done;
  const char *error;
} XC001_MultipartFirmwareContext;

static XC001_FirmwareUploadContext s_fw_upload;
static volatile uint8_t s_fw_upload_busy;
static volatile XC001_FirmwareState s_fw_state = XC001_FW_IDLE;
static volatile uint32_t s_fw_progress;
static volatile uint32_t s_fw_received;
static volatile uint32_t s_fw_total;
static const char * volatile s_fw_message = "Ready";

static void firmware_status_update(XC001_FirmwareState state, uint32_t progress,
                                   uint32_t received, uint32_t total,
                                   const char *message)
{
  s_fw_progress = (progress > 100U) ? 100U : progress;
  s_fw_received = received;
  s_fw_total = total;
  s_fw_message = message;
  s_fw_state = state;
}

static const char *firmware_state_name(XC001_FirmwareState state)
{
  switch (state)
  {
    case XC001_FW_ERASING: return "erasing";
    case XC001_FW_RECEIVING: return "receiving";
    case XC001_FW_VERIFYING: return "verifying";
    case XC001_FW_READY: return "ready";
    case XC001_FW_REBOOTING: return "rebooting";
    case XC001_FW_ERROR: return "error";
    case XC001_FW_IDLE:
    default: return "idle";
  }
}

static uint64_t remote_key_hash(void)
{
  const uint32_t uid[3] = {HAL_GetUIDw0(), HAL_GetUIDw1(), HAL_GetUIDw2()};
  const uint8_t *bytes = (const uint8_t *)uid;
  uint64_t hash = 1469598103934665603ULL ^ 0x58433031ULL;

  for (uint32_t i = 0U; i < sizeof(uid); i++)
  {
    hash ^= bytes[i];
    hash *= 1099511628211ULL;
  }
  return hash;
}

void XC001_Net_FormatRemoteKey(char *out, size_t out_size)
{
  static const char hex[] = "0123456789ABCDEF";
  uint64_t hash;

  if (out == 0 || out_size == 0U)
  {
    return;
  }
  if (out_size < 19U)
  {
    out[0] = '\0';
    return;
  }
  hash = remote_key_hash();
  out[0] = 'X';
  out[1] = 'C';
  for (uint32_t i = 0U; i < 16U; i++)
  {
    out[2U + i] = hex[(hash >> (60U - (i * 4U))) & 0x0FULL];
  }
  out[18] = '\0';
}

static uint8_t remote_key_matches(const char *candidate)
{
  char expected[24];
  uint8_t difference = 0U;
  size_t candidate_len;
  size_t expected_len;

  if (candidate == 0)
  {
    return 0U;
  }
  XC001_Net_FormatRemoteKey(expected, sizeof(expected));
  candidate_len = strlen(candidate);
  expected_len = strlen(expected);
  if (candidate_len != expected_len)
  {
    return 0U;
  }
  for (size_t i = 0U; i < expected_len; i++)
  {
    difference |= (uint8_t)(candidate[i] ^ expected[i]);
  }
  return (difference == 0U) ? 1U : 0U;
}

static uint8_t network_config_password_matches(const char *candidate)
{
  static const char expected[] = "GTS";
  uint8_t difference = 0U;

  if (candidate == 0 || strlen(candidate) != (sizeof(expected) - 1U))
  {
    return 0U;
  }
  for (size_t i = 0U; i < (sizeof(expected) - 1U); i++)
  {
    difference |= (uint8_t)(candidate[i] ^ expected[i]);
  }
  return (difference == 0U) ? 1U : 0U;
}

static uint8_t query_value(const char *query, const char *key, char *out, size_t out_size)
{
  size_t key_len;
  const char *p;

  if (query == 0 || key == 0 || out == 0 || out_size == 0U)
  {
    return 0U;
  }
  key_len = strlen(key);
  p = query;
  while (*p != '\0')
  {
    if ((p == query || *(p - 1) == '&') && strncmp(p, key, key_len) == 0 && p[key_len] == '=')
    {
      const char *v = p + key_len + 1U;
      const char *e = strchr(v, '&');
      char raw[128];
      size_t n = (e == 0) ? strlen(v) : (size_t)(e - v);
      if (n >= sizeof(raw))
      {
        n = sizeof(raw) - 1U;
      }
      memcpy(raw, v, n);
      raw[n] = '\0';
      return XC001_UrlDecode(raw, out, out_size);
    }
    p = strchr(p, '&');
    if (p == 0)
    {
      break;
    }
    p++;
  }
  return 0U;
}

static uint8_t header_value(const char *request, const char *key, char *out, size_t out_size)
{
  const char *line;
  size_t key_len;

  if (request == 0 || key == 0 || out == 0 || out_size == 0U)
  {
    return 0U;
  }
  key_len = strlen(key);
  line = strstr(request, "\r\n");
  while (line != 0)
  {
    const char *value;
    const char *end;
    size_t len;

    line += 2;
    if (line[0] == '\r' && line[1] == '\n')
    {
      break;
    }
    end = strstr(line, "\r\n");
    if (end == 0)
    {
      break;
    }
    if ((size_t)(end - line) > key_len && line[key_len] == ':' &&
        XC001_StrNCaseCmp(line, key, key_len) == 0)
    {
      value = line + key_len + 1U;
      while (value < end && (*value == ' ' || *value == '\t'))
      {
        value++;
      }
      while (end > value && (end[-1] == ' ' || end[-1] == '\t'))
      {
        end--;
      }
      len = (size_t)(end - value);
      if (len >= out_size)
      {
        return 0U;
      }
      memcpy(out, value, len);
      out[len] = '\0';
      return 1U;
    }
    line = end;
  }
  return 0U;
}

static uint8_t remote_command(const char *input, const char *http_key,
                              char *command, size_t command_size)
{
  const char *cmd = input;
  char udp_key[32];

  if (input == 0 || command == 0 || command_size == 0U)
  {
    return 0U;
  }
  if (XC001_SCPI_IsReadOnly(input))
  {
    if (strlen(input) >= command_size)
    {
      return 0U;
    }
    snprintf(command, command_size, "%s", input);
    return 1U;
  }

  if (http_key != 0)
  {
    if (!remote_key_matches(http_key))
    {
      return 0U;
    }
  }
  else
  {
    const char *separator;
    size_t key_len;

    if (XC001_StrNCaseCmp(input, "AUTH ", 5U) != 0)
    {
      return 0U;
    }
    separator = strchr(input + 5, ';');
    if (separator == 0)
    {
      return 0U;
    }
    key_len = (size_t)(separator - (input + 5));
    while (key_len > 0U && (input[5U + key_len - 1U] == ' ' || input[5U + key_len - 1U] == '\t'))
    {
      key_len--;
    }
    if (key_len == 0U || key_len >= sizeof(udp_key))
    {
      return 0U;
    }
    memcpy(udp_key, input + 5, key_len);
    udp_key[key_len] = '\0';
    if (!remote_key_matches(udp_key))
    {
      return 0U;
    }
    cmd = XC001_SkipSpace(separator + 1);
  }

  if (*cmd == '\0' || strlen(cmd) >= command_size)
  {
    return 0U;
  }
  snprintf(command, command_size, "%s", cmd);
  return 1U;
}

static uint8_t http_send_all(int fd, const char *data, size_t len)
{
  size_t sent = 0U;
  uint8_t retry = 0U;

  while (sent < len)
  {
    size_t chunk = len - sent;
    int n;
    if (chunk > 1024U)
    {
      chunk = 1024U;
    }
    n = lwip_send(fd, data + sent, chunk, 0);
    if (n <= 0)
    {
      if (++retry >= 8U)
      {
        s_http_send_fail_count++;
        return 0U;
      }
      osDelay(2);
      continue;
    }
    sent += (size_t)n;
    retry = 0U;
  }
  return 1U;
}

static void send_response(int fd, const char *type, const char *body)
{
  char hdr[160];
  size_t body_len = strlen(body);
  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nConnection: close\r\nContent-Length: %lu\r\n\r\n",
                   type, (unsigned long)body_len);
  if (n > 0 && (size_t)n < sizeof(hdr))
  {
    (void)http_send_all(fd, hdr, (size_t)n);
    (void)http_send_all(fd, body, body_len);
  }
}

static void send_error(int fd, const char *body)
{
  char hdr[160];
  size_t body_len = strlen(body);
  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain; charset=utf-8\r\nConnection: close\r\nContent-Length: %lu\r\n\r\n",
                   (unsigned long)body_len);
  if (n > 0 && (size_t)n < sizeof(hdr))
  {
    (void)http_send_all(fd, hdr, (size_t)n);
    (void)http_send_all(fd, body, body_len);
  }
}

static void send_unauthorized(int fd)
{
  static const char body[] = "Authentication required";
  char hdr[192];
  int n = snprintf(hdr, sizeof(hdr),
                   "HTTP/1.1 401 Unauthorized\r\nContent-Type: text/plain; charset=utf-8\r\n"
                   "Cache-Control: no-store\r\nConnection: close\r\nContent-Length: %lu\r\n\r\n",
                   (unsigned long)(sizeof(body) - 1U));
  if (n > 0 && (size_t)n < sizeof(hdr))
  {
    (void)http_send_all(fd, hdr, (size_t)n);
    (void)http_send_all(fd, body, sizeof(body) - 1U);
  }
}

static void send_no_content(int fd)
{
  static const char hdr[] = "HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
  (void)http_send_all(fd, hdr, sizeof(hdr) - 1U);
}

static void send_firmware_status(int fd)
{
  char body[320];
  XC001_FirmwareState state = s_fw_state;
  uint32_t progress = s_fw_progress;
  uint32_t received = s_fw_received;
  uint32_t total = s_fw_total;
  const char *message = (const char *)s_fw_message;

  if (message == 0)
  {
    message = "";
  }
  (void)snprintf(body, sizeof(body),
                 "{\"state\":\"%s\",\"progress\":%lu,\"received\":%lu,"
                 "\"total\":%lu,\"message\":\"%s\",\"version\":\"%s\"}",
                 firmware_state_name(state), (unsigned long)progress,
                 (unsigned long)received, (unsigned long)total, message,
                 XC001_SOFTWARE_VERSION);
  send_response(fd, "application/json; charset=utf-8", body);
}

static void send_index_page(int fd)
{
  char ip[20], mask[20], gw[20];
  int n;
  static const char tpl[] =
    "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>XC001</title><style>"
    ":root{--bg:#f5f7fb;--panel:#fff;--text:#283142;--muted:#667085;--primary:#485fc7;--success:#48c78e;--danger:#f14668;--border:#e4e7ec;--dark:#141c2f}*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Inter,Segoe UI,Arial,sans-serif}.top{background:linear-gradient(135deg,#141c2f,#26385f);color:white;padding:28px 20px 44px}.brand{max-width:1120px;margin:auto;display:flex;align-items:center;gap:18px}.logo{width:178px;height:40px;flex:0 0 auto}.top h1{margin:0;font-size:26px;font-weight:700}.wrap{max-width:1120px;margin:-28px auto 0;padding:0 14px 22px}.cards{display:grid;grid-template-columns:repeat(6,1fr);gap:12px}.card,.panel,.box{background:var(--panel);border:1px solid var(--border);border-radius:14px;box-shadow:0 10px 28px #10182812}.card{padding:14px}.panel{padding:18px;margin-top:14px}.panel h2{margin:0 0 12px;font-size:20px}.k{font-size:12px;color:var(--muted);letter-spacing:.08em}.v{font-size:18px;font-weight:700;margin-top:6px}.v,.k{overflow:hidden;text-overflow:ellipsis}textarea,input{width:100%%;border:1px solid #d0d5dd;border-radius:10px;padding:10px 12px;font-size:14px;background:white;color:var(--text)}textarea{height:96px;font-family:Consolas,monospace}button,.cfg summary{border:0;border-radius:10px;background:var(--primary);color:white;font-weight:700;padding:10px 14px;margin:8px 8px 0 0;cursor:pointer;list-style:none;box-shadow:0 2px 0 #00000012}button:hover,.cfg summary:hover{filter:brightness(.96)}button:disabled{opacity:.55;cursor:not-allowed}.cfg summary::-webkit-details-marker{display:none}.g{background:#f1f3f9;color:#344054;border:1px solid var(--border)}.ok{background:var(--success);color:#06281a}pre{background:var(--dark);color:#e6f4ff;border-radius:12px;padding:12px;min-height:118px;white-space:pre-wrap;word-break:break-word}progress{width:100%%;height:18px;margin-top:12px;accent-color:var(--success)}.cfg{position:fixed;right:14px;top:14px;z-index:2}.cfg summary{background:white;color:#344054;border:1px solid var(--border)}.box{padding:14px;width:320px}.box label{display:block;margin-top:10px;font-size:12px;color:var(--muted)}#fwst{margin-top:10px;color:var(--muted);font-weight:600}@media(max-width:900px){.cards{grid-template-columns:1fr 1fr 1fr}}@media(max-width:680px){.top{padding-bottom:28px}.brand{display:block}.logo{margin-bottom:10px}.wrap{margin:0 auto}.cards{grid-template-columns:1fr 1fr}.cfg{position:static;margin:10px 14px}.box{width:auto}}"
    "</style></head><body><details class=cfg><summary>&#32593;&#32476;&#35774;&#32622;</summary><div class=box>"
    "<label>IP</label><input id=ip value=\"%s\">"
    "<label>&#25513;&#30721;</label><input id=mask value=\"%s\">"
    "<label>&#32593;&#20851;</label><input id=gw value=\"%s\">"
    "<label>UDP &#31471;&#21475;</label><input id=port type=number value=\"%u\">"
    "<label>&#23494;&#30721;</label><input id=pwd type=password><button class=ok onclick=saveCfg()>&#20445;&#23384;</button></div></details>"
    "<div class=top><div class=brand><svg class=logo viewBox=\"0 0 360 80\" xmlns=\"http://www.w3.org/2000/svg\"><path fill=\"#e4003a\" d=\"M7 6c20-15 76 20 130 58 9 7 8 19-3 22L28 111C18 114 4 35 7 6Z\" transform=\"scale(.34)\"/><path fill=\"#e4003a\" d=\"M152 14c26-17 56-22 64-2 6 17 6 57-6 65-7 5-22-8-54-29-10-7-10-26-4-34Z\" transform=\"scale(.34)\"/><path fill=\"#e4003a\" d=\"M32 137l136-44c10-3 16 7 9 15L47 225c-17 15-34-1-35-29-1-24 4-54 20-59Z\" transform=\"scale(.34)\"/><path fill=\"#e4003a\" d=\"M152 184l52-69c9-12 22-6 27 10 16 51 10 96-20 105-24 6-63-8-80-21-10-8 8-17 21-25Z\" transform=\"scale(.34)\"/><text x=\"86\" y=\"54\" fill=\"#e8eef5\" font-family=\"Arial\" font-size=\"42\" font-weight=\"700\">GeneralTest</text></svg><h1>XC001 &#25511;&#21046;&#26495;</h1></div></div><main class=wrap><div class=cards>"
    "<div class=card><div class=k>VERSION</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>IP</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>&#25513;&#30721;</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>&#32593;&#20851;</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>HTTP</div><div class=v>%u</div></div>"
    "<div class=card><div class=k>UDP</div><div class=v>%u</div></div>"
    "</div><section class=panel><h2>SCPI &#25351;&#20196;</h2><textarea id=cmd>*IDN?</textarea><div><button onclick=sendCmd()>&#21457;&#36865;</button><button class=g onclick=\"run('STAT?')\">STAT?</button><button class=g onclick=\"run('ND?')\">ND?</button><button class=g onclick=\"run('NET:STAT?')\">NET:STAT?</button><button class=g onclick=\"run('CAN:STAT?')\">CAN</button><button class=g onclick=\"run('CAN:RX?')\">CAN RX</button><button class=g onclick=\"run('RS485:STAT?')\">485</button><button class=g onclick=\"run('RS485:RX?')\">485 RX</button><button class=g onclick=\"run('SYST:HELP?')\">HELP</button></div><pre id=out>Ready.</pre></section>"
    "<section class=panel><h2>&#22266;&#20214;&#21319;&#32423;</h2><p>&#21482;&#25509;&#21463;&#26412;&#39033;&#30446;&#29983;&#25104;&#30340; .bin &#25991;&#20214;&#65292;&#26368;&#22823; %lu bytes&#12290;&#21319;&#32423;&#19981;&#38656;&#35201;&#23494;&#30721;&#65292;&#23436;&#25104;&#21518;&#35774;&#22791;&#33258;&#21160;&#37325;&#21551;&#12290;</p><input id=fw type=file accept=.bin,application/octet-stream><button id=fwbtn class=ok onclick=uploadFw()>&#19978;&#20256;&#24182;&#21319;&#32423;</button><progress id=fwpg value=0 max=100></progress><div id=fwst>Ready.</div></section></main>"
    "<script>const $=id=>document.getElementById(id);const auth=()=>({'X-XC001-Key':$('pwd').value});"
    "async function sendCmd(){try{let r=await fetch('/api',{method:'POST',headers:{...auth(),'Content-Type':'application/json'},body:JSON.stringify({method:'cmd.execute',params:{cmd:$('cmd').value}}),cache:'no-store'});let j=await r.json();$('out').textContent=(j.status?'OK: ':'ERR: ')+(j.result||'')}catch(e){$('out').textContent='\\u901a\\u4fe1\\u5931\\u8d25: '+e.message}}"
    "function run(c){$('cmd').value=c;sendCmd()}async function saveCfg(){if(!$('pwd').value){$('out').textContent='\\u8bf7\\u8f93\\u5165\\u5bc6\\u7801';return}let b=new URLSearchParams({ip:$('ip').value,mask:$('mask').value,gw:$('gw').value,port:$('port').value});try{let r=await fetch('/api/config',{method:'POST',headers:{...auth(),'Content-Type':'application/x-www-form-urlencoded'},body:b.toString(),cache:'no-store'});let t=await r.text();$('out').textContent=t+(r.ok?'\\n\\u914d\\u7f6e\\u5df2\\u4fdd\\u5b58\\uff0c\\u91cd\\u542f\\u540e\\u751f\\u6548\\u3002':'')}catch(e){$('out').textContent='\\u4fdd\\u5b58\\u5931\\u8d25: '+e.message}}"
    "let fwPoll=0,fwWaiting=false,fwStarted=false,fwTries=0;const fwName={idle:'\\u5c31\\u7eea',erasing:'\\u6b63\\u5728\\u64e6\\u9664\\u5347\\u7ea7\\u6682\\u5b58\\u533a',receiving:'\\u6b63\\u5728\\u63a5\\u6536\\u5e76\\u5199\\u5165\\u56fa\\u4ef6',verifying:'\\u6b63\\u5728\\u6821\\u9a8c\\u56fa\\u4ef6',ready:'\\u56fa\\u4ef6\\u6821\\u9a8c\\u901a\\u8fc7',rebooting:'\\u8bbe\\u5907\\u6b63\\u5728\\u91cd\\u542f\\u5e76\\u5b89\\u88c5',error:'\\u5347\\u7ea7\\u5931\\u8d25'};"
    "function stopFw(){if(fwPoll){clearInterval(fwPoll);fwPoll=0}}function showFw(s){if(!fwStarted&&(s.state==='idle'||s.state==='error'))return;fwStarted=true;let p=Number(s.progress)||0,d=s.total?' ('+s.received+' / '+s.total+' bytes)':'';$('fwpg').value=p;$('fwst').textContent=(fwName[s.state]||s.message||s.state)+' '+p+'%%'+d+(s.state==='error'&&s.message?' - '+s.message:'');if(s.state==='error'){stopFw();$('fwbtn').disabled=false}if(s.state==='rebooting'&&!fwWaiting){fwWaiting=true;fwTries=0;stopFw();setTimeout(waitFw,3000)}}"
    "async function pollFw(){try{let r=await fetch('/api/firmware/status',{cache:'no-store'});if(r.ok)showFw(await r.json())}catch(e){}}async function triggerUpgrade(){try{$('fwst').textContent='\\u56fa\\u4ef6\\u5df2\\u5199\\u5165\\uff0c\\u6b63\\u5728\\u53d1\\u9001\\u5347\\u7ea7\\u6307\\u4ee4';let r=await fetch('/api',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({method:'firmware.upgrade',params:{}}),cache:'no-store'});let j=await r.json();if(!j.status){throw new Error(j.result||j.message||'upgrade rejected')}pollFw()}catch(e){stopFw();$('fwbtn').disabled=false;$('fwst').textContent='\\u5347\\u7ea7\\u6307\\u4ee4\\u5931\\u8d25: '+e.message}}async function waitFw(){try{let r=await fetch('/api/firmware/status',{cache:'no-store'});if(r.ok){let s=await r.json();if(s.state!=='rebooting'){$('fwpg').value=100;$('fwst').textContent='\\u8bbe\\u5907\\u5df2\\u6062\\u590d\\uff0c\\u5f53\\u524d\\u7248\\u672c '+s.version;$('fwbtn').disabled=false;fwWaiting=false;return}}}catch(e){}if(++fwTries<90){setTimeout(waitFw,1000)}else{$('fwst').textContent='\\u8bbe\\u5907\\u91cd\\u542f\\u65f6\\u95f4\\u8f83\\u957f\\uff0c\\u8bf7\\u7a0d\\u540e\\u5237\\u65b0\\u9875\\u9762\\u786e\\u8ba4\\u7248\\u672c'}}"
    "function uploadFw(){let f=$('fw').files[0];if(!f){$('fwst').textContent='\\u8bf7\\u9009\\u62e9 .bin \\u6587\\u4ef6';return}stopFw();fwWaiting=false;fwStarted=false;$('fwbtn').disabled=true;$('fwpg').value=0;$('fwst').textContent='\\u6b63\\u5728\\u5efa\\u7acb\\u5347\\u7ea7\\u8fde\\u63a5';let form=new FormData();form.append('fw',f,f.name);let x=new XMLHttpRequest();x.open('POST','/firmware_upload');x.setRequestHeader('X-XC001-Firmware-Size',String(f.size));x.upload.onprogress=e=>{if(e.lengthComputable&&!fwStarted){let p=Math.min(90,Math.floor(e.loaded*90/e.total));$('fwpg').value=p;$('fwst').textContent='\\u6b63\\u5728\\u4e0a\\u4f20 '+p+'%%'}};x.onload=()=>{if(x.status===200){pollFw();setTimeout(triggerUpgrade,300)}else{stopFw();$('fwbtn').disabled=false;$('fwst').textContent=x.responseText||'Upload failed'}};x.onerror=()=>{$('fwst').textContent='\\u4e0a\\u4f20\\u8fde\\u63a5\\u4e2d\\u65ad\\uff0c\\u6b63\\u5728\\u67e5\\u8be2\\u8bbe\\u5907\\u72b6\\u6001'};x.send(form);setTimeout(pollFw,150);fwPoll=setInterval(pollFw,400)}</script></body></html>";

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  XC001_Config_FormatIp(XC001_NetConfig.netmask, mask, sizeof(mask));
  XC001_Config_FormatIp(XC001_NetConfig.gateway, gw, sizeof(gw));

  n = snprintf(s_http_page, sizeof(s_http_page), tpl,
               ip, mask, gw, XC001_NetConfig.udp_port,
               XC001_SOFTWARE_VERSION, ip, mask, gw, XC001_HTTP_PORT,
               XC001_NetConfig.udp_port,
               (unsigned long)XC001_Storage_FirmwareMaxSize());
  if (n < 0 || (size_t)n >= sizeof(s_http_page))
  {
    send_error(fd, "Page too large");
    return;
  }
  s_http_last_bytes = (uint32_t)n;
  send_response(fd, "text/html; charset=utf-8", s_http_page);
}

static int http_recv_request(int fd, char *req, size_t req_size, int len)
{
  if (req == 0 || req_size < 2U || len <= 0 || (size_t)len >= req_size)
  {
    return -1;
  }

  for (;;)
  {
    char *body;
    size_t expected = 0U;
    int room;
    int n;

    req[len] = '\0';
    body = strstr(req, "\r\n\r\n");
    if (body != 0)
    {
      char content_length_text[16];
      uint32_t content_length = 0UL;
      size_t header_length = (size_t)((body + 4) - req);

      if (header_value(req, "Content-Length", content_length_text,
                       sizeof(content_length_text)) &&
          !XC001_ParseU32(content_length_text,
                          (uint32_t)(req_size - 1U - header_length), &content_length))
      {
        return -1;
      }
      expected = header_length + content_length;
      if ((size_t)len >= expected)
      {
        req[expected] = '\0';
        return (int)expected;
      }
    }

    if ((size_t)len >= req_size - 1U)
    {
      return -1;
    }
    room = (int)(req_size - 1U - (size_t)len);
    n = lwip_recv(fd, req + len, room, 0);
    if (n <= 0)
    {
      return -1;
    }
    len += n;
  }
}

static int http_recv_headers(int fd, char *req, size_t req_size, int len)
{
  if (req == 0 || req_size < 2U || len <= 0 || (size_t)len >= req_size)
  {
    return -1;
  }
  for (;;)
  {
    int received;
    int room;

    req[len] = '\0';
    if (strstr(req, "\r\n\r\n") != 0)
    {
      return len;
    }
    if ((size_t)len >= req_size - 1U)
    {
      return -1;
    }
    room = (int)(req_size - 1U - (size_t)len);
    received = lwip_recv(fd, req + len, room, 0);
    if (received <= 0)
    {
      return -1;
    }
    len += received;
  }
}

static uint8_t request_path_is(const char *request, const char *expected_method,
                               const char *expected_path)
{
  char method[8];
  char path[64];

  return (request != 0 && sscanf(request, "%7s %63s", method, path) == 2 &&
          XC001_StrCaseCmp(method, expected_method) == 0 &&
          strcmp(path, expected_path) == 0) ? 1U : 0U;
}

static uint8_t multipart_boundary_from_content_type(const char *content_type,
                                                    char *boundary,
                                                    size_t boundary_size)
{
  const char *p;
  const char *end;
  size_t len;

  if (content_type == 0 || boundary == 0 || boundary_size == 0U ||
      XC001_StrNCaseCmp(content_type, "multipart/form-data", 19U) != 0)
  {
    return 0U;
  }
  p = content_type;
  while (*p != '\0' && XC001_StrNCaseCmp(p, "boundary=", 9U) != 0)
  {
    p++;
  }
  if (p == 0)
  {
    return 0U;
  }
  if (*p == '\0')
  {
    return 0U;
  }
  p += 9U;
  if (*p == '"')
  {
    p++;
    end = strchr(p, '"');
  }
  else
  {
    end = p;
    while (*end != '\0' && *end != ';' && *end != ' ' && *end != '\t')
    {
      end++;
    }
  }
  if (end == 0 || end <= p)
  {
    return 0U;
  }
  len = (size_t)(end - p);
  if (len >= boundary_size || len > XC001_MULTIPART_BOUNDARY_MAX)
  {
    return 0U;
  }
  memcpy(boundary, p, len);
  boundary[len] = '\0';
  return 1U;
}

static uint8_t firmware_upload_finish(int fd, uint32_t written,
                                      uint32_t expected_size,
                                      uint8_t json_reply)
{
  char reply[128];
  uint32_t image_crc = 0UL;

  if (written != expected_size)
  {
    XC001_Storage_FirmwareAbort();
    firmware_status_update(XC001_FW_ERROR, s_fw_progress, written,
                           expected_size, "Firmware size mismatch");
    send_error(fd, "Firmware size mismatch");
    return 0U;
  }
  firmware_status_update(XC001_FW_VERIFYING, 92U, written, expected_size,
                         "Validating firmware image");
  if (!XC001_Storage_FirmwareFinishWithSize(written, &image_crc))
  {
    firmware_status_update(XC001_FW_ERROR, 92U, written, expected_size,
                           "Firmware image validation failed");
    send_error(fd, "Firmware image validation failed");
    return 0U;
  }
  firmware_status_update(XC001_FW_READY, 98U, written, expected_size,
                         "Firmware ready to install");
  if (json_reply != 0U)
  {
    snprintf(reply, sizeof(reply),
             "{\"status\":true,\"crc32\":\"%08lX\",\"rebooting\":false}",
             (unsigned long)image_crc);
    send_response(fd, "application/json; charset=utf-8", reply);
  }
  else
  {
    snprintf(reply, sizeof(reply), "OK,CRC32=%08lX,READY",
             (unsigned long)image_crc);
    send_response(fd, "text/plain; charset=utf-8", reply);
  }
  return 1U;
}

static uint8_t multipart_firmware_begin(void *ctx, const char *filename)
{
  XC001_MultipartFirmwareContext *fw = (XC001_MultipartFirmwareContext *)ctx;

  (void)filename;
  if (fw == 0 || fw->storage_started != 0U)
  {
    return 0U;
  }
  firmware_status_update(XC001_FW_ERASING, 2U, 0U, fw->expected_size,
                         "Preparing firmware staging area");
  if (!XC001_Storage_FirmwareBegin(fw->expected_size))
  {
    fw->error = "Unable to prepare firmware staging area";
    return 0U;
  }
  fw->storage_started = 1U;
  firmware_status_update(XC001_FW_RECEIVING, 5U, 0U, fw->expected_size,
                         "Receiving firmware");
  return 1U;
}

static uint8_t multipart_firmware_data(void *ctx, const uint8_t *data,
                                       uint32_t length)
{
  XC001_MultipartFirmwareContext *fw = (XC001_MultipartFirmwareContext *)ctx;

  if (fw == 0 || fw->storage_started == 0U ||
      fw->written > fw->expected_size ||
      length > (fw->expected_size - fw->written))
  {
    if (fw != 0)
    {
      fw->error = "Firmware size mismatch";
    }
    return 0U;
  }
  if (!XC001_Storage_FirmwareWrite(data, length))
  {
    fw->error = "Firmware flash write failed";
    return 0U;
  }
  fw->written += length;
  firmware_status_update(XC001_FW_RECEIVING,
                         5U + (uint32_t)(((uint64_t)fw->written * 85ULL) /
                                        fw->expected_size),
                         fw->written, fw->expected_size,
                         "Receiving firmware");
  return 1U;
}

static void multipart_firmware_end(void *ctx)
{
  XC001_MultipartFirmwareContext *fw = (XC001_MultipartFirmwareContext *)ctx;

  if (fw != 0)
  {
    fw->file_done = 1U;
  }
}

static uint8_t handle_firmware_upload_raw(int fd, char *request,
                                          int received_length)
{
  char content_length_text[16];
  char *body;
  uint32_t content_length;
  uint32_t written = 0U;
  size_t header_length;
  struct timeval timeout;

  if (!request_path_is(request, "POST", "/api/firmware"))
  {
    firmware_status_update(XC001_FW_ERROR, 0U, 0U, 0U,
                           "POST /api/firmware required");
    send_error(fd, "POST /api/firmware required");
    return 0U;
  }
  body = strstr(request, "\r\n\r\n");
  if (body == 0 ||
      !header_value(request, "Content-Length", content_length_text,
                    sizeof(content_length_text)) ||
      !XC001_ParseU32(content_length_text, XC001_Storage_FirmwareMaxSize(),
                      &content_length) || content_length < 8U)
  {
    firmware_status_update(XC001_FW_ERROR, 0U, 0U, 0U,
                           "Invalid firmware length");
    send_error(fd, "Invalid firmware length");
    return 0U;
  }
  body += 4;
  header_length = (size_t)(body - request);
  firmware_status_update(XC001_FW_ERASING, 2U, 0U, content_length,
                         "Preparing firmware staging area");
  if (!XC001_Storage_FirmwareBegin(content_length))
  {
    firmware_status_update(XC001_FW_ERROR, 0U, 0U, content_length,
                           "Unable to prepare firmware staging area");
    send_error(fd, "Unable to prepare firmware staging area");
    return 0U;
  }
  firmware_status_update(XC001_FW_RECEIVING, 5U, 0U, content_length,
                         "Receiving firmware");

  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  if ((size_t)received_length > header_length)
  {
    uint32_t initial_length = (uint32_t)((size_t)received_length - header_length);
    if (initial_length > content_length)
    {
      initial_length = content_length;
    }
    if (!XC001_Storage_FirmwareWrite((const uint8_t *)body, initial_length))
    {
      XC001_Storage_FirmwareAbort();
      firmware_status_update(XC001_FW_ERROR, 0U, written, content_length,
                             "Firmware flash write failed");
      send_error(fd, "Firmware flash write failed");
      return 0U;
    }
    written = initial_length;
    firmware_status_update(XC001_FW_RECEIVING,
                           5U + (uint32_t)(((uint64_t)written * 85ULL) /
                                          content_length),
                           written, content_length, "Receiving firmware");
  }

  while (written < content_length)
  {
    uint8_t chunk[1024];
    uint32_t remaining = content_length - written;
    int request_length = (remaining < sizeof(chunk)) ? (int)remaining : (int)sizeof(chunk);
    int count = lwip_recv(fd, chunk, request_length, 0);
    if (count <= 0 || !XC001_Storage_FirmwareWrite(chunk, (uint32_t)count))
    {
      XC001_Storage_FirmwareAbort();
      firmware_status_update(XC001_FW_ERROR, s_fw_progress, written,
                             content_length, "Firmware upload interrupted");
      send_error(fd, "Firmware upload interrupted");
      return 0U;
    }
    written += (uint32_t)count;
    firmware_status_update(XC001_FW_RECEIVING,
                           5U + (uint32_t)(((uint64_t)written * 85ULL) /
                                          content_length),
                           written, content_length, "Receiving firmware");
  }

  return firmware_upload_finish(fd, written, content_length, 0U);
}

static uint8_t handle_firmware_upload_multipart(int fd, char *request,
                                                int received_length)
{
  char content_length_text[16];
  char firmware_size_text[16];
  char content_type[160];
  char boundary[XC001_MULTIPART_BOUNDARY_MAX + 1U];
  char *body;
  uint32_t content_length;
  uint32_t firmware_size;
  uint8_t firmware_size_known;
  uint32_t consumed;
  size_t header_length;
  struct timeval timeout;
  XC001_MultipartFirmwareContext fw = {0};
  XC001_MultipartParser parser;
  XC001_MultipartCallbacks callbacks = {
    .on_file_begin = multipart_firmware_begin,
    .on_file_data = multipart_firmware_data,
    .on_file_end = multipart_firmware_end,
    .ctx = &fw
  };

  body = strstr(request, "\r\n\r\n");
  firmware_size_known = header_value(request, "X-XC001-Firmware-Size",
                                     firmware_size_text,
                                     sizeof(firmware_size_text));
  if (body == 0 ||
      !header_value(request, "Content-Length", content_length_text,
                    sizeof(content_length_text)) ||
      !header_value(request, "Content-Type", content_type,
                    sizeof(content_type)) ||
      !multipart_boundary_from_content_type(content_type, boundary,
                                            sizeof(boundary)) ||
      !XC001_ParseU32(content_length_text,
                      XC001_Storage_FirmwareMaxSize() + 4096UL,
                      &content_length))
  {
    firmware_status_update(XC001_FW_ERROR, 0U, 0U, 0U,
                           "Invalid multipart firmware upload");
    send_error(fd, "Invalid multipart firmware upload");
    return 0U;
  }

  if (firmware_size_known != 0U)
  {
    if (!XC001_ParseU32(firmware_size_text, XC001_Storage_FirmwareMaxSize(),
                        &firmware_size) || firmware_size < 8U ||
        content_length <= firmware_size)
    {
      firmware_status_update(XC001_FW_ERROR, 0U, 0U, 0U,
                             "Invalid multipart firmware size");
      send_error(fd, "Invalid multipart firmware size");
      return 0U;
    }
  }
  else
  {
    firmware_size = XC001_Storage_FirmwareMaxSize();
  }

  fw.expected_size = firmware_size;
  if (!XC001_Multipart_Init(&parser, boundary, &callbacks))
  {
    firmware_status_update(XC001_FW_ERROR, 0U, 0U, firmware_size,
                           "Invalid multipart boundary");
    send_error(fd, "Invalid multipart boundary");
    return 0U;
  }
  firmware_status_update(XC001_FW_RECEIVING, 1U, 0U, firmware_size,
                         "Parsing firmware upload");

  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  body += 4;
  header_length = (size_t)(body - request);
  consumed = 0U;
  if ((size_t)received_length > header_length)
  {
    uint32_t initial_length =
        (uint32_t)((size_t)received_length - header_length);
    if (initial_length > content_length)
    {
      initial_length = content_length;
    }
    if (!XC001_Multipart_Execute(&parser, (const uint8_t *)body,
                                 initial_length))
    {
      if (fw.storage_started != 0U)
      {
        XC001_Storage_FirmwareAbort();
      }
      firmware_status_update(XC001_FW_ERROR, s_fw_progress, fw.written,
                             firmware_size,
                             fw.error ? fw.error : "Multipart parse failed");
      send_error(fd, fw.error ? fw.error : "Multipart parse failed");
      return 0U;
    }
    consumed = initial_length;
  }

  while (consumed < content_length)
  {
    uint8_t chunk[1024];
    uint32_t remaining = content_length - consumed;
    int request_length =
        (remaining < sizeof(chunk)) ? (int)remaining : (int)sizeof(chunk);
    int count = lwip_recv(fd, chunk, request_length, 0);

    if (count <= 0 ||
        !XC001_Multipart_Execute(&parser, chunk, (uint32_t)count))
    {
      if (fw.storage_started != 0U)
      {
        XC001_Storage_FirmwareAbort();
      }
      firmware_status_update(XC001_FW_ERROR, s_fw_progress, fw.written,
                             firmware_size,
                             fw.error ? fw.error : "Firmware upload interrupted");
      send_error(fd, fw.error ? fw.error : "Firmware upload interrupted");
      return 0U;
    }
    consumed += (uint32_t)count;
  }

  if (!XC001_Multipart_IsDone(&parser) ||
      !XC001_Multipart_SawFile(&parser) ||
      fw.file_done == 0U || fw.storage_started == 0U)
  {
    if (fw.storage_started != 0U)
    {
      XC001_Storage_FirmwareAbort();
    }
    firmware_status_update(XC001_FW_ERROR, s_fw_progress, fw.written,
                           firmware_size, "Firmware file field missing");
    send_error(fd, "Firmware file field missing");
    return 0U;
  }
  return firmware_upload_finish(fd, fw.written,
                                (firmware_size_known != 0U) ? firmware_size : fw.written,
                                1U);
}

static uint8_t handle_firmware_upload(int fd, char *request, int received_length)
{
  if (request_path_is(request, "POST", "/api/firmware"))
  {
    return handle_firmware_upload_raw(fd, request, received_length);
  }
  if (request_path_is(request, "POST", "/firmware_upload"))
  {
    return handle_firmware_upload_multipart(fd, request, received_length);
  }
  firmware_status_update(XC001_FW_ERROR, 0U, 0U, 0U,
                         "POST /firmware_upload required");
  send_error(fd, "POST /firmware_upload required");
  return 0U;
}

static void firmware_upload_thread(void *argument)
{
  XC001_FirmwareUploadContext *context =
      (XC001_FirmwareUploadContext *)argument;
  (void)handle_firmware_upload(context->fd, context->request,
                               context->request_length);

  lwip_close(context->fd);
  context->fd = -1;
  s_fw_upload_thread = 0;
  s_fw_upload_busy = 0U;
  osThreadExit();
}

uint8_t XC001_Net_ApplyConfig(uint8_t save_to_flash)
{
  ip4_addr_t ipaddr, netmask, gw;

  IP4_ADDR(&ipaddr, XC001_NetConfig.ip[0], XC001_NetConfig.ip[1], XC001_NetConfig.ip[2], XC001_NetConfig.ip[3]);
  IP4_ADDR(&netmask, XC001_NetConfig.netmask[0], XC001_NetConfig.netmask[1], XC001_NetConfig.netmask[2], XC001_NetConfig.netmask[3]);
  IP4_ADDR(&gw, XC001_NetConfig.gateway[0], XC001_NetConfig.gateway[1], XC001_NetConfig.gateway[2], XC001_NetConfig.gateway[3]);
  (void)netifapi_netif_set_addr(&gnetif, &ipaddr, &netmask, &gw);
  if (save_to_flash != 0U && XC001_Storage_Save(&XC001_NetConfig) == 0U)
  {
    return 0U;
  }
  return 1U;
}

static void udp_thread(void *argument)
{
  (void)argument;
  for (;;)
  {
    int sock;
    struct sockaddr_in addr;
    struct timeval tv;

    sock = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0)
    {
      osDelay(1000);
      continue;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = PP_HTONL(INADDR_ANY);
    addr.sin_port = htons(XC001_NetConfig.udp_port);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    (void)lwip_setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (lwip_bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
      lwip_close(sock);
      osDelay(1000);
      continue;
    }
    s_bound_udp_port = XC001_NetConfig.udp_port;
    while (s_bound_udp_port == XC001_NetConfig.udp_port)
    {
      char rx[XC001_NET_RX_SIZE];
      char tx[XC001_SCPI_REPLY_SIZE];
      struct sockaddr_in peer;
      socklen_t peer_len = sizeof(peer);
      int n = lwip_recvfrom(sock, rx, sizeof(rx) - 1U, 0, (struct sockaddr *)&peer, &peer_len);
      if (n > 0)
      {
        char command[XC001_SCPI_LINE_SIZE];
        rx[n] = '\0';
        if (remote_command(rx, 0, command, sizeof(command)))
        {
          XC001_SCPI_Execute(command, tx, sizeof(tx));
        }
        else
        {
          snprintf(tx, sizeof(tx), "ERR,-201,\"Authentication required\"");
        }
        (void)lwip_sendto(sock, tx, strlen(tx), 0, (struct sockaddr *)&peer, peer_len);
      }
    }
    lwip_close(sock);
  }
}

static const char *json_skip_ws(const char *p)
{
  while (p != 0 && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n'))
  {
    p++;
  }
  return p;
}

static uint8_t json_get_string(const char *json, const char *key,
                               char *out, size_t out_size)
{
  char pattern[48];
  const char *p;
  size_t pattern_len;

  if (json == 0 || key == 0 || out == 0 || out_size == 0U)
  {
    return 0U;
  }
  if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) <= 0)
  {
    return 0U;
  }
  pattern_len = strlen(pattern);
  p = json;
  while ((p = strstr(p, pattern)) != 0)
  {
    const char *v = json_skip_ws(p + pattern_len);
    size_t n = 0U;

    if (*v != ':')
    {
      p += pattern_len;
      continue;
    }
    v = json_skip_ws(v + 1U);
    if (*v != '"')
    {
      p += pattern_len;
      continue;
    }
    v++;
    while (*v != '\0' && *v != '"')
    {
      char c = *v++;
      if (c == '\\' && *v != '\0')
      {
        c = *v++;
        if (c == 'n')
        {
          c = '\n';
        }
        else if (c == 'r')
        {
          c = '\r';
        }
        else if (c == 't')
        {
          c = '\t';
        }
      }
      if (n + 1U >= out_size)
      {
        out[0] = '\0';
        return 0U;
      }
      out[n++] = c;
    }
    if (*v != '"')
    {
      out[0] = '\0';
      return 0U;
    }
    out[n] = '\0';
    return 1U;
  }
  out[0] = '\0';
  return 0U;
}

static void json_escape_copy(char *out, size_t out_size, const char *in)
{
  size_t n = 0U;

  if (out == 0 || out_size == 0U)
  {
    return;
  }
  if (in == 0)
  {
    in = "";
  }
  while (*in != '\0' && n + 1U < out_size)
  {
    char c = *in++;
    if ((c == '"' || c == '\\') && n + 2U < out_size)
    {
      out[n++] = '\\';
      out[n++] = c;
    }
    else if (c == '\r' && n + 2U < out_size)
    {
      out[n++] = '\\';
      out[n++] = 'r';
    }
    else if (c == '\n' && n + 2U < out_size)
    {
      out[n++] = '\\';
      out[n++] = 'n';
    }
    else if ((unsigned char)c >= 0x20U)
    {
      out[n++] = c;
    }
  }
  out[n] = '\0';
}

static void format_mac_compact(char *out, size_t out_size)
{
  if (out == 0 || out_size < 13U || gnetif.hwaddr_len != 6U)
  {
    if (out != 0 && out_size != 0U)
    {
      out[0] = '\0';
    }
    return;
  }
  snprintf(out, out_size, "%02X%02X%02X%02X%02X%02X",
           gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
           gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);
}

static void send_api_status(int fd, uint8_t status, const char *result)
{
  char escaped[XC001_SCPI_REPLY_SIZE + 64U];
  char body[XC001_SCPI_REPLY_SIZE + 128U];

  json_escape_copy(escaped, sizeof(escaped), result);
  snprintf(body, sizeof(body), "{\"status\":%s,\"result\":\"%s\"}",
           status ? "true" : "false", escaped);
  send_response(fd, "application/json; charset=utf-8", body);
}

static uint8_t api_password_ok(const char *body, const char *remote_key)
{
  char key[32];

  if (network_config_password_matches(remote_key))
  {
    return 1U;
  }
  if (json_get_string(body, "key", key, sizeof(key)) ||
      json_get_string(body, "password", key, sizeof(key)))
  {
    return network_config_password_matches(key);
  }
  return 0U;
}

static void send_api_info(int fd)
{
  char ip[20], mask[20], gw[20], mac[16], body[640];

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  XC001_Config_FormatIp(XC001_NetConfig.netmask, mask, sizeof(mask));
  XC001_Config_FormatIp(XC001_NetConfig.gateway, gw, sizeof(gw));
  format_mac_compact(mac, sizeof(mac));
  snprintf(body, sizeof(body),
           "{\"software\":\"%s\",\"description\":\"XC001 STM32H743 Control Board\","
           "\"manufacturer\":\"GeneralTest\",\"sn\":\"%08lX%08lX%08lX\","
           "\"network\":{\"ip\":\"%s\",\"mask\":\"%s\",\"gateway\":\"%s\",\"mac\":\"%s\"},"
           "\"model\":{\"type\":\"XC\",\"series\":\"001\",\"version\":\"%s\",\"board_name\":\"XC001\"}}",
           XC001_SOFTWARE_VERSION,
           (unsigned long)HAL_GetUIDw0(), (unsigned long)HAL_GetUIDw1(),
           (unsigned long)HAL_GetUIDw2(), ip, mask, gw, mac,
           XC001_SOFTWARE_VERSION);
  send_response(fd, "application/json; charset=utf-8", body);
}

static void send_api_network_config(int fd)
{
  char ip[20], mask[20], gw[20], body[128];

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  XC001_Config_FormatIp(XC001_NetConfig.netmask, mask, sizeof(mask));
  XC001_Config_FormatIp(XC001_NetConfig.gateway, gw, sizeof(gw));
  snprintf(body, sizeof(body), "{\"ip\":\"%s\",\"mask\":\"%s\",\"gateway\":\"%s\"}",
           ip, mask, gw);
  send_response(fd, "application/json; charset=utf-8", body);
}

static void handle_chaos_api(int fd, const char *body, const char *remote_key)
{
  char method[40];

  if (body == 0 || !json_get_string(body, "method", method, sizeof(method)))
  {
    send_api_status(fd, 0U, "Missing method");
    return;
  }

  if (strcmp(method, "info.get") == 0)
  {
    send_api_info(fd);
  }
  else if (strcmp(method, "cmd.execute") == 0 || strcmp(method, "scpi") == 0)
  {
    char raw[XC001_SCPI_LINE_SIZE];
    char cmd[XC001_SCPI_LINE_SIZE];
    char reply[XC001_SCPI_REPLY_SIZE];

    if (!json_get_string(body, "cmd", raw, sizeof(raw)))
    {
      send_api_status(fd, 0U, "Missing cmd");
      return;
    }
    if (!remote_command(raw, remote_key, cmd, sizeof(cmd)))
    {
      send_unauthorized(fd);
      return;
    }
    XC001_SCPI_Execute(cmd, reply, sizeof(reply));
    send_api_status(fd, (XC001_StrNCaseCmp(reply, "ERR,", 4U) == 0) ? 0U : 1U, reply);
  }
  else if (strcmp(method, "config.network.get") == 0)
  {
    send_api_network_config(fd);
  }
  else if (strcmp(method, "config.network.set") == 0)
  {
    char ip_arg[24], mask_arg[24], gw_arg[24];
    uint8_t ip_bin[4], mask_bin[4], gw_bin[4];
    XC001_NetworkConfig candidate;

    if (!api_password_ok(body, remote_key))
    {
      send_unauthorized(fd);
      return;
    }
    if (!json_get_string(body, "ip", ip_arg, sizeof(ip_arg)) ||
        !json_get_string(body, "mask", mask_arg, sizeof(mask_arg)) ||
        !json_get_string(body, "gateway", gw_arg, sizeof(gw_arg)) ||
        !XC001_Config_ParseIp(ip_arg, ip_bin) ||
        !XC001_Config_ParseIp(mask_arg, mask_bin) ||
        !XC001_Config_ParseIp(gw_arg, gw_bin))
    {
      send_api_status(fd, 0U, "Invalid network config");
      return;
    }
    memcpy(candidate.ip, ip_bin, sizeof(candidate.ip));
    memcpy(candidate.netmask, mask_bin, sizeof(candidate.netmask));
    memcpy(candidate.gateway, gw_bin, sizeof(candidate.gateway));
    candidate.udp_port = XC001_NetConfig.udp_port;
    if (!XC001_Config_ValidateNetworkFull(candidate.ip, candidate.netmask,
                                          candidate.gateway, candidate.udp_port) ||
        !XC001_Storage_Save(&candidate))
    {
      send_api_status(fd, 0U, "Invalid network config");
      return;
    }
    send_api_status(fd, 1U, "OK,SAVED,REBOOT_REQUIRED");
  }
  else if (strcmp(method, "firmware.upgrade") == 0)
  {
    if (s_fw_state == XC001_FW_READY)
    {
      firmware_status_update(XC001_FW_REBOOTING, 100U, s_fw_received, s_fw_total,
                             "Rebooting to install firmware");
      s_reboot_tick = osKernelGetTickCount() + 500U;
      s_reboot_requested = 1U;
    }
    send_api_status(fd, (s_fw_state == XC001_FW_REBOOTING || s_fw_state == XC001_FW_READY) ? 1U : 0U,
                    (s_fw_state == XC001_FW_ERROR) ? "Firmware upload failed" : "OK");
  }
  else if (strcmp(method, "config.reset") == 0)
  {
    if (!api_password_ok(body, remote_key))
    {
      send_unauthorized(fd);
      return;
    }
    XC001_Config_LoadDefaults();
    send_api_status(fd, XC001_Storage_Save(&XC001_NetConfig), "OK,SAVED,REBOOT_REQUIRED");
  }
  else if (strcmp(method, "links.get") == 0)
  {
    send_response(fd, "application/json; charset=utf-8", "{\"list\":[{\"links\":[]}]}");
  }
  else if (strcmp(method, "node.map.get") == 0)
  {
    send_response(fd, "application/json; charset=utf-8",
                  "{\"node_left\":[{\"label\":\"SCPI\",\"band\":\"SCPI\"},{\"label\":\"CAN\",\"band\":\"CAN\"}],"
                  "\"node_middle\":[],"
                  "\"node_right\":[{\"label\":\"RS485\",\"band\":\"RS485\"},{\"label\":\"NET\",\"band\":\"NET\"}]}");
  }
  else
  {
    send_api_status(fd, 0U, "Unsupported method");
  }
}
static void handle_http(int fd, char *req)
{
  char method[8], path[256];
  char *query = 0;
  char *body_ptr;
  char remote_key[32] = {0};

  if (sscanf(req, "%7s %255s", method, path) != 2)
  {
    send_error(fd, "Bad request");
    return;
  }
  query = strchr(path, '?');
  if (query != 0)
  {
    *query++ = '\0';
  }
  body_ptr = strstr(req, "\r\n\r\n");
  if (body_ptr != 0)
  {
    body_ptr += 4;
  }
  (void)header_value(req, "X-XC001-Key", remote_key, sizeof(remote_key));

  if (strcmp(path, "/") == 0)
  {
    if (XC001_StrCaseCmp(method, "GET") == 0)
    {
      send_index_page(fd);
    }
    else
    {
      send_error(fd, "Method not allowed");
    }
  }
  else if (strcmp(path, "/favicon.ico") == 0)
  {
    send_no_content(fd);
  }
  else if (strcmp(path, "/api") == 0)
  {
    if (XC001_StrCaseCmp(method, "POST") != 0 || body_ptr == 0)
    {
      send_error(fd, "POST JSON body required");
      return;
    }
    handle_chaos_api(fd, body_ptr, remote_key);
  }
  else if (strcmp(path, "/doc.md") == 0)
  {
    send_response(fd, "text/markdown; charset=utf-8",
                  "# XC001 SCPI\n\n*IDN?\nSTAT?\nNET:STAT?\nCAN:STAT?\nCAN:RX?\nCAN:SEND id,hex\nRS485:STAT?\nRS485:RX?\nRS485:SEND text\n");
  }
  else if (strcmp(path, "/link.model") == 0)
  {
    send_response(fd, "application/json; charset=utf-8",
                  "{\"node_left\":[{\"label\":\"SCPI\",\"band\":\"SCPI\"},{\"label\":\"CAN\",\"band\":\"CAN\"}],\"node_middle\":[],\"node_right\":[{\"label\":\"RS485\",\"band\":\"RS485\"},{\"label\":\"NET\",\"band\":\"NET\"}]}");
  }
  else if (strcmp(path, "/link.png") == 0)
  {
    send_no_content(fd);
  }  else if (strcmp(path, "/api/firmware/status") == 0)
  {
    if (XC001_StrCaseCmp(method, "GET") != 0)
    {
      send_error(fd, "Method not allowed");
      return;
    }
    send_firmware_status(fd);
  }
  else if (strcmp(path, "/api/config") == 0)
  {
    char body[256], ip[20], mask[20], gw[20];
    if (XC001_StrCaseCmp(method, "POST") == 0)
    {
      char ip_arg[24], mask_arg[24], gw_arg[24], port_arg[12];
      uint8_t ip_bin[4], mask_bin[4], gw_bin[4];
      XC001_NetworkConfig candidate;
      uint16_t port;

      if (!network_config_password_matches(remote_key))
      {
        send_unauthorized(fd);
        return;
      }
      if (body_ptr == 0 ||
          !query_value(body_ptr, "ip", ip_arg, sizeof(ip_arg)) ||
          !query_value(body_ptr, "mask", mask_arg, sizeof(mask_arg)) ||
          !query_value(body_ptr, "gw", gw_arg, sizeof(gw_arg)) ||
          !query_value(body_ptr, "port", port_arg, sizeof(port_arg)) ||
          !XC001_Config_ParseIp(ip_arg, ip_bin) ||
          !XC001_Config_ParseIp(mask_arg, mask_bin) ||
          !XC001_Config_ParseIp(gw_arg, gw_bin) ||
          !XC001_ParseU16(port_arg, &port))
      {
        send_error(fd, "Invalid network config");
        return;
      }
      memcpy(candidate.ip, ip_bin, sizeof(candidate.ip));
      memcpy(candidate.netmask, mask_bin, sizeof(candidate.netmask));
      memcpy(candidate.gateway, gw_bin, sizeof(candidate.gateway));
      candidate.udp_port = port;
      if (!XC001_Config_ValidateNetworkFull(candidate.ip, candidate.netmask,
                                            candidate.gateway, candidate.udp_port) ||
          !XC001_Storage_Save(&candidate))
      {
        send_error(fd, "Invalid network config");
        return;
      }
      send_response(fd, "text/plain; charset=utf-8", "OK,SAVED,REBOOT_REQUIRED");
      return;
    }
    if (XC001_StrCaseCmp(method, "GET") != 0)
    {
      send_error(fd, "Method not allowed");
      return;
    }
    XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
    XC001_Config_FormatIp(XC001_NetConfig.netmask, mask, sizeof(mask));
    XC001_Config_FormatIp(XC001_NetConfig.gateway, gw, sizeof(gw));
    snprintf(body, sizeof(body), "{\"ip\":\"%s\",\"netmask\":\"%s\",\"gateway\":\"%s\",\"udp_port\":%u,\"http_port\":%u}", ip, mask, gw, XC001_NetConfig.udp_port, XC001_HTTP_PORT);
    send_response(fd, "application/json; charset=utf-8", body);
  }
  else if (strcmp(path, "/api/cmd") == 0)
  {
    char cmd[XC001_SCPI_LINE_SIZE], reply[XC001_SCPI_REPLY_SIZE];
    if (XC001_StrCaseCmp(method, "POST") != 0 || body_ptr == 0 || body_ptr[0] == '\0')
    {
      send_error(fd, "POST command body required");
      return;
    }
    if (!remote_command(body_ptr, remote_key, cmd, sizeof(cmd)))
    {
      send_unauthorized(fd);
      return;
    }
    XC001_SCPI_Execute(cmd, reply, sizeof(reply));
    send_response(fd, "text/plain; charset=utf-8", reply);
  }
  else
  {
    send_error(fd, "Not found");
  }
}

static void http_thread(void *argument)
{
  (void)argument;
  for (;;)
  {
    int srv;
    struct sockaddr_in addr;
    int opt = 1;

    s_http_ready = 0U;
    srv = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv < 0)
    {
      s_http_restart_count++;
      osDelay(1000);
      continue;
    }
    (void)lwip_setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = PP_HTONL(INADDR_ANY);
    addr.sin_port = htons(XC001_HTTP_PORT);
    if (lwip_bind(srv, (struct sockaddr *)&addr, sizeof(addr)) != 0 || lwip_listen(srv, 4) != 0)
    {
      lwip_close(srv);
      s_http_restart_count++;
      osDelay(1000);
      continue;
    }
    s_http_ready = 1U;
    for (;;)
    {
      int fd = lwip_accept(srv, 0, 0);
      if (fd >= 0)
      {
        char req[XC001_HTTP_RX_SIZE];
        struct timeval tv;
        int nodelay = 1;
        uint8_t handed_off = 0U;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;
        (void)lwip_setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        (void)lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        (void)lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        int n = lwip_recv(fd, req, sizeof(req) - 1U, 0);
        s_http_accept_count++;
        if (n > 0)
        {
          uint32_t start_tick = osKernelGetTickCount();
          uint32_t elapsed;
          s_http_request_count++;
          n = http_recv_headers(fd, req, sizeof(req), n);
          if (n > 0)
          {
            if (request_path_is(req, "POST", "/api/firmware") ||
                request_path_is(req, "POST", "/firmware_upload"))
            {
              if (s_fw_upload_busy != 0U)
              {
                send_error(fd, "Firmware update already in progress");
              }
              else
              {
                const osThreadAttr_t fw_attr = {
                  .name = "xc001_fwup",
                  .stack_size = 1536 * 4,
                  .priority = (osPriority_t)osPriorityNormal
                };

                s_fw_upload.fd = fd;
                s_fw_upload.request_length = n;
                memcpy(s_fw_upload.request, req, (size_t)n);
                s_fw_upload.request[n] = '\0';
                s_fw_upload_busy = 1U;
                firmware_status_update(XC001_FW_ERASING, 1U, 0U, 0U,
                                       "Starting firmware update");
                s_fw_upload_thread = osThreadNew(firmware_upload_thread,
                                                 &s_fw_upload, &fw_attr);
                if (s_fw_upload_thread == 0)
                {
                  s_fw_upload_busy = 0U;
                  firmware_status_update(XC001_FW_ERROR, 0U, 0U, 0U,
                                         "Unable to start firmware update task");
                  send_error(fd, "Unable to start firmware update task");
                }
                else
                {
                  handed_off = 1U;
                }
              }
            }
            else
            {
              n = http_recv_request(fd, req, sizeof(req), n);
              if (n > 0)
              {
                handle_http(fd, req);
              }
              else
              {
                send_error(fd, "Incomplete or oversized request");
              }
            }
          }
          else
          {
            send_error(fd, "Incomplete or oversized request");
          }
          elapsed = osKernelGetTickCount() - start_tick;
          s_http_last_ms = elapsed;
          if (elapsed > s_http_max_ms)
          {
            s_http_max_ms = elapsed;
          }
        }
        if (handed_off == 0U)
        {
          lwip_close(fd);
        }
      }
      else
      {
        break;
      }
    }
    s_http_ready = 0U;
    lwip_close(srv);
    s_http_restart_count++;
    osDelay(1000);
  }
}

void XC001_Net_Init(void)
{
  if (XC001_NET_SERVICES_AUTOSTART != 0U)
  {
    XC001_Net_StartServices();
  }
}

void XC001_Net_StartServices(void)
{
  const osThreadAttr_t udp_attr = {
    .name = "xc001_udp",
    .stack_size = 1536 * 4,
    .priority = (osPriority_t)osPriorityNormal
  };
  const osThreadAttr_t http_attr = {
    .name = "xc001_http",
    .stack_size = 2048 * 4,
    .priority = (osPriority_t)osPriorityNormal
  };

  if (s_udp_thread == 0)
  {
    s_udp_thread = osThreadNew(udp_thread, 0, &udp_attr);
    if (s_udp_thread == 0)
    {
      XC001_Board_SetStatusOk(0U);
    }
  }
  if (s_http_thread == 0)
  {
    s_http_thread = osThreadNew(http_thread, 0, &http_attr);
    if (s_http_thread == 0)
    {
      XC001_Board_SetStatusOk(0U);
    }
  }
}

void XC001_Net_RequestStartServices(void)
{
  if (s_udp_thread == 0 || s_http_thread == 0)
  {
    s_start_services_req = 1U;
  }
}

void XC001_Net_Task(void)
{
  if (s_reboot_requested != 0U &&
      (int32_t)(osKernelGetTickCount() - s_reboot_tick) >= 0)
  {
    __DSB();
    NVIC_SystemReset();
  }
  if (s_start_services_req != 0U)
  {
    s_start_services_req = 0U;
    XC001_Net_StartServices();
  }
}

void XC001_Net_Diag(char *out, uint32_t out_size)
{
  snprintf(out, out_size,
           "NET:IP=%u.%u.%u.%u,UP=%u,LINK=%u,HTTP=%u,HTTP_ACC=%lu,HTTP_REQ=%lu,HTTP_RST=%lu,HTTP_TXERR=%lu,HTTP_MS=%lu,HTTP_MAX=%lu,HTTP_BYTES=%lu,PHY_INIT=%lu,PHY_ADDR=%lu,ETH_ERR=0x%08lX,DMA_ERR=0x%08lX,DMACSR=0x%08lX,MACMDIOAR=0x%08lX",
           XC001_NetConfig.ip[0], XC001_NetConfig.ip[1], XC001_NetConfig.ip[2], XC001_NetConfig.ip[3],
           netif_is_up(&gnetif) ? 1U : 0U,
           netif_is_link_up(&gnetif) ? 1U : 0U,
           s_http_ready,
           (unsigned long)s_http_accept_count,
           (unsigned long)s_http_request_count,
           (unsigned long)s_http_restart_count,
           (unsigned long)s_http_send_fail_count,
           (unsigned long)s_http_last_ms,
           (unsigned long)s_http_max_ms,
           (unsigned long)s_http_last_bytes,
           (unsigned long)LAN8742.Is_Initialized,
           (unsigned long)LAN8742.DevAddr,
           (unsigned long)HAL_ETH_GetError(&heth),
           (unsigned long)HAL_ETH_GetDMAError(&heth),
           (unsigned long)heth.Instance->DMACSR,
           (unsigned long)heth.Instance->MACMDIOAR);
}

void XC001_Net_PhyDiag(char *out, uint32_t out_size)
{
  uint32_t bsr = 0xFFFFFFFFUL;
  uint32_t phyid1 = 0xFFFFFFFFUL;
  uint32_t phyid2 = 0xFFFFFFFFUL;
  uint32_t physcsr = 0xFFFFFFFFUL;
  int32_t link = LAN8742_STATUS_ERROR;
  uint32_t dev = LAN8742.DevAddr;

  if (LAN8742.Is_Initialized != 0U && dev <= 31U)
  {
    (void)HAL_ETH_ReadPHYRegister(&heth, dev, LAN8742_BSR, &bsr);
    (void)HAL_ETH_ReadPHYRegister(&heth, dev, LAN8742_PHYI1R, &phyid1);
    (void)HAL_ETH_ReadPHYRegister(&heth, dev, LAN8742_PHYI2R, &phyid2);
    (void)HAL_ETH_ReadPHYRegister(&heth, dev, LAN8742_PHYSCSR, &physcsr);
    link = LAN8742_GetLinkState(&LAN8742);
  }

  snprintf(out, out_size,
           "PHY:INIT=%lu,ADDR=%lu,LINK=%ld,BSR=0x%04lX,PHYID=0x%04lX:0x%04lX,PHYSCSR=0x%04lX,ETH_ERR=0x%08lX,DMA_ERR=0x%08lX,DMACSR=0x%08lX,MACMDIOAR=0x%08lX",
           (unsigned long)LAN8742.Is_Initialized,
           (unsigned long)dev,
           (long)link,
           (unsigned long)bsr,
           (unsigned long)phyid1,
           (unsigned long)phyid2,
           (unsigned long)physcsr,
           (unsigned long)HAL_ETH_GetError(&heth),
           (unsigned long)HAL_ETH_GetDMAError(&heth),
           (unsigned long)heth.Instance->DMACSR,
           (unsigned long)heth.Instance->MACMDIOAR);
}

void XC001_Net_FormatMac(char *out, size_t out_size)
{
  if (out == 0 || out_size == 0U)
  {
    return;
  }
  if (gnetif.hwaddr_len != 6U)
  {
    snprintf(out, out_size, "UNAVAILABLE");
    return;
  }
  snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
           gnetif.hwaddr[0], gnetif.hwaddr[1], gnetif.hwaddr[2],
           gnetif.hwaddr[3], gnetif.hwaddr[4], gnetif.hwaddr[5]);
}
