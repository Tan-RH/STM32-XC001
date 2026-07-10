#include "xc001_app.h"
#include "xc001_config.h"
#include "xc001_storage.h"
#include "xc001_board.h"
#include "xc001_scpi.h"
#include "xc001_console.h"
#include "xc001_rs485.h"
#include "xc001_spi_bus.h"
#include "xc001_can.h"
#include "xc001_net.h"
#include <stdio.h>

static void app_print_network_url(void)
{
  char ip[20];
  char msg[120];

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  snprintf(msg, sizeof(msg), "[BOOT] HTTP and UDP SCPI services started.\r\n[BOOT] Try http://%s/, ND? or NET:DIAG?.\r\nXC001> ", ip);
  XC001_Console_WriteRaw(msg);
}

void XC001_APP_PreLwipInit(void)
{
  uint8_t reset_net_cfg;

  XC001_Config_LoadDefaults();
  reset_net_cfg = XC001_Board_IsEthResetPressed();
  if (reset_net_cfg != 0U)
  {
    (void)XC001_Storage_Reset();
  }
  else
  {
    (void)XC001_Storage_Load(&XC001_NetConfig);
  }
  XC001_Board_Init();
  XC001_Console_Init();
  if (reset_net_cfg != 0U)
  {
    XC001_Console_WriteRaw("\r\n[BOOT] ETH_NRST held LOW: network config reset to default.\r\n");
  }
  XC001_Console_WriteRaw("\r\n[BOOT] ETH_NRST is PE15, active-low. Reset pulse LOW 100ms then HIGH.\r\n");
  XC001_Board_PhyResetPulse();
  XC001_Console_WriteRaw("[BOOT] Starting LwIP init...\r\n");
}

void XC001_APP_Init(void)
{
  XC001_SCPI_Init();
  XC001_Console_Init();
  XC001_Console_WriteRaw("[BOOT] LwIP init done. Starting services...\r\n");
  XC001_RS485_Init();
  XC001_SPIBus_Init();
  XC001_CAN_Init();
  XC001_Net_Init();
  app_print_network_url();
}

void XC001_APP_Task(void)
{
  XC001_RS485_Task();
  XC001_CAN_Task();
  XC001_Net_Task();
  XC001_Board_Task();
}
