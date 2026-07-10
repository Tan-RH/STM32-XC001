#include "xc001_scpi.h"
#include "xc001_config.h"
#include "xc001_utils.h"
#include "xc001_board.h"
#include "xc001_can.h"
#include "xc001_rs485.h"
#include "xc001_spi_bus.h"
#include "xc001_net.h"
#include "xc001_storage.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void XC001_SCPI_Init(void)
{
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

  if (reply == 0 || reply_size == 0U)
  {
    return;
  }
  reply[0] = '\0';
  if (command == 0)
  {
    snprintf(reply, reply_size, "ERR,-100,\"Empty command\"");
    return;
  }

  snprintf(line, sizeof(line), "%s", command);
  cmd = XC001_Trim(line);
  if (*cmd == '\0')
  {
    snprintf(reply, reply_size, "ERR,-100,\"Empty command\"");
    return;
  }

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
  else if (XC001_StrCaseCmp(cmd, "SYST:ERR?") == 0)
  {
    snprintf(reply, reply_size, "0,\"No error\"");
  }
  else if (XC001_StrCaseCmp(cmd, "SYST:HELP?") == 0)
  {
    snprintf(reply, reply_size,
             "*IDN?,PING?,*RST,*CLS,SYST:ERR?,STAT?,NET:STAT?,NET:DIAG?,NET:PHY?,NET:IP?,NET:IP a.b.c.d,NET:MASK?,NET:MASK a.b.c.d,NET:GATE?,NET:GATE a.b.c.d,NET:PORT?,NET:PORT n,NET:SERV ON,DIG:LIST?,DIG:OUTP pin,val,DIG:OUTP? pin,CAN:STAT?,CAN:SEND id,hex,CAN:RX?,RS485:STAT?,RS485:SEND text,RS485:RX?,SPI:STAT?,SPI:TRAN? hex");
  }
  else if (XC001_StrCaseCmp(cmd, "STAT?") == 0)
  {
    char can[80], rs[80], spi[80], net[120];
    XC001_CAN_Status(can, sizeof(can));
    XC001_RS485_Status(rs, sizeof(rs));
    XC001_SPIBus_Status(spi, sizeof(spi));
    reply_net_config(net, sizeof(net));
    snprintf(reply, reply_size, "%s;%s;%s;%s", net, can, rs, spi);
  }
  else if (XC001_StrCaseCmp(cmd, "NET:IP?") == 0)
  {
    XC001_Config_FormatIp(XC001_NetConfig.ip, reply, reply_size);
  }
  else if (XC001_StrNCaseCmp(cmd, "NET:IP ", 7) == 0)
  {
    uint8_t ip[4];
    XC001_NetworkConfig active_cfg = XC001_NetConfig;
    if (XC001_Config_ParseIp(XC001_SkipSpace(cmd + 7), ip) &&
        XC001_Config_SetNetwork(ip, XC001_NetConfig.udp_port) &&
        XC001_Storage_Save(&XC001_NetConfig))
    {
      XC001_NetConfig = active_cfg;
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      XC001_NetConfig = active_cfg;
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
    XC001_NetworkConfig active_cfg = XC001_NetConfig;
    if (XC001_Config_ParseIp(XC001_SkipSpace(cmd + 9), mask) &&
        XC001_Config_SetNetworkFull(XC001_NetConfig.ip, mask, XC001_NetConfig.gateway, XC001_NetConfig.udp_port) &&
        XC001_Storage_Save(&XC001_NetConfig))
    {
      XC001_NetConfig = active_cfg;
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      XC001_NetConfig = active_cfg;
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
    XC001_NetworkConfig active_cfg = XC001_NetConfig;
    if (XC001_Config_ParseIp(XC001_SkipSpace(cmd + 9), gw) &&
        XC001_Config_SetNetworkFull(XC001_NetConfig.ip, XC001_NetConfig.netmask, gw, XC001_NetConfig.udp_port) &&
        XC001_Storage_Save(&XC001_NetConfig))
    {
      XC001_NetConfig = active_cfg;
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      XC001_NetConfig = active_cfg;
      snprintf(reply, reply_size, "ERR,-222,\"Invalid gateway\"");
    }
  }
  else if (XC001_StrNCaseCmp(cmd, "NET:PORT ", 9) == 0)
  {
    uint16_t port;
    XC001_NetworkConfig active_cfg = XC001_NetConfig;
    if (XC001_ParseU16(XC001_SkipSpace(cmd + 9), &port) &&
        XC001_Config_SetNetwork(XC001_NetConfig.ip, port) &&
        XC001_Storage_Save(&XC001_NetConfig))
    {
      XC001_NetConfig = active_cfg;
      snprintf(reply, reply_size, "OK,SAVED,REBOOT_REQUIRED");
    }
    else
    {
      XC001_NetConfig = active_cfg;
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
    snprintf(reply, reply_size, "00:80:E1:00:00:00");
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
      ok = (toupper((unsigned char)comma[0]) == 'T') ?
           XC001_Board_WriteGpio(arg, 0U, 1U) :
           XC001_Board_WriteGpio(arg, (comma[0] == '1') ? 1U : 0U, 0U);
      snprintf(reply, reply_size, "%s", ok ? "OK" : "ERR,-224,\"Unknown GPIO\"");
    }
    else
    {
      snprintf(reply, reply_size, "ERR,-109,\"Missing parameter\"");
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
  else if (XC001_StrNCaseCmp(cmd, "CAN:SEND ", 9) == 0)
  {
    char *arg = cmd + 9;
    char *comma = strchr(arg, ',');
    uint8_t data[8], len = 0U;
    uint32_t id;
    if (comma == 0)
    {
      snprintf(reply, reply_size, "ERR,-109,\"Missing parameter\"");
    }
    else
    {
      *comma = '\0';
      id = strtoul(XC001_Trim(arg), 0, 0);
      if (XC001_ParseHexBytes(comma + 1, data, sizeof(data), &len) && XC001_CAN_Send(id, data, len))
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
}
