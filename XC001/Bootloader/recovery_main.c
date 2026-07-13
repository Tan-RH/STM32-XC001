#include "stm32h7xx_hal.h"
#include "xc001_update.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

__attribute__((noreturn, naked)) static void branch_to(
    uint32_t stack_pointer __attribute__((unused)),
    uint32_t reset_handler __attribute__((unused)))
{
  __asm volatile(
      "msr msp, r0\n"
      "bx r1\n");
}

static void recovery_tick_init(void)
{
#if defined(CoreDebug_DEMCR_TRCENA_Msk) && defined(DWT_CTRL_CYCCNTENA_Msk)
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0UL;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

uint32_t HAL_GetTick(void)
{
#if defined(DWT_CTRL_CYCCNTENA_Msk)
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0UL)
  {
    uint32_t ticks_per_ms = SystemCoreClock / 1000UL;
    if (ticks_per_ms == 0UL)
    {
      ticks_per_ms = 1UL;
    }
    return DWT->CYCCNT / ticks_per_ms;
  }
#endif
  static uint32_t fallback_tick;
  return fallback_tick++;
}

static const XC001_UpdateRecord *latest_update_record(void)
{
  const XC001_UpdateRecord *latest = 0;
  const uint32_t count = XC001_FLASH_SECTOR_SIZE / XC001_FLASH_WORD_SIZE;

  for (uint32_t i = 0U; i < count; i++)
  {
    const XC001_UpdateRecord *record =
        (const XC001_UpdateRecord *)(XC001_UPDATE_META_ADDRESS +
                                     (i * XC001_FLASH_WORD_SIZE));
    if (XC001_Update_RecordValid(record) &&
        (latest == 0 || (int32_t)(record->sequence - latest->sequence) > 0))
    {
      latest = record;
    }
  }
  return latest;
}

static uint8_t staged_image_valid(const XC001_UpdateRecord *record)
{
  const uint32_t *vectors = (const uint32_t *)XC001_STAGING_ADDRESS;

  return (record != 0 && record->state == XC001_UPDATE_STATE_PENDING &&
          XC001_Update_IsVectorValid(vectors, record->image_size) &&
          XC001_Update_Crc32((const void *)XC001_STAGING_ADDRESS,
                             record->image_size) == record->image_crc) ? 1U : 0U;
}

static uint8_t install_image(const XC001_UpdateRecord *record)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0UL;
  uint32_t rounded_size = (record->image_size + XC001_FLASH_WORD_SIZE - 1UL) &
                          ~(XC001_FLASH_WORD_SIZE - 1UL);
  HAL_StatusTypeDef status;

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    return 0U;
  }
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_1;
  erase.Sector = FLASH_SECTOR_1;
  erase.NbSectors = 5U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  status = HAL_FLASHEx_Erase(&erase, &sector_error);
  for (uint32_t offset = 0U; status == HAL_OK && offset < rounded_size;
       offset += XC001_FLASH_WORD_SIZE)
  {
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                               XC001_APPLICATION_ADDRESS + offset,
                               XC001_STAGING_ADDRESS + offset);
  }
  (void)HAL_FLASH_Lock();
  if (status != HAL_OK)
  {
    return 0U;
  }
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_CleanInvalidateDCache();
  }
  if ((SCB->CCR & SCB_CCR_IC_Msk) != 0U)
  {
    SCB_InvalidateICache();
  }
  __DSB();
  __ISB();
  return (XC001_Update_Crc32((const void *)XC001_APPLICATION_ADDRESS,
                             record->image_size) == record->image_crc) ? 1U : 0U;
}

static uint8_t flashword_is_erased(const void *address)
{
  const uint32_t *words = (const uint32_t *)address;

  for (uint32_t i = 0U; i < (XC001_FLASH_WORD_SIZE / sizeof(uint32_t)); i++)
  {
    if (words[i] != 0xFFFFFFFFUL)
    {
      return 0U;
    }
  }
  return 1U;
}

static uint8_t append_applied_record(const XC001_UpdateRecord *record)
{
  typedef union
  {
    XC001_UpdateRecord rec;
    uint32_t words[8];
  } UpdateFlashWord;

  UpdateFlashWord block __attribute__((aligned(32)));
  uint32_t address = 0UL;
  HAL_StatusTypeDef status;

  if (record == 0)
  {
    return 0U;
  }
  for (uint32_t i = 0U; i < (XC001_FLASH_SECTOR_SIZE / XC001_FLASH_WORD_SIZE); i++)
  {
    uint32_t slot = XC001_UPDATE_META_ADDRESS + (i * XC001_FLASH_WORD_SIZE);
    if (flashword_is_erased((const void *)slot))
    {
      address = slot;
      break;
    }
  }
  if (address == 0UL || HAL_FLASH_Unlock() != HAL_OK)
  {
    return 0U;
  }

  memset(&block, 0, sizeof(block));
  block.rec.magic = XC001_UPDATE_MAGIC;
  block.rec.version = XC001_UPDATE_RECORD_VERSION;
  block.rec.state = XC001_UPDATE_STATE_APPLIED;
  block.rec.sequence = record->sequence + 1UL;
  block.rec.image_size = record->image_size;
  block.rec.image_crc = record->image_crc;
  block.rec.record_crc = XC001_Update_Crc32(&block.rec,
                                             offsetof(XC001_UpdateRecord, record_crc));
  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address,
                             (uint32_t)(uintptr_t)block.words);
  (void)HAL_FLASH_Lock();
  return (status == HAL_OK && XC001_Update_RecordValid((const XC001_UpdateRecord *)address)) ? 1U : 0U;
}

__attribute__((noreturn)) static void jump_to_application(void)
{
  const uint32_t *vectors = (const uint32_t *)XC001_APPLICATION_ADDRESS;

  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk | SCB_ICSR_PENDSVCLR_Msk;
  for (uint32_t i = 0U; i < (sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0])); i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFUL;
    NVIC->ICPR[i] = 0xFFFFFFFFUL;
  }
  SCB->VTOR = XC001_APPLICATION_ADDRESS;
  __set_BASEPRI(0U);
  __set_CONTROL(0U);
  __DSB();
  __ISB();
  __enable_irq();
  branch_to(vectors[0], vectors[1]);
}

void Error_Handler(void)
{
  for (;;)
  {
    __WFI();
  }
}

int main(void)
{
  const XC001_UpdateRecord *latest;

  __disable_irq();
  recovery_tick_init();
  latest = latest_update_record();
  if (staged_image_valid(latest))
  {
    if (install_image(latest) && append_applied_record(latest))
    {
      NVIC_SystemReset();
    }
    Error_Handler();
  }
  if (XC001_Update_IsVectorValid((const uint32_t *)XC001_APPLICATION_ADDRESS,
                                 XC001_APPLICATION_MAX_SIZE))
  {
    jump_to_application();
  }
  Error_Handler();
  return 0;
}
