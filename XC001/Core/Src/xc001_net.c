#include "xc001_net.h"
#include "xc001_config.h"
#include "xc001_storage.h"
#include "xc001_scpi.h"
#include "xc001_utils.h"
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
static char s_http_page[6144];

static uint8_t query_value(const char *query, const char *key, char *out, size_t out_size)
{
  size_t key_len = strlen(key);
  const char *p = query;

  if (query == 0 || key == 0 || out == 0 || out_size == 0U)
  {
    return 0U;
  }
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
  if (n > 0)
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
  if (n > 0)
  {
    (void)http_send_all(fd, hdr, (size_t)n);
    (void)http_send_all(fd, body, body_len);
  }
}

static void send_no_content(int fd)
{
  static const char hdr[] = "HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
  (void)http_send_all(fd, hdr, sizeof(hdr) - 1U);
}

static void send_index_page(int fd)
{
  char ip[20], mask[20], gw[20];
  int n;
  static const char tpl[] =
    "<!doctype html><html><head><meta charset=utf-8><meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>XC001</title><style>"
    "body{margin:0;background:#f2f5f7;color:#1b2630;font-family:Arial,sans-serif}.top{background:#142536;color:white;padding:16px}.brand{display:flex;align-items:center;gap:14px}.logo{width:178px;height:40px;flex:0 0 auto}.top h1{margin:0;font-size:22px}.wrap{max-width:900px;margin:auto;padding:12px}.cards{display:grid;grid-template-columns:repeat(5,1fr);gap:8px}.card,.panel{background:white;border:1px solid #d6dde5;border-radius:8px;padding:12px;margin-bottom:10px}.k{font-size:12px;color:#667380}.v{font-size:17px;font-weight:700;margin-top:4px}textarea,input{width:100%%;box-sizing:border-box;border:1px solid #b7c1ca;border-radius:6px;padding:9px;font-size:14px}textarea{height:92px}button,.cfg summary{border:0;border-radius:6px;background:#0f62a8;color:white;font-weight:700;padding:9px 12px;margin:6px 6px 0 0;cursor:pointer;list-style:none}.cfg summary::-webkit-details-marker{display:none}.g{background:#edf2f7;color:#18222d;border:1px solid #d6dde5}.ok{background:#16825d}pre{background:#101820;color:#dff3ff;border-radius:8px;padding:10px;min-height:108px;white-space:pre-wrap;word-break:break-word}.cfg{position:fixed;right:12px;top:12px;z-index:2}.cfg summary{background:#edf2f7;color:#18222d;border:1px solid #d6dde5}.box{background:white;color:#1b2630;border:1px solid #d6dde5;border-radius:8px;padding:12px;width:310px;box-shadow:0 8px 22px #0003}.box label{display:block;margin-top:8px;font-size:12px;color:#667380}@media(max-width:760px){.brand{display:block}.logo{margin-bottom:8px}.cards{grid-template-columns:1fr 1fr}.cfg{position:static;margin:10px 12px}.box{width:auto}}"
    "</style></head><body><details class=cfg><summary>&#32593;&#32476;&#35774;&#32622;</summary><div class=box>"
    "<label>IP</label><input id=ip value=\"%s\">"
    "<label>&#25513;&#30721;</label><input id=mask value=\"%s\">"
    "<label>&#32593;&#20851;</label><input id=gw value=\"%s\">"
    "<label>UDP &#31471;&#21475;</label><input id=port type=number value=\"%u\">"
    "<label>&#23494;&#30721;</label><input id=pwd type=password><button class=ok onclick=saveCfg()>&#20445;&#23384;</button></div></details>"
    "<div class=top><div class=brand><svg class=logo viewBox=\"0 0 360 80\" xmlns=\"http://www.w3.org/2000/svg\"><path fill=\"#e4003a\" d=\"M7 6c20-15 76 20 130 58 9 7 8 19-3 22L28 111C18 114 4 35 7 6Z\" transform=\"scale(.34)\"/><path fill=\"#e4003a\" d=\"M152 14c26-17 56-22 64-2 6 17 6 57-6 65-7 5-22-8-54-29-10-7-10-26-4-34Z\" transform=\"scale(.34)\"/><path fill=\"#e4003a\" d=\"M32 137l136-44c10-3 16 7 9 15L47 225c-17 15-34-1-35-29-1-24 4-54 20-59Z\" transform=\"scale(.34)\"/><path fill=\"#e4003a\" d=\"M152 184l52-69c9-12 22-6 27 10 16 51 10 96-20 105-24 6-63-8-80-21-10-8 8-17 21-25Z\" transform=\"scale(.34)\"/><text x=\"86\" y=\"54\" fill=\"#e8eef5\" font-family=\"Arial\" font-size=\"42\" font-weight=\"700\">GeneralTest</text></svg><h1>XC001 &#25511;&#21046;&#26495;</h1></div></div><main class=wrap><div class=cards>"
    "<div class=card><div class=k>IP</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>&#25513;&#30721;</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>&#32593;&#20851;</div><div class=v>%s</div></div>"
    "<div class=card><div class=k>HTTP</div><div class=v>%u</div></div>"
    "<div class=card><div class=k>UDP</div><div class=v>%u</div></div>"
    "</div><section class=panel><h2>SCPI &#25351;&#20196;</h2><textarea id=cmd>*IDN?</textarea><div><button onclick=sendCmd()>&#21457;&#36865;</button><button class=g onclick=\"run('STAT?')\">STAT?</button><button class=g onclick=\"run('ND?')\">ND?</button><button class=g onclick=\"run('NET:STAT?')\">NET:STAT?</button><button class=g onclick=\"run('SYST:HELP?')\">HELP</button></div><pre id=out>Ready.</pre></section></main>"
    "<script>const $=id=>document.getElementById(id);async function sendCmd(){try{let r=await fetch('/api/cmd',{method:'POST',body:$('cmd').value,cache:'no-store'});$('out').textContent=await r.text()}catch(e){$('out').textContent='\\u901a\\u4fe1\\u5931\\u8d25: '+e.message}}function run(c){$('cmd').value=c;sendCmd()}async function saveCfg(){if(!$('pwd').value){$('out').textContent='\\u8bf7\\u8f93\\u5165\\u5bc6\\u7801';return}let u='/api/config?ip='+encodeURIComponent($('ip').value)+'&mask='+encodeURIComponent($('mask').value)+'&gw='+encodeURIComponent($('gw').value)+'&port='+encodeURIComponent($('port').value)+'&pwd='+encodeURIComponent($('pwd').value);try{let r=await fetch(u,{cache:'no-store'});let t=await r.text();$('out').textContent=t+'\\n\\u914d\\u7f6e\\u5df2\\u4fdd\\u5b58\\uff0c\\u91cd\\u542f\\u540e\\u751f\\u6548\\u3002'}catch(e){$('out').textContent='\\u4fdd\\u5b58\\u5931\\u8d25: '+e.message}}</script></body></html>";

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  XC001_Config_FormatIp(XC001_NetConfig.netmask, mask, sizeof(mask));
  XC001_Config_FormatIp(XC001_NetConfig.gateway, gw, sizeof(gw));

  n = snprintf(s_http_page, sizeof(s_http_page), tpl,
               ip, mask, gw, XC001_NetConfig.udp_port,
               ip, mask, gw, XC001_HTTP_PORT, XC001_NetConfig.udp_port);
  if (n < 0 || (size_t)n >= sizeof(s_http_page))
  {
    send_error(fd, "Page too large");
    return;
  }
  s_http_last_bytes = (uint32_t)n;
  send_response(fd, "text/html; charset=utf-8", s_http_page);
}

static void http_recv_rest(int fd, char *req, size_t req_size, int *len)
{
  char *body;
  char *cl;
  int content_length = 0;
  int header_len;

  if (req == 0 || len == 0 || *len <= 0)
  {
    return;
  }
  req[*len] = '\0';
  body = strstr(req, "\r\n\r\n");
  if (body == 0)
  {
    return;
  }
  cl = strstr(req, "Content-Length:");
  if (cl == 0)
  {
    cl = strstr(req, "content-length:");
  }
  if (cl == 0)
  {
    return;
  }
  content_length = atoi(cl + 15);
  if (content_length <= 0)
  {
    return;
  }
  header_len = (int)((body + 4) - req);
  while ((*len - header_len) < content_length && (size_t)(*len) < (req_size - 1U))
  {
    int room = (int)(req_size - 1U - (size_t)(*len));
    int n = lwip_recv(fd, req + *len, room, 0);
    if (n <= 0)
    {
      break;
    }
    *len += n;
    req[*len] = '\0';
  }
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
        rx[n] = '\0';
        XC001_SCPI_Execute(rx, tx, sizeof(tx));
        (void)lwip_sendto(sock, tx, strlen(tx), 0, (struct sockaddr *)&peer, peer_len);
      }
    }
    lwip_close(sock);
  }
}

static void handle_http(int fd, char *req)
{
  char method[8], path[256];
  char *query = 0;
  char *body_ptr;

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

  if (strcmp(path, "/") == 0)
  {
    send_index_page(fd);
  }
  else if (strcmp(path, "/favicon.ico") == 0)
  {
    send_no_content(fd);
  }
  else if (strcmp(path, "/api/config") == 0)
  {
    char body[256], ip[20], mask[20], gw[20];
    if (query != 0)
    {
      char ip_arg[24], mask_arg[24], gw_arg[24], port_arg[12], pwd_arg[24];
      uint8_t ip_bin[4], mask_bin[4], gw_bin[4];
      XC001_NetworkConfig active_cfg = XC001_NetConfig;
      uint16_t port;
      if (!query_value(query, "pwd", pwd_arg, sizeof(pwd_arg)) ||
          strcmp(pwd_arg, XC001_WEB_CONFIG_PASSWORD) != 0)
      {
        send_error(fd, "Invalid password");
        return;
      }
      if (!query_value(query, "ip", ip_arg, sizeof(ip_arg)) ||
          !query_value(query, "mask", mask_arg, sizeof(mask_arg)) ||
          !query_value(query, "gw", gw_arg, sizeof(gw_arg)) ||
          !query_value(query, "port", port_arg, sizeof(port_arg)) ||
          !XC001_Config_ParseIp(ip_arg, ip_bin) ||
          !XC001_Config_ParseIp(mask_arg, mask_bin) ||
          !XC001_Config_ParseIp(gw_arg, gw_bin) ||
          !XC001_ParseU16(port_arg, &port) ||
          !XC001_Config_SetNetworkFull(ip_bin, mask_bin, gw_bin, port) ||
          !XC001_Storage_Save(&XC001_NetConfig))
      {
        XC001_NetConfig = active_cfg;
        send_error(fd, "Invalid network config");
        return;
      }
      XC001_NetConfig = active_cfg;
      send_response(fd, "text/plain; charset=utf-8", "OK,SAVED,REBOOT_REQUIRED");
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
    if (XC001_StrCaseCmp(method, "POST") == 0 && body_ptr != 0 && body_ptr[0] != '\0')
    {
      snprintf(cmd, sizeof(cmd), "%s", body_ptr);
    }
    else if (query == 0 || !query_value(query, "c", cmd, sizeof(cmd)))
    {
      send_error(fd, "Missing command");
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
          http_recv_rest(fd, req, sizeof(req), &n);
          req[n] = '\0';
          handle_http(fd, req);
          elapsed = osKernelGetTickCount() - start_tick;
          s_http_last_ms = elapsed;
          if (elapsed > s_http_max_ms)
          {
            s_http_max_ms = elapsed;
          }
        }
        lwip_close(fd);
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
  }
  if (s_http_thread == 0)
  {
    s_http_thread = osThreadNew(http_thread, 0, &http_attr);
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
