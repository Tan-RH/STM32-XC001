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
  char key[24];
  char msg[220];
  uint32_t reset_flags = XC001_Board_GetAndClearResetFlags();

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  XC001_Net_FormatRemoteKey(key, sizeof(key));
  snprintf(msg, sizeof(msg),
           "[BOOT] HTTP and UDP SCPI services started.\r\n"
           "[BOOT] Try http://%s/, ND? or NET:DIAG?.\r\n"
           "[BOOT] Remote write key: %s\r\n"
           "[BOOT] Reset flags: 0x%08lX\r\nXC001> ",
           ip, key, (unsigned long)reset_flags);
  XC001_Console_WriteRaw(msg);
}

void XC001_APP_PreLwipInit(void)
{
  uint8_t reset_net_cfg;

  XC001_Config_LoadDefaults();
  XC001_Storage_Init();
  XC001_SCPI_Init();
  reset_net_cfg = XC001_Board_IsEthResetPressed();
  if (reset_net_cfg != 0U)
  {
    if (XC001_Storage_Reset() == 0U)
    {
      XC001_Board_SetStatusOk(0U);
    }
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
  XC001_Board_WatchdogInit();
  app_print_network_url();
}

void XC001_APP_Task(void)
{
  XC001_RS485_Task();
  XC001_CAN_Task();
  XC001_Net_Task();
  XC001_Board_Task();
  XC001_Board_WatchdogRefresh();
}
