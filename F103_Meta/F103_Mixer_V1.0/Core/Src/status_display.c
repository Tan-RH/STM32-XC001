#include "status_display.h"

#include "app_version.h"
#include "control_panel.h"
#include "lcd_st7735.h"
#include "lmx2592.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define DISPLAY_POLL_MS 50U
#define LINE_COUNT      7U

typedef struct
{
  char label[8];
  char value[22];
  uint16_t color;
  bool valid;
} LineCache;

static LineCache cache[LINE_COUNT];

static void value_line(uint8_t index, uint8_t y, bool selected, const char *label,
                       const char *value, uint16_t color)
{
  char display_label[8];
  (void)snprintf(display_label, sizeof(display_label), "%c%s", selected ? '>' : ' ', label);
  if (cache[index].valid && cache[index].color == color &&
      strcmp(cache[index].label, display_label) == 0 &&
      strcmp(cache[index].value, value) == 0) return;

  LCD_FillRect(0U, y, 128U, 12U, LCD_COLOR_BLACK);
  LCD_DrawString(2U, y + 2U, display_label,
                 selected ? LCD_COLOR_BLUE : LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U);
  LCD_DrawString(43U, y + 2U, value, color, LCD_COLOR_BLACK, 1U);
  (void)snprintf(cache[index].label, sizeof(cache[index].label), "%s", display_label);
  (void)snprintf(cache[index].value, sizeof(cache[index].value), "%s", value);
  cache[index].color = color;
  cache[index].valid = true;
}

static void footer(bool pending, ControlPanel_Field field)
{
  const char *text = pending ? "EDIT  K1 APPLY" :
                     (field == CONTROL_FIELD_OUTPUT ? "K1 OUT ON/OFF" : "K2/K3 SEL K4/K5 ADJ");
  uint16_t color = pending ? LCD_COLOR_RED : LCD_COLOR_GRAY;
  if (cache[6].valid && cache[6].color == color && strcmp(cache[6].value, text) == 0) return;
  LCD_FillRect(0U, 146U, 128U, 12U, LCD_COLOR_BLACK);
  LCD_DrawString(2U, 148U, text, color, LCD_COLOR_BLACK, 1U);
  (void)snprintf(cache[6].value, sizeof(cache[6].value), "%s", text);
  cache[6].color = color;
  cache[6].valid = true;
}

void StatusDisplay_Init(void)
{
  memset(cache, 0, sizeof(cache));
  LCD_Init();
  LCD_FillRect(0U, 0U, 128U, 22U, LCD_COLOR_BLUE);
  LCD_DrawString(7U, 3U, "GENERAL TEST", LCD_COLOR_WHITE, LCD_COLOR_BLUE, 1U);
  LCD_DrawString(7U, 12U, "LMX2592 " APP_VERSION_STRING, LCD_COLOR_WHITE, LCD_COLOR_BLUE, 1U);
  StatusDisplay_Process();
}

void StatusDisplay_Process(void)
{
  static uint32_t previous;
  uint32_t now = HAL_GetTick();
  ControlPanel_Field field;
  LMX2592_Output output;
  char text[22];
  uint64_t frequency;
  uint32_t step;
  static bool lock_raw;
  static bool lock_stable;
  static uint8_t lock_samples;
  bool lock_sample;

  if ((uint32_t)(now - previous) < DISPLAY_POLL_MS) return;
  previous = now;
  field = ControlPanel_GetField();
  output = ControlPanel_GetOutput();
  frequency = ControlPanel_GetFrequency();
  step = ControlPanel_GetStep();
  lock_sample = LMX2592_IsLocked();
  if (lock_sample != lock_raw)
  {
    lock_raw = lock_sample;
    lock_samples = 1U;
  }
  else if (lock_samples < 3U)
  {
    ++lock_samples;
    if (lock_samples == 3U) lock_stable = lock_raw;
  }

  (void)snprintf(text, sizeof(text), "%lu.%03lu MHZ",
                 (unsigned long)(frequency / 1000000U),
                 (unsigned long)((frequency % 1000000U) / 1000U));
  value_line(0U, 28U, field == CONTROL_FIELD_FREQUENCY, "FREQ", text,
             ControlPanel_HasPendingChanges() ? LCD_COLOR_RED : LCD_COLOR_WHITE);

  if (step >= 1000000U)
    (void)snprintf(text, sizeof(text), "%lu MHZ", (unsigned long)(step / 1000000U));
  else
    (void)snprintf(text, sizeof(text), "%lu KHZ", (unsigned long)(step / 1000U));
  value_line(1U, 44U, field == CONTROL_FIELD_STEP, "STEP", text, LCD_COLOR_WHITE);

  (void)snprintf(text, sizeof(text), "TX%c %s", output == LMX2592_OUTPUT_A ? 'A' : 'B',
                 LMX2592_IsOutputEnabled(output) ? "ON" : "OFF");
  value_line(2U, 64U, field == CONTROL_FIELD_OUTPUT, "OUT", text,
             LMX2592_IsOutputEnabled(output) ? LCD_COLOR_GREEN : LCD_COLOR_GRAY);

  (void)snprintf(text, sizeof(text), "CODE %u", ControlPanel_GetPower());
  value_line(3U, 80U, field == CONTROL_FIELD_POWER, "POWER", text,
             ControlPanel_HasPendingChanges() ? LCD_COLOR_RED : LCD_COLOR_WHITE);

  value_line(4U, 104U, false, "CE", LMX2592_IsChipEnabled() ? "ON" : "OFF",
             LMX2592_IsChipEnabled() ? LCD_COLOR_GREEN : LCD_COLOR_RED);
  value_line(5U, 120U, false, "PLL", lock_stable ? "LOCKED" : "UNLOCKED",
             lock_stable ? LCD_COLOR_GREEN : LCD_COLOR_RED);
  footer(ControlPanel_HasPendingChanges(), field);
}
