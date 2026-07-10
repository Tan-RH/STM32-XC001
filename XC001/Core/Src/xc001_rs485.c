#include "xc001_rs485.h"
#include "usart.h"
#include "main.h"
#include "cmsis_os2.h"
#include "xc001_board.h"
#include <stdio.h>
#include <string.h>

#define RS485_DE_PORT GPIOE
#define RS485_DE_PIN  GPIO_PIN_3

static char s_last_rx[128];
static uint32_t s_rx_count;
static uint32_t s_tx_count;
static uint16_t s_rx_len;
static osMutexId_t s_mutex;

void XC001_RS485_Init(void)
{
  if (s_mutex == 0)
  {
    const osMutexAttr_t attr = {.name = "xc001_rs485"};
    s_mutex = osMutexNew(&attr);
    if (s_mutex == 0)
    {
      XC001_Board_SetStatusOk(0U);
    }
  }
  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
  s_last_rx[0] = '\0';
}

void XC001_RS485_Task(void)
{
  uint8_t ch;
  uint32_t budget = 64U;

  if (s_mutex == 0 || osMutexAcquire(s_mutex, 0U) != osOK)
  {
    return;
  }
  while (budget-- > 0U && HAL_UART_Receive(&huart8, &ch, 1, 0) == HAL_OK)
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
  if (s_mutex != 0)
  {
    (void)osMutexRelease(s_mutex);
  }
}

uint8_t XC001_RS485_SendText(const char *text)
{
  HAL_StatusTypeDef st;

  if (text == 0)
  {
    return 0U;
  }
  if (s_mutex == 0 || osMutexAcquire(s_mutex, osWaitForever) != osOK)
  {
    return 0U;
  }
  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_SET);
  st = HAL_UART_Transmit(&huart8, (uint8_t *)text, (uint16_t)strlen(text), 500);
  if (st == HAL_OK)
  {
    st = HAL_UART_Transmit(&huart8, (uint8_t *)"\r\n", 2, 50);
  }
  HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN, GPIO_PIN_RESET);
  if (st == HAL_OK)
  {
    s_tx_count++;
  }
  if (s_mutex != 0)
  {
    (void)osMutexRelease(s_mutex);
  }
  return (st == HAL_OK) ? 1U : 0U;
}

void XC001_RS485_Status(char *out, size_t out_size)
{
  uint32_t tx_count;
  uint32_t rx_count;

  if (s_mutex != 0)
  {
    (void)osMutexAcquire(s_mutex, osWaitForever);
  }
  tx_count = s_tx_count;
  rx_count = s_rx_count;
  if (s_mutex != 0)
  {
    (void)osMutexRelease(s_mutex);
  }
  snprintf(out, out_size, "RS485:TX=%lu,RX=%lu", (unsigned long)tx_count, (unsigned long)rx_count);
}

void XC001_RS485_LastRx(char *out, size_t out_size)
{
  char last_rx[sizeof(s_last_rx)];

  if (s_mutex != 0)
  {
    (void)osMutexAcquire(s_mutex, osWaitForever);
  }
  memcpy(last_rx, s_last_rx, sizeof(last_rx));
  if (s_mutex != 0)
  {
    (void)osMutexRelease(s_mutex);
  }
  last_rx[sizeof(last_rx) - 1U] = '\0';
  snprintf(out, out_size, "%s", last_rx[0] ? last_rx : "EMPTY");
}
