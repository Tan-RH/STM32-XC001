#include "app.h"

#include "board_led.h"
#include "console.h"
#include "lmx2592.h"
#include "status_display.h"
#include "control_panel.h"

void App_Init(void)
{
  BoardLed_Init();
  LMX2592_Init();
  ControlPanel_Init();
  StatusDisplay_Init();
  Console_Init();
}

void App_Process(void)
{
  BoardLed_Process();
  Console_Process();
  ControlPanel_Process();
  StatusDisplay_Process();
}
