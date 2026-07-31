#include "control_panel.h"

#include "main.h"

#define KEY_SCAN_MS       10U
#define KEY_DEBOUNCE_MS   30U
#define KEY_REPEAT_START  600U
#define KEY_REPEAT_MS     120U
#define MIN_FREQUENCY_HZ  UINT64_C(20000000)
#define MAX_FREQUENCY_HZ  UINT64_C(9800000000)

typedef enum { KEY_NONE, KEY_OK, KEY_RIGHT, KEY_LEFT, KEY_DOWN, KEY_UP } Key;

static const uint32_t steps[] = {1000U, 10000U, 100000U, 1000000U,
                                 10000000U, 100000000U};
static ControlPanel_Field field;
static uint64_t edit_frequency;
static uint8_t edit_power;
static uint8_t step_index = 3U;
static LMX2592_Output edit_output;
static bool frequency_dirty;
static bool power_dirty;

static Key read_key(void)
{
  if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET) return KEY_OK;
  if (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET) return KEY_RIGHT;
  if (HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET) return KEY_LEFT;
  if (HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin) == GPIO_PIN_RESET) return KEY_DOWN;
  if (HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin) == GPIO_PIN_RESET) return KEY_UP;
  return KEY_NONE;
}

static uint8_t next_power(uint8_t value)
{
  if (value < 31U) return value + 1U;
  if (value == 31U) return 48U;
  if (value < 63U) return value + 1U;
  return 63U;
}

static uint8_t previous_power(uint8_t value)
{
  if (value > 48U) return value - 1U;
  if (value == 48U) return 31U;
  if (value > 0U) return value - 1U;
  return 0U;
}

static void modify(bool increase)
{
  switch (field)
  {
    case CONTROL_FIELD_FREQUENCY:
      if (increase)
      {
        uint64_t room = MAX_FREQUENCY_HZ - edit_frequency;
        edit_frequency += room < steps[step_index] ? room : steps[step_index];
      }
      else
      {
        uint64_t room = edit_frequency - MIN_FREQUENCY_HZ;
        edit_frequency -= room < steps[step_index] ? room : steps[step_index];
      }
      frequency_dirty = true;
      break;
    case CONTROL_FIELD_STEP:
      if (increase && step_index + 1U < sizeof(steps)/sizeof(steps[0])) ++step_index;
      if (!increase && step_index > 0U) --step_index;
      break;
    case CONTROL_FIELD_OUTPUT:
      edit_output = (edit_output == LMX2592_OUTPUT_A) ? LMX2592_OUTPUT_B : LMX2592_OUTPUT_A;
      edit_power = LMX2592_GetPower(edit_output);
      power_dirty = false;
      break;
    case CONTROL_FIELD_POWER:
      edit_power = increase ? next_power(edit_power) : previous_power(edit_power);
      power_dirty = true;
      break;
    default: break;
  }
}

static void apply(void)
{
  if (field == CONTROL_FIELD_OUTPUT && !frequency_dirty && !power_dirty)
  {
    LMX2592_SetOutput(edit_output, !LMX2592_IsOutputEnabled(edit_output));
    return;
  }
  if (LMX2592_SetFrequency(edit_frequency) == LMX2592_OK &&
      LMX2592_SetPower(edit_output, edit_power) == LMX2592_OK)
  {
    LMX2592_SetOutput(edit_output, true);
    frequency_dirty = false;
    power_dirty = false;
  }
}

static void handle_key(Key key)
{
  if (key == KEY_OK) apply();
  else if (key == KEY_RIGHT) field = (ControlPanel_Field)((field + 1U) % CONTROL_FIELD_COUNT);
  else if (key == KEY_LEFT) field = (ControlPanel_Field)((field + CONTROL_FIELD_COUNT - 1U) % CONTROL_FIELD_COUNT);
  else if (key == KEY_UP) modify(true);
  else if (key == KEY_DOWN) modify(false);
}

void ControlPanel_Init(void)
{
  edit_frequency = LMX2592_GetFrequency();
  edit_output = LMX2592_OUTPUT_A;
  edit_power = LMX2592_GetPower(edit_output);
}

void ControlPanel_Process(void)
{
  static uint32_t last_scan, raw_since, pressed_since, last_repeat;
  static Key raw, stable;
  uint32_t now = HAL_GetTick();
  Key sample;

  if ((uint32_t)(now - last_scan) < KEY_SCAN_MS) return;
  last_scan = now;
  sample = read_key();
  if (sample != raw) { raw = sample; raw_since = now; }
  if (raw != stable && (uint32_t)(now - raw_since) >= KEY_DEBOUNCE_MS)
  {
    stable = raw;
    if (stable != KEY_NONE)
    {
      pressed_since = now;
      last_repeat = now;
      handle_key(stable);
    }
  }
  else if ((stable == KEY_UP || stable == KEY_DOWN) &&
           (uint32_t)(now - pressed_since) >= KEY_REPEAT_START &&
           (uint32_t)(now - last_repeat) >= KEY_REPEAT_MS)
  {
    last_repeat = now;
    handle_key(stable);
  }

  if (!frequency_dirty) edit_frequency = LMX2592_GetFrequency();
  if (!power_dirty) edit_power = LMX2592_GetPower(edit_output);
}

ControlPanel_Field ControlPanel_GetField(void) { return field; }
uint64_t ControlPanel_GetFrequency(void) { return edit_frequency; }
uint32_t ControlPanel_GetStep(void) { return steps[step_index]; }
LMX2592_Output ControlPanel_GetOutput(void) { return edit_output; }
uint8_t ControlPanel_GetPower(void) { return edit_power; }
bool ControlPanel_HasPendingChanges(void) { return frequency_dirty || power_dirty; }
