#include "xc001_board.h"
#include "xc001_config.h"
#include "cmsis_os2.h"
#include "xc001_utils.h"
#include <stdio.h>
#include <string.h>

#define LED_RUN_PORT       GPIOB
#define LED_RUN_PIN        GPIO_PIN_1
#define LED_ERR_PORT       GPIOB
#define LED_ERR_PIN        GPIO_PIN_2
#define LED_ERR_ON         GPIO_PIN_RESET
#define LED_ERR_OFF        GPIO_PIN_SET
#define ETH_NRST_PORT      GPIOE
#define ETH_NRST_PIN       GPIO_PIN_15

static uint8_t s_status_ok = 1U;
static uint32_t s_last_blink;
static uint8_t s_led_on;
static uint8_t s_watchdog_started;

static const XC001_GpioItem s_gpio_table[] = {
  {"PE4", GPIOE, GPIO_PIN_4, 1}, {"PE5", GPIOE, GPIO_PIN_5, 1},
  {"PE6", GPIOE, GPIO_PIN_6, 1},
  {"PC8", GPIOC, GPIO_PIN_8, 1}, {"PC9", GPIOC, GPIO_PIN_9, 1},
  {"PC10", GPIOC, GPIO_PIN_10, 1}, {"PC11", GPIOC, GPIO_PIN_11, 1},
  {"PC12", GPIOC, GPIO_PIN_12, 1}, {"PC13", GPIOC, GPIO_PIN_13, 1},
  {"PA8", GPIOA, GPIO_PIN_8, 1}, {"PA9", GPIOA, GPIO_PIN_9, 1},
  {"PD2", GPIOD, GPIO_PIN_2, 1}, {"LED1", GPIOB, GPIO_PIN_1, 1},
  {"LED2", GPIOB, GPIO_PIN_2, 1}, {"ETH_NRST", GPIOE, GPIO_PIN_15, 0},
};

static void eth_nrst_as_output(GPIO_PinState level)
{
  GPIO_InitTypeDef init = {0};

  HAL_GPIO_WritePin(ETH_NRST_PORT, ETH_NRST_PIN, level);
  init.Pin = ETH_NRST_PIN;
  init.Mode = GPIO_MODE_OUTPUT_PP;
  init.Pull = GPIO_NOPULL;
  init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ETH_NRST_PORT, &init);
}

void XC001_Board_Init(void)
{
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_2, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_RUN_PORT, LED_RUN_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_ERR_PORT, LED_ERR_PIN, LED_ERR_OFF);
  eth_nrst_as_output(GPIO_PIN_SET);
  s_last_blink = osKernelGetTickCount();
}

void XC001_Board_Task(void)
{
  uint32_t now = osKernelGetTickCount();

  if ((now - s_last_blink) >= XC001_STATUS_BLINK_MS)
  {
    s_last_blink = now;
    s_led_on ^= 1U;
    HAL_GPIO_WritePin(LED_RUN_PORT, LED_RUN_PIN, s_led_on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ERR_PORT, LED_ERR_PIN, s_status_ok ? LED_ERR_OFF : LED_ERR_ON);
  }
}

void XC001_Board_SetStatusOk(uint8_t ok)
{
  s_status_ok = ok ? 1U : 0U;
}

void XC001_Board_WatchdogInit(void)
{
#if !defined(DEBUG)
  uint32_t timeout = 1000000UL;

  RCC->CSR |= RCC_CSR_LSION;
  while ((RCC->CSR & RCC_CSR_LSIRDY) == 0U && timeout > 0U)
  {
    timeout--;
  }
  if (timeout == 0U)
  {
    XC001_Board_SetStatusOk(0U);
    return;
  }

  IWDG1->KR = 0x5555U;
  IWDG1->PR = 6U;
  IWDG1->RLR = 999U;
  timeout = 1000000UL;
  while (IWDG1->SR != 0U && timeout > 0U)
  {
    timeout--;
  }
  if (timeout == 0U)
  {
    XC001_Board_SetStatusOk(0U);
    return;
  }
  IWDG1->KR = 0xAAAAU;
  IWDG1->KR = 0xCCCCU;
  s_watchdog_started = 1U;
#endif
}

uint32_t XC001_Board_GetAndClearResetFlags(void)
{
  uint32_t flags = RCC->RSR;
  RCC->RSR |= RCC_RSR_RMVF;
  return flags;
}

void XC001_Board_WatchdogRefresh(void)
{
  if (s_watchdog_started != 0U)
  {
    IWDG1->KR = 0xAAAAU;
  }
}

uint8_t XC001_Board_IsEthResetPressed(void)
{
  GPIO_InitTypeDef init = {0};
  GPIO_PinState state;

  init.Pin = ETH_NRST_PIN;
  init.Mode = GPIO_MODE_INPUT;
  init.Pull = GPIO_PULLUP;
  init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ETH_NRST_PORT, &init);
  osDelay(20);
  state = HAL_GPIO_ReadPin(ETH_NRST_PORT, ETH_NRST_PIN);
  eth_nrst_as_output(GPIO_PIN_SET);
  return (state == GPIO_PIN_RESET) ? 1U : 0U;
}

void XC001_Board_PhyResetPulse(void)
{
  /* ETH_NRST is active-low: low resets the PHY, high releases it. */
  eth_nrst_as_output(GPIO_PIN_RESET);
  osDelay(100);
  eth_nrst_as_output(GPIO_PIN_SET);
  osDelay(300);
}

const XC001_GpioItem *XC001_Board_FindGpio(const char *name)
{
  for (uint32_t i = 0; i < (sizeof(s_gpio_table) / sizeof(s_gpio_table[0])); i++)
  {
    if (XC001_StrCaseCmp(name, s_gpio_table[i].name) == 0)
    {
      return &s_gpio_table[i];
    }
  }
  return 0;
}

uint8_t XC001_Board_WriteGpio(const char *name, uint8_t value, uint8_t toggle)
{
  const XC001_GpioItem *item = XC001_Board_FindGpio(name);

  if (item == 0 || item->writable == 0U)
  {
    return 0U;
  }
  if (toggle != 0U)
  {
    HAL_GPIO_TogglePin(item->port, item->pin);
  }
  else
  {
    HAL_GPIO_WritePin(item->port, item->pin, value ? GPIO_PIN_SET : GPIO_PIN_RESET);
  }
  return 1U;
}

uint8_t XC001_Board_ReadGpio(const char *name, uint8_t *value)
{
  const XC001_GpioItem *item = XC001_Board_FindGpio(name);

  if (item == 0 || value == 0)
  {
    return 0U;
  }
  *value = (HAL_GPIO_ReadPin(item->port, item->pin) == GPIO_PIN_SET) ? 1U : 0U;
  return 1U;
}

void XC001_Board_GpioList(char *out, size_t out_size)
{
  size_t used = 0U;

  if (out == 0 || out_size == 0U)
  {
    return;
  }
  out[0] = '\0';
  for (uint32_t i = 0; i < (sizeof(s_gpio_table) / sizeof(s_gpio_table[0])); i++)
  {
    int n = snprintf(out + used, out_size - used, "%s%s", (i == 0U) ? "" : ",", s_gpio_table[i].name);
    if (n < 0 || (size_t)n >= out_size - used)
    {
      out[out_size - 1U] = '\0';
      return;
    }
    used += (size_t)n;
  }
}
