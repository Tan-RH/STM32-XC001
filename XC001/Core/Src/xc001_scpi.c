#include "xc001_scpi.h"
#include "xc001_config.h"
#include "xc001_utils.h"
#include "xc001_board.h"
#include "xc001_can.h"
#include "xc001_rs485.h"
#include "xc001_spi_bus.h"
#include "xc001_net.h"
#include "xc001_storage.h"
#include "xc001_console.h"
#include "stm32h7xx_hal.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static osMutexId_t s_scpi_mutex;

void XC001_SCPI_Init(void)
{
  if (s_scpi_mutex == 0)
  {
    const osMutexAttr_t attr = {.name = "xc001_scpi"};
    s_scpi_mutex = osMutexNew(&attr);
    if (s_scpi_mutex == 0)
    {
      XC001_Board_SetStatusOk(0U);
    }
  }
}

static uint8_t save_network_candidate(const uint8_t ip[4], const uint8_t mask[4],
                                      const uint8_t gateway[4], uint16_t port)
{
  XC001_NetworkConfig candidate;

  memcpy(candidate.ip, ip, sizeof(candidate.ip));
  memcpy(candidate.netmask, mask, sizeof(candidate.netmask));
  memcpy(candidate.gateway, gateway, sizeof(candidate.gateway));
  candidate.udp_port = port;
  if (!XC001_Config_ValidateNetworkFull(candidate.ip, candidate.netmask,
                                        candidate.gateway, candidate.udp_port))
  {
    return 0U;
  }
  return XC001_Storage_Save(&candidate);
}

static uint8_t parse_rf_channel(const char *text, uint8_t *channel)
{
  char *end;
  unsigned long value;
  uint8_t close_paren = 0U;

  if (text == 0 || channel == 0)
  {
    return 0U;
  }

  text = XC001_SkipSpace(text);
  if (*text == '(')
  {
    close_paren = 1U;
    text = XC001_SkipSpace(text + 1);
  }
  if (*text == '@')
  {
    text = XC001_SkipSpace(text + 1);
  }
  if (XC001_StrNCaseCmp(text, "RF", 2U) == 0)
  {
    text = XC001_SkipSpace(text + 2);
  }
  if (!isdigit((unsigned char)*text))
  {
    return 0U;
  }

  value = strtoul(text, &end, 10);
  end = (char *)XC001_SkipSpace(end);
  if (close_paren != 0U)
  {
    if (*end != ')')
    {
      return 0U;
    }
    end = (char *)XC001_SkipSpace(end + 1);
  }
  if (*end != '\0' || value < 1UL || value > 8UL)
  {
    return 0U;
  }

  *channel = (uint8_t)value;
  return 1U;
}

static uint8_t parse_power_query(char *text, uint8_t *node_id,
                                 uint32_t *frequency_khz, uint8_t *sample_time_ms,
                                 uint8_t *sample_mode, uint16_t *trigger_threshold,
                                 uint16_t *trigger_timeout_ms,
                                 uint8_t *antenna_compensation)
{
  uint32_t values[7] = {0U, 0U, 20U, 0U, 0U, 1000U, 0U};
  uint8_t count = 0U;
  char *token = text;

  while (token != 0 && count < 7U)
  {
    char *comma = strchr(token, ',');
    if (comma != 0)
    {
      *comma = '\0';
    }
    if (!XC001_ParseU32(XC001_Trim(token), 0xFFFFFFUL, &values[count]))
    {
      return 0U;
    }
    count++;
    token = (comma != 0) ? (comma + 1) : 0;
  }
  if (token != 0 || count < 2U || values[0] < 1U || values[0] > 40U ||
      values[1] == 0U || values[2] == 0U || values[2] > 255U ||
      values[3] > 2U || values[4] > 65535U || values[5] > 65535U ||
      values[6] > 1U)
  {
    return 0U;
  }

  *node_id = (uint8_t)values[0];
  *frequency_khz = values[1];
  *sample_time_ms = (uint8_t)values[2];
  *sample_mode = (uint8_t)values[3];
  *trigger_threshold = (uint16_t)values[4];
  *trigger_timeout_ms = (uint16_t)values[5];
  *antenna_compensation = (uint8_t)values[6];
  return 1U;
}

static void format_cdbm(int16_t value, char *out, size_t out_size)
{
  int32_t magnitude = value;
  const char *sign = "";

  if (magnitude < 0)
  {
    sign = "-";
    magnitude = -magnitude;
  }
  snprintf(out, out_size, "%s%ld.%02ld", sign,
           (long)(magnitude / 100L), (long)(magnitude % 100L));
}

static void indicate_scpi_error(const char *reply)
{
  if (reply != 0 && XC001_StrNCaseCmp(reply, "ERR,", 4U) == 0)
  {
    XC001_Board_IndicateCommandError();
  }
}

uint8_t XC001_SCPI_IsReadOnly(const char *command)
{
  static const char *const exact_commands[] = {
    "*IDN?", "IDN?", "PING?", "*STB?", "*OPC?", "*WAI", "SYST:ERR?", "SYST:ERR:NEXT?", "SYST:ERR:COUN?", "SYST:ERR:COUNT?", "SYSTEM:ERROR?", "SYSTEM:ERROR:NEXT?", "SYSTEM:ERROR:COUNT?", "SYST:HELP?", "SYST:CLOCK?", "STAT?",
    "NET:STAT?", "NET:DIAG?", "ND?", "NET:PHY?", "NET:IP?",
    "NET:MASK?", "NET:GATE?", "NET:PORT?", "NET:MAC?", "DIG:LIST?",
    "ROUT:CHAN?", "ROUT:STAT?", "ROUTE:CHANNEL?", "ROUTE:STATE?",
    "CAN:STAT?", "CAN:RX?", "CAN:POW?", "MEAS:POW?", "MEASURE:POWER?",
    "RS485:STAT?", "RS485:RX?", "SPI:STAT?"
  };
  char line[XC001_SCPI_LINE_SIZE];
  char *cmd;

  if (command == 0)
  {
    return 0U;
  }
  snprintf(line, sizeof(line), "%s", command);
  cmd = XC001_Trim(line);
  for (uint32_t i = 0U; i < (sizeof(exact_commands) / sizeof(exact_commands[0])); i++)
  {
    if (XC001_StrCaseCmp(cmd, exact_commands[i]) == 0)
    {
      return 1U;
    }
  }
  return (XC001_StrNCaseCmp(cmd, "DIG:OUTP? ", 10U) == 0 ||
          XC001_StrNCaseCmp(cmd, "CAN:POW? ", 9U) == 0 ||
          XC001_StrNCaseCmp(cmd, "MEAS:POW? ", 10U) == 0 ||
          XC001_StrNCaseCmp(cmd, "MEASURE:POWER? ", 15U) == 0) ? 1U : 0U;
}

static void reply_net_config(char *reply, size_t reply_size)
{
  char ip[20], gw[20], mask[20];

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  XC001_Config_FormatIp(XC001_NetConfig.gateway, gw, sizeof(gw));
  XC001_Config_FormatIp(XC001_NetConfig.netmask, mask, sizeof(mask));
  snprintf(reply, reply_size, "IP=%s,GW=%s,MASK=%s,UDP=%u,HTTP=%u",
           ip, gw, mask, XC001_NetConfig.udp_port, XC001_HTTP_PORT);
}

void XC001_SCPI_Execute(const char *command, char *reply, size_t reply_size)
{
  char line[XC001_SCPI_LINE_SIZE];
  char *cmd;
  uint8_t mutex_locked = 0U;

  if (reply == 0 || reply_size == 0U)
  {
    return;
  }
  reply[0] = '\0';
  if (command == 0)
  {
    snprintf(reply, reply_size, "ERR,-100,\"Empty command\"");
    indicate_scpi_error(reply);
    return;
  }

  snprintf(line, sizeof(line), "%s", command);
  cmd = XC001_Trim(line);
  if (*cmd == '\0')
  {
    snprintf(reply, reply_size, "ERR,-100,\"Empty command\"");
    indicate_scpi_error(reply);
    return;
  }

  if (s_scpi_mutex == 0)
  {
    snprintf(reply, reply_size, "ERR,-300,\"Command executor unavailable\"");
    indicate_scpi_error(reply);
    return;
  }
  if (osMutexAcquire(s_scpi_mutex, osWaitForever) != osOK)
  {
    snprintf(reply, reply_size, "ERR,-300,\"Command executor unavailable\"");
    indicate_scpi_error(reply);
    return;
  }
  mutex_locked = 1U;

  if (XC001_StrCaseCmp(cmd, "*IDN?") == 0 || XC001_StrCaseCmp(cmd, "IDN?") == 0)
  {
    snprintf(reply, reply_size, "%s", XC001_DEVICE_ID);
  }
  else if (XC001_StrCaseCmp(cmd, "PING?") == 0)
  {
    snprintf(reply, reply_size, "PONG");
  }
  else if (XC001_StrCaseCmp(cmd, "*CLS") == 0)
  {
    snprintf(reply, reply_size, "OK");
  }
  else if (XC001_StrCaseCmp(cmd, "*RST") == 0)
  {
    XC001_Board_Init();
    snprintf(reply, reply_size, "OK");
  }
  else if (XC001_StrCaseCmp(cmd, "*STB?") == 0)
  {
    snprintf(reply, reply_size, "0");
  }
  else if (XC001_StrCaseCmp(cmd, "*OPC?") == 0)
  {
    snprintf(reply, reply_size, "1");
  }
  else if (XC001_StrCaseCmp(cmd, "*WAI") == 0)
  {
    snprintf(reply, reply_size, "OK");
  }
  else if (XC001_StrCaseCmp(cmd, "SYST:ERR?") == 0 ||
           XC001_StrCaseCmp(cmd, "SYST:ERR:NEXT?") == 0 ||
           XC001_StrCaseCmp(cmd, "SYSTEM:ERROR?") == 0 ||
           XC001_StrCaseCmp(cmd, "SYSTEM:ERROR:NEXT?") == 0)
  {
    snprintf(reply, reply_size, "0,\"No error\"");
  }
  else if (XC001_StrCaseCmp(cmd, "SYST:ERR:COUN?") == 0 ||
           XC001_StrCaseCmp(cmd, "SYST:ERR:COUNT?") == 0 ||
           XC001_StrCaseCmp(cmd, "SYSTEM:ERROR:COUNT?") == 0)
  {
    snprintf(reply, reply_size, "0");
  }
  else if (XC001_StrCaseCmp(cmd, "SYST:HELP?") == 0)
  {
    snprintf(reply, reply_size,
             "*IDN?,PING?,*RST,*CLS,*STB?,*OPC?,*WAI,SYST:ERR?,SYST:ERR:COUN?,SYST:CLOCK?,STAT?,NET:STAT?,NET:DIAG?,NET:PHY?,NET:IP?,NET:MASK?,NET:GATE?,NET:PORT?,NET:SERV ON,DIG:LIST?,DIG:OUTP pin,val,DIG:OUTP? pin,ROUT:CHAN?,ROUT:CHAN n,ROUT:CLOS (@n),ROUT:STAT?,CAN:STAT?,CAN:SEND id,hex,CAN:RX?,MEAS:POW? node,freq_kHz[,sample_ms,mode,threshold,timeout_ms,comp],RS485:STAT?,RS485:SEND text,RS485:RX?,SPI:STAT?,SPI:TRAN? hex");
  }
  else if (XC001_StrCaseCmp(cmd, "SYST:CLOCK?") == 0)
  {
    snprintf(reply, reply_size, "CPU=%lu,HCLK=%lu,PCLK1=%lu,PCLK2=%lu,PROFILE=%s",
             (unsigned long)SystemCoreClock,
             (unsigned long)HAL_RCC_GetHCLKFreq(),
             (unsigned long)HAL_RCC_GetPCLK1Freq(),
             (unsigned long)HAL_RCC_GetPCLK2Freq(),
             (XC001_LOW_POWER_PROFILE != 0U) ? "LOW_POWER" : "PERFORMANCE");
  }
  else if (XC001_StrCaseCmp(cmd, "STAT?") == 0)
  {
    char can[112], rs[80], spi[80], net[120], uart[40];
    XC001_CAN_Status(can, sizeof(can));
    XC001_RS485_Status(rs, sizeof(rs));
    XC001_SPIBus_Status(spi, sizeof(spi));
    reply_net_config(net, sizeof(net));
    snprintf(uart, sizeof(uart), "UART7:OVF=%lu", (unsigned long)XC001_Console_GetRxOverflow());
    snprintf(reply, reply_size, "%s;%s;%s;%s;%s", net, can, rs, spi, uart);
  }
  else if (XC001_StrCaseCmp(cmd, "NET:IP?") == 0)
  {
    XC001_Config_FormatIp(XC001_NetConfig.ip, reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "NET:IP ", 7) == 0)
  {
    uint8_t ip[4];
    if (XC001_Config_ParseIp(XC001_SkipSpace(cmd + 7), ip) &&
        save_network_candidate(ip, XC001_NetConfig.netmask, XC001_NetConfig.gateway,
                               XC001_NetConfig.udp_port))
    {
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"Invalid IP\"");
    }
  }
  else if (XC001_StrCaseCmp(cmd, "NET:PORT?") == 0)
  {
    snprintf(reply, reply_size, "%u", XC001_NetConfig.udp_port);
  }
  else if (XC001_StrCaseCmp(cmd, "NET:MASK?") == 0)
  {
    XC001_Config_FormatIp(XC001_NetConfig.netmask, reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "NET:MASK ", 9) == 0)
  {
    uint8_t mask[4];
    if (XC001_Config_ParseIp(XC001_SkipSpace(cmd + 9), mask) &&
        save_network_candidate(XC001_NetConfig.ip, mask, XC001_NetConfig.gateway,
                               XC001_NetConfig.udp_port))
    {
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"Invalid netmask\"");
    }
  }
  else if (XC001_StrCaseCmp(cmd, "NET:GATE?") == 0)
  {
    XC001_Config_FormatIp(XC001_NetConfig.gateway, reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "NET:GATE ", 9) == 0)
  {
    uint8_t gw[4];
    if (XC001_Config_ParseIp(XC001_SkipSpace(cmd + 9), gw) &&
        save_network_candidate(XC001_NetConfig.ip, XC001_NetConfig.netmask, gw,
                               XC001_NetConfig.udp_port))
    {
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"Invalid gateway\"");
    }
  }
  else if (XC001_StrNCaseCmp(cmd, "NET:PORT ", 9) == 0)
  {
    uint16_t port;
    if (XC001_ParseU16(XC001_SkipSpace(cmd + 9), &port) &&
        save_network_candidate(XC001_NetConfig.ip, XC001_NetConfig.netmask,
                               XC001_NetConfig.gateway, port))
    {
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"Invalid port\"");
    }
  }
  else if (XC001_StrCaseCmp(cmd, "NET:STAT?") == 0)
  {
    reply_net_config(reply, reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "NET:DIAG?") == 0 || XC001_StrCaseCmp(cmd, "ND?") == 0)
  {
    XC001_Net_Diag(reply, (uint32_t)reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "NET:PHY?") == 0)
  {
    XC001_Net_PhyDiag(reply, (uint32_t)reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "NET:SERV ON") == 0 || XC001_StrCaseCmp(cmd, "NET:SERV:ON") == 0)
  {
    XC001_Net_RequestStartServices();
    snprintf(reply, reply_size, "OK,STARTING");
  }
  else if (XC001_StrCaseCmp(cmd, "NET:MAC?") == 0)
  {
    XC001_Net_FormatMac(reply, reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "DIG:LIST?") == 0)
  {
    XC001_Board_GpioList(reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "DIG:OUTP? ", 10) == 0)
  {
    uint8_t value;
    const char *pin = XC001_SkipSpace(cmd + 10);
    if (XC001_Board_ReadGpio(pin, &value))
    {
      snprintf(reply, reply_size, "%u", value);
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-224,\"Unknown GPIO\"");
    }
  }
  else if (XC001_StrNCaseCmp(cmd, "DIG:OUTP ", 9) == 0)
  {
    char *arg = cmd + 9;
    char *comma = strchr(arg, ',');
    if (comma != 0)
    {
      uint8_t ok;
      *comma = '\0';
      arg = XC001_Trim(arg);
      comma = XC001_Trim(comma + 1);
      if (XC001_StrCaseCmp(comma, "T") == 0)
      {
        ok = XC001_Board_WriteGpio(arg, 0U, 1U);
      }
      else if (strcmp(comma, "0") == 0 || strcmp(comma, "1") == 0)
      {
        ok = XC001_Board_WriteGpio(arg, (comma[0] == '1') ? 1U : 0U, 0U);
      }
      else
      {
        snprintf(reply, reply_size, "ERR,-222,\"Value must be 0, 1 or T\"");
        ok = 2U;
      }
      if (ok != 2U)
      {
        snprintf(reply, reply_size, "%s", ok ? "OK" : "ERR,-224,\"Unknown GPIO\"");
      }
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-109,\"Missing parameter\"");
    }
  }
  else if (XC001_StrCaseCmp(cmd, "ROUT:CHAN?") == 0 ||
           XC001_StrCaseCmp(cmd, "ROUTE:CHANNEL?") == 0)
  {
    snprintf(reply, reply_size, "%u", XC001_Board_GetRfChannel());
  }
  else if (XC001_StrCaseCmp(cmd, "ROUT:STAT?") == 0 ||
           XC001_StrCaseCmp(cmd, "ROUTE:STATE?") == 0)
  {
    XC001_Board_RfSwitchStatus(reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "ROUT:CHAN ", 10) == 0 ||
           XC001_StrNCaseCmp(cmd, "ROUTE:CHANNEL ", 14) == 0)
  {
    uint8_t channel;
    const char *arg = (XC001_StrNCaseCmp(cmd, "ROUT:CHAN ", 10) == 0) ? (cmd + 10) : (cmd + 14);
    if (parse_rf_channel(arg, &channel) && XC001_Board_SetRfChannel(channel))
    {
      snprintf(reply, reply_size, "OK,CHAN=%u", channel);
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"Channel must be 1 to 8\"");
    }
  }
  else if (XC001_StrNCaseCmp(cmd, "ROUT:CLOS ", 10) == 0 ||
           XC001_StrNCaseCmp(cmd, "ROUTE:CLOSE ", 12) == 0)
  {
    uint8_t channel;
    const char *arg = (XC001_StrNCaseCmp(cmd, "ROUT:CLOS ", 10) == 0) ? (cmd + 10) : (cmd + 12);
    if (parse_rf_channel(arg, &channel) && XC001_Board_SetRfChannel(channel))
    {
      snprintf(reply, reply_size, "OK,CHAN=%u", channel);
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"Channel must be 1 to 8\"");
    }
  }
  else if (XC001_StrCaseCmp(cmd, "CAN:STAT?") == 0)
  {
    XC001_CAN_Status(reply, reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "CAN:RX?") == 0)
  {
    XC001_CAN_LastRx(reply, reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "CAN:POW?") == 0 ||
           XC001_StrCaseCmp(cmd, "MEAS:POW?") == 0 ||
           XC001_StrCaseCmp(cmd, "MEASURE:POWER?") == 0)
  {
    snprintf(reply, reply_size, "ERR,-109,\"Missing node and frequency\"");
  }
  else if (XC001_StrNCaseCmp(cmd, "CAN:POW? ", 9U) == 0 ||
           XC001_StrNCaseCmp(cmd, "MEAS:POW? ", 10U) == 0 ||
           XC001_StrNCaseCmp(cmd, "MEASURE:POWER? ", 15U) == 0)
  {
    char *arg;
    uint8_t node_id, sample_time_ms, sample_mode, antenna_compensation;
    uint16_t trigger_threshold, trigger_timeout_ms;
    uint32_t frequency_khz;
    XC001_CAN_RxPower power;
    XC001_CAN_QueryResult query_result;

    if (XC001_StrNCaseCmp(cmd, "CAN:POW? ", 9U) == 0)
    {
      arg = cmd + 9;
    }
    else if (XC001_StrNCaseCmp(cmd, "MEAS:POW? ", 10U) == 0)
    {
      arg = cmd + 10;
    }
    else
    {
      arg = cmd + 15;
    }

    if (!parse_power_query(arg, &node_id, &frequency_khz, &sample_time_ms,
                           &sample_mode, &trigger_threshold, &trigger_timeout_ms,
                           &antenna_compensation))
    {
      snprintf(reply, reply_size,
               "ERR,-222,\"Expected node,freq_kHz[,sample_ms,mode,threshold,timeout_ms,comp]\"");
    }
    else
    {
      query_result = XC001_CAN_QueryRxPower(node_id, frequency_khz, sample_time_ms,
                                             sample_mode, trigger_threshold,
                                             trigger_timeout_ms, antenna_compensation,
                                             &power);
      if (query_result == XC001_CAN_QUERY_OK && power.device_status == 0U)
      {
        char h_power[16], v_power[16];
        format_cdbm(power.h_power_cdbm, h_power, sizeof(h_power));
        format_cdbm(power.v_power_cdbm, v_power, sizeof(v_power));
        snprintf(reply, reply_size,
                 "NODE=%u,FREQ=%lu,H=%sdBm,V=%sdBm,MODE=%u,STATE=%u",
                 power.node_id, (unsigned long)power.frequency_khz,
                 h_power, v_power, power.sample_mode, power.sample_state);
      }
      else if (query_result == XC001_CAN_QUERY_OK)
      {
        snprintf(reply, reply_size, "ERR,-222,\"A1 status %u\"", power.device_status);
      }
      else if (query_result == XC001_CAN_QUERY_TIMEOUT)
      {
        snprintf(reply, reply_size, "ERR,-410,\"CAN power response timeout\"");
      }
      else if (query_result == XC001_CAN_QUERY_BUSY)
      {
        snprintf(reply, reply_size, "ERR,-221,\"CAN power query busy\"");
      }
      else if (query_result == XC001_CAN_QUERY_NOT_READY)
      {
        snprintf(reply, reply_size, "ERR,-300,\"CAN not ready\"");
      }
      else
      {
        snprintf(reply, reply_size, "ERR,-222,\"CAN power request failed\"");
      }
    }
  }
  else if (XC001_StrNCaseCmp(cmd, "CAN:SEND ", 9) == 0)
  {
    char *arg = cmd + 9;
    char *comma = strchr(arg, ',');
    uint8_t data[64], len = 0U;
    uint32_t id;
    if (comma == 0)
    {
      snprintf(reply, reply_size, "ERR,-109,\"Missing parameter\"");
    }
    else
    {
      *comma = '\0';
      if (XC001_ParseU32(XC001_Trim(arg), 0x1FFFFFFFUL, &id) &&
          XC001_ParseHexBytes(comma + 1, data, sizeof(data), &len) && XC001_CAN_Send(id, data, len))
      {
        snprintf(reply, reply_size, "OK");
      }
      else
      {
        snprintf(reply, reply_size, "ERR,-222,\"CAN send failed\"");
      }
    }
  }
  else if (XC001_StrCaseCmp(cmd, "RS485:STAT?") == 0)
  {
    XC001_RS485_Status(reply, reply_size);
  }
  else if (XC001_StrCaseCmp(cmd, "RS485:RX?") == 0)
  {
    XC001_RS485_LastRx(reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "RS485:SEND ", 11) == 0)
  {
    snprintf(reply, reply_size, "%s", XC001_RS485_SendText(XC001_SkipSpace(cmd + 11)) ? "OK" : "ERR,-222,\"RS485 send failed\"");
  }
  else if (XC001_StrCaseCmp(cmd, "SPI:STAT?") == 0)
  {
    XC001_SPIBus_Status(reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "SPI:TRAN? ", 10) == 0)
  {
    uint8_t tx[32], rx[32], len = 0U;
    if (XC001_ParseHexBytes(cmd + 10, tx, sizeof(tx), &len) && XC001_SPIBus_Transfer(tx, rx, len))
    {
      XC001_FormatHexBytes(rx, len, reply, reply_size);
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-222,\"SPI transfer failed\"");
    }
  }
  else
  {
    snprintf(reply, reply_size, "ERR,-113,\"Undefined header\"");
  }

  if (mutex_locked != 0U)
  {
    (void)osMutexRelease(s_scpi_mutex);
  }
  indicate_scpi_error(reply);
}
