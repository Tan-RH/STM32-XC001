#ifndef __LMX2592_H
#define __LMX2592_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  LMX2592_OUTPUT_A = 0,
  LMX2592_OUTPUT_B = 1
} LMX2592_Output;

typedef enum
{
  LMX2592_OK = 0,
  LMX2592_ERROR_ARGUMENT,
  LMX2592_ERROR_RANGE
} LMX2592_Status;

void LMX2592_Init(void);
LMX2592_Status LMX2592_SetFrequency(uint64_t frequency_hz);
LMX2592_Status LMX2592_SetReference(uint32_t reference_hz);
LMX2592_Status LMX2592_SetPower(LMX2592_Output output, uint8_t power_code);
LMX2592_Status LMX2592_ConfigureTx(LMX2592_Output output, uint64_t frequency_hz,
                                   uint8_t power_code);
void LMX2592_SetOutput(LMX2592_Output output, bool enabled);
/* Temporarily apply the supplier's dual-output test profile. */
void LMX2592_ApplySupplierOutputProfile(void);
void LMX2592_SetChipEnabled(bool enabled);
void LMX2592_WriteRegister(uint8_t address, uint16_t data);
bool LMX2592_IsLocked(void);
bool LMX2592_IsChipEnabled(void);
bool LMX2592_IsOutputEnabled(LMX2592_Output output);
uint8_t LMX2592_GetPower(LMX2592_Output output);
uint64_t LMX2592_GetFrequency(void);
uint32_t LMX2592_GetReference(void);
uint16_t LMX2592_GetRegister(uint8_t address);

#ifdef __cplusplus
}
#endif

#endif
