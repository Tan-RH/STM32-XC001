#include "xc001_rs485.h"
#include "usart.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define RS485_DE_PORT GPIOE
#define RS485_DE_PIN  GPIO_PIN_3

static char s_last_rx[128];
static uint32_t s_rx_count;
static uint32_t s_tx_count;
static uint16_t s_rx_len;

void XC001_RS485_Init(void)
{
  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
  s_last_rx[0] = '\0';
}

void XC001_RS485_Task(void)
{
  uint8_t ch;

  while (HAL_UART_Receive(&huart8, &ch, 1, 0) == HAL_OK)
  {
    if (ch == '\r' || ch == '\n')
    {
      if (s_rx_len > 0U)
      {
        s_last_rx[s_rx_len] = '\0';
        s_rx_count++;
        s_rx_len = 0U;
      }
    }
    else if (s_rx_len + 1U < sizeof(s_last_rx))
    {
      s_last_rx[s_rx_len++] = (char)ch;
    }
  }
}

uint8_t XC001_RS485_SendText(const char *text)
{
  HAL_StatusTypeDef st;

  if (text == 0)
  {
    return 0U;
  }
  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
  st = HAL_UART_Transmit(&huart8, (uint8_t *)text, (uint16_t)strlen(text), 500);
  HAL_UART_Transmit(&huart8, (uint8_t *)"\r\n", 2, 50);
  HAL_Delay(2);
  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
  if (st == HAL_OK)
  {
    s_tx_count++;
  }
  return (st == HAL_OK) ? 1U : 0U;
}

void XC001_RS485_Status(char *out, size_t out_size)
{
  snprintf(out, out_size, "RS485:TX=%lu,RX=%lu", (unsigned long)s_tx_count, (unsigned long)s_rx_count);
}

void XC001_RS485_LastRx(char *out, size_t out_size)
{
  snprintf(out, out_size, "%s", s_last_rx[0] ? s_last_rx : "EMPTY");
}
