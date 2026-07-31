#include "lmx2592.h"

#include "main.h"

#define LMX2592_MIN_FREQUENCY_HZ  UINT64_C(20000000)
#define LMX2592_MAX_FREQUENCY_HZ  UINT64_C(9800000000)
#define LMX2592_MIN_REFERENCE_HZ  UINT32_C(5000000)
#define LMX2592_MAX_REFERENCE_HZ  UINT32_C(200000000)

typedef struct
{
  uint16_t divide;
  uint8_t seg1;
  uint8_t seg2;
  uint8_t seg3;
} ChannelDivider;

static const ChannelDivider channel_dividers[] = {
  {2, 2, 1, 1}, {3, 3, 1, 1}, {4, 2, 2, 1}, {6, 3, 2, 1},
  {8, 2, 4, 1}, {12, 3, 4, 1}, {16, 2, 8, 1}, {24, 3, 8, 1},
  {32, 2, 8, 2}, {36, 3, 6, 2}, {48, 3, 8, 2}, {64, 2, 8, 4},
  {96, 2, 8, 6}, {128, 2, 8, 8}, {192, 3, 8, 8}
};

/* Register-map reset values from LMX2592 data sheet, Figure 22. */
static const struct
{
  uint8_t address;
  uint16_t value;
} register_defaults[] = {
  {64, 0x007F}, {62, 0x0000}, {61, 0x0001}, {59, 0x0000},
  {48, 0x03FC}, {47, 0x00C0}, {46, 0x0FA3}, {45, 0x0000},
  {44, 0x0000}, {43, 0x0000}, {42, 0x0000}, {41, 0x03E8},
  {40, 0x0000}, {39, 0x8204}, {38, 0x0036}, {37, 0x4000},
  {36, 0x0411}, {35, 0x021D}, {34, 0xC3EA}, {33, 0x2A0A},
  {32, 0x210A}, {31, 0x0401}, {30, 0x0034}, {29, 0x0084},
  {28, 0x2924}, {25, 0x0000}, {24, 0x0529}, {23, 0x8842},
  {22, 0x4600}, {20, 0x012C}, {19, 0x0965}, {14, 0x018D},
  {13, 0x4000}, {12, 0x7001}, {11, 0x0018}, {10, 0x00D8},
  {9, 0x0202}, {8, 0x1084}, {7, 0x2852}, {4, 0x1943},
  {2, 0x0500}, {1, 0x080B}
};

static uint16_t registers[65];
static uint32_t reference_hz = 50000000U;
static uint64_t frequency_hz = UINT64_C(2400000000);
static uint8_t output_power[2] = {15U, 15U};
static bool output_enabled[2] = {true, false};
static bool chip_enabled;

static void short_delay(void)
{
  for (volatile uint32_t i = 0; i < 12U; ++i)
  {
    __NOP();
  }
}

static uint32_t gcd_u32(uint32_t a, uint32_t b)
{
  while (b != 0U)
  {
    uint32_t remainder = a % b;
    a = b;
    b = remainder;
  }
  return a;
}

static bool is_frequency_valid(uint64_t requested_hz)
{
  return (requested_hz >= LMX2592_MIN_FREQUENCY_HZ) &&
         (requested_hz <= LMX2592_MAX_FREQUENCY_HZ);
}

static bool is_power_valid(uint8_t power_code)
{
  return (power_code <= 31U) || ((power_code >= 48U) && (power_code <= 63U));
}

static uint8_t segment_code(uint8_t divide)
{
  switch (divide)
  {
    case 2: return 1U;
    case 4: return 2U;
    case 6: return 4U;
    case 8: return 8U;
    default: return 0U;
  }
}

void LMX2592_WriteRegister(uint8_t address, uint16_t data)
{
  uint32_t frame;

  if (address > 64U)
  {
    return;
  }

  frame = ((uint32_t)address << 16) | data;
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_RESET);
  for (int bit = 23; bit >= 0; --bit)
  {
    HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LMX2592_DATA_GPIO_Port, LMX2592_DATA_Pin,
                      ((frame >> bit) & 1U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    short_delay();
    HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_SET);
    short_delay();
  }
  HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_SET);
  short_delay();
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_RESET);
  registers[address] = data;
}

static void update_outputs(bool divider_used)
{
  uint16_t r31 = registers[31] & (uint16_t)~((1U << 10) | (1U << 9) | (1U << 7));
  uint16_t r34 = registers[34] & (uint16_t)~(1U << 5);
  uint16_t r36 = registers[36] & (uint16_t)~((1U << 11) | (1U << 10));
  uint16_t r46 = registers[46] & (uint16_t)~((0x3FU << 8) | (1U << 7) | (1U << 6));
  uint16_t r47 = registers[47] & (uint16_t)~((0x3U << 11) | 0x3FU);
  uint16_t r48 = registers[48] & (uint16_t)~0x3U;

  r46 |= (uint16_t)output_power[0] << 8;
  r47 |= output_power[1];
  if (!output_enabled[0]) r46 |= 1U << 6;
  if (!output_enabled[1]) r46 |= 1U << 7;

  if (divider_used)
  {
    r31 |= (1U << 10) | (1U << 9);
    r34 |= 1U << 5;
    if (output_enabled[0]) r36 |= 1U << 10;
    if (output_enabled[1]) r36 |= 1U << 11;
  }
  else
  {
    r31 |= 1U << 7;
    if (!output_enabled[0]) r31 |= 1U << 9;
    if (!output_enabled[1]) r31 |= 1U << 10;
    r47 |= 1U << 11;
    r48 |= 1U;
  }

  LMX2592_WriteRegister(31U, r31);
  LMX2592_WriteRegister(34U, r34);
  LMX2592_WriteRegister(36U, r36);
  LMX2592_WriteRegister(46U, r46);
  LMX2592_WriteRegister(47U, r47);
  LMX2592_WriteRegister(48U, r48);
}

LMX2592_Status LMX2592_SetFrequency(uint64_t requested_hz)
{
  const ChannelDivider *divider = NULL;
  uint64_t vco_hz;
  uint64_t n_base;
  uint32_t n_integer;
  uint32_t numerator;
  uint32_t denominator;
  uint32_t common;
  bool doubler = false;

  if (!is_frequency_valid(requested_hz))
  {
    return LMX2592_ERROR_RANGE;
  }

  if (requested_hz > UINT64_C(7100000000))
  {
    doubler = true;
    vco_hz = (requested_hz + 1U) / 2U;
  }
  else if (requested_hz >= UINT64_C(3550000000))
  {
    vco_hz = requested_hz;
  }
  else
  {
    for (uint32_t i = 0; i < (sizeof(channel_dividers) / sizeof(channel_dividers[0])); ++i)
    {
      uint64_t candidate = requested_hz * channel_dividers[i].divide;
      if ((candidate >= UINT64_C(3550000000)) && (candidate <= UINT64_C(7100000000)))
      {
        divider = &channel_dividers[i];
        vco_hz = candidate;
        break;
      }
    }
    if (divider == NULL) return LMX2592_ERROR_RANGE;
  }

  n_base = (uint64_t)reference_hz * 2U;
  n_integer = (uint32_t)(vco_hz / n_base);
  numerator = (uint32_t)(vco_hz % n_base);
  denominator = (uint32_t)n_base;
  if (numerator != 0U)
  {
    common = gcd_u32(numerator, denominator);
    numerator /= common;
    denominator /= common;
  }
  else
  {
    denominator = 1U;
  }

  LMX2592_WriteRegister(30U, (registers[30] & (uint16_t)~1U) | (doubler ? 1U : 0U));
  LMX2592_WriteRegister(35U, divider == NULL ? registers[35] :
    (uint16_t)((registers[35] & 0xE078U) |
      ((uint16_t)segment_code(divider->seg2) << 9) |
      ((divider->seg3 != 1U) ? (1U << 8) : 0U) |
      ((divider->seg2 != 1U) ? (1U << 7) : 0U) |
      ((divider->seg1 == 3U) ? (1U << 2) : 0U) |
      (1U << 1) | 1U));
  if (divider != NULL)
  {
    uint8_t select = (divider->seg3 != 1U) ? 4U : ((divider->seg2 != 1U) ? 2U : 1U);
    uint16_t r36 = registers[36] & (uint16_t)~((0x7U << 4) | 0xFU);
    r36 |= (uint16_t)select << 4;
    r36 |= segment_code(divider->seg3);
    LMX2592_WriteRegister(36U, r36);
  }

  LMX2592_WriteRegister(38U, (registers[38] & 0xE001U) | (uint16_t)(n_integer << 1));
  LMX2592_WriteRegister(40U, (uint16_t)(denominator >> 16));
  LMX2592_WriteRegister(41U, (uint16_t)denominator);
  LMX2592_WriteRegister(44U, (uint16_t)(numerator >> 16));
  LMX2592_WriteRegister(45U, (uint16_t)numerator);

  if (numerator == 0U)
    LMX2592_WriteRegister(46U, registers[46] & (uint16_t)~((1U << 5) | 0x7U));
  else
    LMX2592_WriteRegister(46U, (registers[46] & (uint16_t)~0x7U) | (1U << 5) | 3U);

  update_outputs(divider != NULL);
  frequency_hz = requested_hz;
  LMX2592_WriteRegister(0U, registers[0] | (1U << 3));
  return LMX2592_OK;
}

void LMX2592_Init(void)
{
  HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LMX2592_DATA_GPIO_Port, LMX2592_DATA_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_RESET);
  LMX2592_SetChipEnabled(true);
  HAL_Delay(10U);
  LMX2592_WriteRegister(0U, 0x0002U);
  HAL_Delay(1U);
  for (uint32_t i = 0; i < (sizeof(register_defaults) / sizeof(register_defaults[0])); ++i)
  {
    LMX2592_WriteRegister(register_defaults[i].address, register_defaults[i].value);
  }
  LMX2592_WriteRegister(0U, 0x2014U);
  (void)LMX2592_SetFrequency(frequency_hz);
}

LMX2592_Status LMX2592_SetReference(uint32_t requested_hz)
{
  if ((requested_hz < LMX2592_MIN_REFERENCE_HZ) ||
      (requested_hz > LMX2592_MAX_REFERENCE_HZ))
  {
    return LMX2592_ERROR_RANGE;
  }
  reference_hz = requested_hz;
  return LMX2592_SetFrequency(frequency_hz);
}

LMX2592_Status LMX2592_SetPower(LMX2592_Output output, uint8_t power_code)
{
  if ((output > LMX2592_OUTPUT_B) || !is_power_valid(power_code))
  {
    return LMX2592_ERROR_ARGUMENT;
  }
  output_power[output] = power_code;
  update_outputs(frequency_hz < UINT64_C(3550000000));
  return LMX2592_OK;
}

LMX2592_Status LMX2592_ConfigureTx(LMX2592_Output output, uint64_t requested_hz,
                                   uint8_t power_code)
{
  LMX2592_Status status;

  if ((output > LMX2592_OUTPUT_B) || !is_frequency_valid(requested_hz) ||
      !is_power_valid(power_code))
  {
    return LMX2592_ERROR_ARGUMENT;
  }

  if (!chip_enabled) LMX2592_SetChipEnabled(true);

  output_enabled[LMX2592_OUTPUT_A] = false;
  output_enabled[LMX2592_OUTPUT_B] = false;
  update_outputs(frequency_hz < UINT64_C(3550000000));

  status = LMX2592_SetFrequency(requested_hz);
  if (status != LMX2592_OK) return status;

  output_power[output] = power_code;
  output_enabled[output] = true;
  update_outputs(frequency_hz < UINT64_C(3550000000));
  return LMX2592_OK;
}

void LMX2592_SetOutput(LMX2592_Output output, bool enabled)
{
  if (output <= LMX2592_OUTPUT_B)
  {
    output_enabled[output] = enabled;
    update_outputs(frequency_hz < UINT64_C(3550000000));
  }
}

void LMX2592_SetChipEnabled(bool enabled)
{
  HAL_GPIO_WritePin(LMX2592_CE_GPIO_Port, LMX2592_CE_Pin,
                    enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
  chip_enabled = enabled;
  if (enabled && registers[0] != 0U)
  {
    HAL_Delay(1U);
    LMX2592_WriteRegister(0U, registers[0] | (1U << 3));
  }
}

bool LMX2592_IsLocked(void)
{
  return HAL_GPIO_ReadPin(LMX2592_MUX_GPIO_Port, LMX2592_MUX_Pin) == GPIO_PIN_SET;
}

bool LMX2592_IsChipEnabled(void) { return chip_enabled; }
bool LMX2592_IsOutputEnabled(LMX2592_Output output) { return output_enabled[output]; }
uint8_t LMX2592_GetPower(LMX2592_Output output) { return output_power[output]; }
uint64_t LMX2592_GetFrequency(void) { return frequency_hz; }
uint32_t LMX2592_GetReference(void) { return reference_hz; }
