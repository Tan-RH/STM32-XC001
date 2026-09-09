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
  uint8_t seg1_en;
  uint8_t seg2;
  uint8_t seg2_en;
  uint8_t seg3;
  uint8_t seg3_en;
  uint8_t seg_sel;
} ChannelDivider;

static const ChannelDivider channel_dividers[] = {
  {2, 0, 1, 0, 0, 0, 0, 1}, {3, 1, 1, 0, 0, 0, 0, 1},
  {4, 0, 1, 1, 1, 0, 0, 2}, {6, 1, 1, 1, 1, 0, 0, 2},
  {8, 0, 1, 2, 1, 0, 0, 2}, {12, 1, 1, 2, 1, 0, 0, 2},
  {16, 0, 1, 8, 1, 0, 0, 2}, {24, 1, 1, 8, 1, 0, 0, 2},
  {32, 0, 1, 8, 1, 1, 1, 4}, {36, 1, 1, 4, 1, 1, 1, 4},
  {48, 1, 1, 8, 1, 1, 1, 4}, {64, 0, 1, 8, 1, 2, 1, 4},
  {96, 0, 1, 8, 1, 4, 1, 4}, {128, 0, 1, 8, 1, 8, 1, 4},
  {192, 1, 1, 8, 1, 8, 1, 4}
};

/* Board-validated baseline from the supplier's CREDIT-LMX2592 firmware. */
static const struct
{
  uint8_t address;
  uint16_t value;
} register_defaults[] = {
  {64, 0x037F}, {62, 0x0000}, {61, 0x0001}, {59, 0x0000},
  {48, 0x03FD}, {47, 0x00C0}, {46, 0x0000}, {45, 0x0000},
  {44, 0x0000}, {43, 0x0000}, {42, 0x0000}, {41, 0x03E8},
  {40, 0x0000}, {39, 0x8104}, {38, 0x0000}, {37, 0x4000},
  {36, 0x0000}, {35, 0x0019}, {34, 0xC3CA}, {33, 0x2A0A},
  {32, 0x210A}, {31, 0x0401}, {30, 0x0034}, {29, 0x0084},
  {28, 0x2924}, {25, 0x0000}, {24, 0x0509}, {23, 0x8B42},
  {22, 0x2300}, {20, 0x012C}, {19, 0x0AF5}, {14, 0x018F},
  {13, 0x4000}, {12, 0x7001}, {11, 0x0018}, {10, 0x10D8},
  {9, 0x0302}, {8, 0x1084}, {7, 0x28B2}, {4, 0x0543},
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

void LMX2592_WriteRegister(uint8_t address, uint16_t data)
{
  uint32_t frame;

  if (address > 64U)
  {
    return;
  }

  frame = ((uint32_t)address << 16) | data;
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_RESET);
  /* Match the supplier waveform: SCK idles high between frames. */
  HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_SET);
  for (int bit = 23; bit >= 0; --bit)
  {
    HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LMX2592_DATA_GPIO_Port, LMX2592_DATA_Pin,
                      ((frame >> bit) & 1U) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET);
    short_delay();
    HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_SET);
    short_delay();
  }
  HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_SET);
  short_delay();
  registers[address] = data;
}

/* Update the output-related shadow registers without touching the device. */
static void update_output_registers(bool divider_used)
{
  uint16_t r31 = registers[31] & (uint16_t)~((1U << 10) | (1U << 9) | (1U << 7));
  uint16_t r34 = registers[34] & (uint16_t)~(1U << 5);
  uint16_t r36 = registers[36] & (uint16_t)~((1U << 11) | (1U << 10));
  uint16_t r46 = registers[46] & (uint16_t)~((0x3FU << 8) | (1U << 7) | (1U << 6));
  uint16_t r47 = registers[47] & (uint16_t)~((0x3U << 11) | 0x3FU);
  /* OUTB_MUX=1 is the supplier board's fixed output routing. */
  uint16_t r48 = registers[48] | 1U;

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
  }

  registers[48] = r48;
  registers[47] = r47;
  registers[46] = r46;
  registers[36] = r36;
  registers[34] = r34;
  registers[31] = r31;
}

static void commit_output_registers(bool include_r48)
{
  /* The supplier updates output controls before the synthesizer block. */
  if (include_r48) LMX2592_WriteRegister(48U, registers[48]);
  LMX2592_WriteRegister(47U, registers[47]);
  LMX2592_WriteRegister(46U, registers[46]);
  LMX2592_WriteRegister(36U, registers[36]);
  LMX2592_WriteRegister(34U, registers[34]);
  LMX2592_WriteRegister(31U, registers[31]);
  if (registers[0] != 0U) LMX2592_WriteRegister(0U, registers[0] | (1U << 3));
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
  uint8_t mash_order;
  uint8_t pfd_delay;
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

  registers[30] = (registers[30] & (uint16_t)~1U) | (doubler ? 1U : 0U);
  registers[37] = (registers[37] & (uint16_t)~(1U << 12)) |
                  (doubler ? (1U << 12) : 0U);
  registers[35] = divider == NULL ? 0x0019U :
    (uint16_t)(0x0019U |
      ((uint16_t)divider->seg2 << 9) |
      ((uint16_t)divider->seg3_en << 8) |
      ((uint16_t)divider->seg2_en << 7) |
      ((uint16_t)divider->seg1 << 2) |
      ((uint16_t)divider->seg1_en << 1));
  if (divider != NULL)
  {
    uint16_t r36 = registers[36] & (uint16_t)~((0x7U << 4) | 0xFU);
    r36 |= (uint16_t)divider->seg_sel << 4;
    r36 |= divider->seg3;
    registers[36] = r36;
  }
  else
  {
    /* Clear stale divider fields when moving back to the direct VCO path. */
    registers[36] = 0x0000U;
  }

  registers[38] = (registers[38] & 0xE001U) | (uint16_t)(n_integer << 1);
  registers[40] = (uint16_t)(denominator >> 16);
  registers[41] = (uint16_t)denominator;
  registers[44] = (uint16_t)(numerator >> 16);
  registers[45] = (uint16_t)numerator;

  if (numerator == 0U)
  {
    mash_order = 0U;
    pfd_delay = 1U;
  }
  else if (n_integer < 16U)
  {
    mash_order = 1U;
    pfd_delay = 1U;
  }
  else if (n_integer < 18U)
  {
    mash_order = 2U;
    pfd_delay = 2U;
  }
  else if (n_integer < 30U)
  {
    mash_order = 3U;
    pfd_delay = 2U;
  }
  else
  {
    mash_order = 4U;
    pfd_delay = 8U;
  }
  registers[39] = (registers[39] & (uint16_t)~0x0F00U) |
                   ((uint16_t)pfd_delay << 8);
  registers[46] = (registers[46] & (uint16_t)~((1U << 5) | 0x7U)) |
                  (numerator != 0U ? (1U << 5) : 0U) | mash_order;

  update_output_registers(divider != NULL);
  frequency_hz = requested_hz;

  /* LMX2592 calibration is sensitive to programming order.  The supplier
     writes the synthesizer block from R47 down to R30 and commits with R0. */
  for (int address = 47; address >= 30; --address)
  {
    LMX2592_WriteRegister((uint8_t)address, registers[address]);
  }
  LMX2592_WriteRegister(0U, registers[0] | (1U << 3));
  return LMX2592_OK;
}

void LMX2592_Init(void)
{
  HAL_GPIO_WritePin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LMX2592_DATA_GPIO_Port, LMX2592_DATA_Pin, GPIO_PIN_SET);
  /* CSB/LE is active low; leave it high between serial frames. */
  HAL_GPIO_WritePin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin, GPIO_PIN_SET);
  LMX2592_SetChipEnabled(true);
  HAL_Delay(10U);
  LMX2592_WriteRegister(0U, 0x231EU);
  HAL_Delay(1U);
  LMX2592_WriteRegister(0U, 0x231CU);
  for (uint32_t i = 0; i < (sizeof(register_defaults) / sizeof(register_defaults[0])); ++i)
  {
    LMX2592_WriteRegister(register_defaults[i].address, register_defaults[i].value);
  }
  LMX2592_WriteRegister(0U, 0x221CU);
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
  update_output_registers(frequency_hz < UINT64_C(3550000000));
  commit_output_registers(false);
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
  update_output_registers(frequency_hz < UINT64_C(3550000000));
  commit_output_registers(false);

  status = LMX2592_SetFrequency(requested_hz);
  if (status != LMX2592_OK) return status;

  output_power[output] = power_code;
  output_enabled[output] = true;
  update_output_registers(frequency_hz < UINT64_C(3550000000));
  commit_output_registers(false);
  return LMX2592_OK;
}

void LMX2592_SetOutput(LMX2592_Output output, bool enabled)
{
  if (output <= LMX2592_OUTPUT_B)
  {
    output_enabled[output] = enabled;
    update_output_registers(frequency_hz < UINT64_C(3550000000));
    commit_output_registers(false);
  }
}

void LMX2592_ApplySupplierOutputProfile(void)
{
  uint16_t r46 = registers[46] & (uint16_t)~((0x3FU << 8) | (1U << 7) | (1U << 6));
  uint16_t r47 = registers[47] & (uint16_t)~((0x3U << 11) | 0x3FU);

  /* CREDIT-LMX2592-SW-3 uses OUTB_POW=63 and leaves OUTB_PD cleared. */
  r46 |= (uint16_t)output_power[LMX2592_OUTPUT_A] << 8;
  r47 |= 63U;
  output_enabled[LMX2592_OUTPUT_A] = true;
  output_enabled[LMX2592_OUTPUT_B] = true;
  output_power[LMX2592_OUTPUT_B] = 63U;
  registers[46] = r46;
  registers[47] = r47;
  registers[48] = 0x03FDU;
  commit_output_registers(true);
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
uint16_t LMX2592_GetRegister(uint8_t address)
{
  return address <= 64U ? registers[address] : 0U;
}
