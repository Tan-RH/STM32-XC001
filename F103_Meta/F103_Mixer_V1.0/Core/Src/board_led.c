#include "board_led.h"

#include "main.h"

#define LED_BLINK_INTERVAL_MS    500U

/* Both LED cathodes are connected to the MCU, so the outputs are active low. */
static void set_led(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state)
{
  HAL_GPIO_WritePin(port, pin,
                    state == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void configure_gpio(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
  gpio.Pin = LED_BLUE_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_BLUE_GPIO_Port, &gpio);
  gpio.Pin = LED_RED_Pin;
  HAL_GPIO_Init(LED_RED_GPIO_Port, &gpio);
}

void BoardLed_Init(void)
{
  configure_gpio();
  set_led(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
  set_led(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_SET);
}

void BoardLed_Process(void)
{
  static uint32_t last_toggle;
  uint32_t now = HAL_GetTick();

  if ((uint32_t)(now - last_toggle) >= LED_BLINK_INTERVAL_MS)
  {
    last_toggle = now;
    HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port, LED_BLUE_Pin);
  }
}

void BoardLed_FatalError(void)
{
  configure_gpio();
  set_led(LED_BLUE_GPIO_Port, LED_BLUE_Pin, GPIO_PIN_RESET);
  set_led(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);

  while (1)
  {
    HAL_Delay(LED_BLINK_INTERVAL_MS);
    HAL_GPIO_TogglePin(LED_RED_GPIO_Port, LED_RED_Pin);
  }
}
