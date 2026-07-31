#ifndef CONTROL_PANEL_H
#define CONTROL_PANEL_H

#include "lmx2592.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  CONTROL_FIELD_FREQUENCY = 0,
  CONTROL_FIELD_STEP,
  CONTROL_FIELD_OUTPUT,
  CONTROL_FIELD_POWER,
  CONTROL_FIELD_COUNT
} ControlPanel_Field;

void ControlPanel_Init(void);
void ControlPanel_Process(void);
ControlPanel_Field ControlPanel_GetField(void);
uint64_t ControlPanel_GetFrequency(void);
uint32_t ControlPanel_GetStep(void);
LMX2592_Output ControlPanel_GetOutput(void);
uint8_t ControlPanel_GetPower(void);
bool ControlPanel_HasPendingChanges(void);

#endif
