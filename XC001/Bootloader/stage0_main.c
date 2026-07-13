#include "stm32h743xx.h"
#include "xc001_update.h"
#include <stdint.h>

#ifndef XC001_STAGE0_ATTACH_DELAY_CYCLES
#define XC001_STAGE0_ATTACH_DELAY_CYCLES 0UL
#endif

__attribute__((noreturn, naked)) static void branch_to(
    uint32_t stack_pointer __attribute__((unused)),
    uint32_t reset_handler __attribute__((unused)))
{
  __asm volatile(
      "msr msp, r0\n"
      "bx r1\n");
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

static uint8_t target_vector_valid(uint32_t address, uint32_t end_address)
{
  const uint32_t *vectors = (const uint32_t *)address;
  uint32_t reset_handler = vectors[1] & ~1UL;

  return (XC001_Update_IsStackPointerValid(vectors[0]) &&
          (vectors[1] & 1UL) != 0UL && reset_handler >= address &&
          reset_handler < end_address) ? 1U : 0U;
}

static void debug_attach_window(void)
{
#if defined(RCC_APB4ENR_DBGMCUEN)
  RCC->APB4ENR |= RCC_APB4ENR_DBGMCUEN;
  (void)RCC->APB4ENR;
#endif
#if defined(DBGMCU_CR_DBG_SLEEPD1)
  DBGMCU->CR |= DBGMCU_CR_DBG_SLEEPD1;
#endif
#if defined(DBGMCU_CR_DBG_STOPD1)
  DBGMCU->CR |= DBGMCU_CR_DBG_STOPD1;
#endif
#if defined(DBGMCU_CR_DBG_STANDBYD1)
  DBGMCU->CR |= DBGMCU_CR_DBG_STANDBYD1;
#endif
#if defined(DBGMCU_APB4FZ1_DBG_IWDG1)
  DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
#endif

#if (XC001_STAGE0_ATTACH_DELAY_CYCLES > 0UL)
  for (volatile uint32_t i = 0U; i < XC001_STAGE0_ATTACH_DELAY_CYCLES; i++)
  {
    __NOP();
  }
#endif
}

__attribute__((noreturn)) static void jump_to(uint32_t address)
{
  const uint32_t *vectors = (const uint32_t *)address;

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
  SCB->VTOR = address;
  __set_BASEPRI(0U);
  __set_CONTROL(0U);
  __DSB();
  __ISB();
  __enable_irq();
  branch_to(vectors[0], vectors[1]);
}

int main(void)
{
  const XC001_UpdateRecord *latest = latest_update_record();

  debug_attach_window();

  if (latest != 0 && latest->state == XC001_UPDATE_STATE_PENDING &&
      target_vector_valid(XC001_BOOTLOADER_ADDRESS,
                          XC001_BOOTLOADER_ADDRESS + XC001_FLASH_SECTOR_SIZE))
  {
    jump_to(XC001_BOOTLOADER_ADDRESS);
  }
  if (target_vector_valid(XC001_APPLICATION_ADDRESS,
                          XC001_APPLICATION_ADDRESS + XC001_APPLICATION_MAX_SIZE))
  {
    jump_to(XC001_APPLICATION_ADDRESS);
  }
  for (;;)
  {
    __WFI();
  }
}
