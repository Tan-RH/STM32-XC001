#include "xc001_console.h"
#include "xc001_config.h"
#include "xc001_scpi.h"
#include "usart.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

#define CONSOLE_RX_RING_SIZE 256U

static char s_line[XC001_SCPI_LINE_SIZE];
static uint16_t s_len;
static uint32_t s_next_heartbeat;
static uint8_t s_inited;
static uint8_t s_drop_line_end;
static osThreadId_t s_console_thread;
static uint8_t s_rx_byte;
static volatile uint8_t s_rx_ring[CONSOLE_RX_RING_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static volatile uint32_t s_rx_overflow;

static void console_start_rx_it(void)
{
  if (HAL_UART_Receive_IT(&huart7, &s_rx_byte, 1) != HAL_OK)
  {
    __HAL_UART_CLEAR_OREFLAG(&huart7);
    __HAL_UART_CLEAR_FEFLAG(&huart7);
    __HAL_UART_CLEAR_NEFLAG(&huart7);
    __HAL_UART_CLEAR_PEFLAG(&huart7);
    huart7.ErrorCode = HAL_UART_ERROR_NONE;
    huart7.RxState = HAL_UART_STATE_READY;
    (void)HAL_UART_Receive_IT(&huart7, &s_rx_byte, 1);
  }
}

static void console_write(const char *text)
{
  if (text != 0)
  {
    HAL_UART_Transmit(&huart7, (uint8_t *)text, (uint16_t)strlen(text), 200);
  }
}

static void console_print_prompt(void)
{
  console_write("\r\nXC001> ");
}

static void console_recover_uart(void)
{
  uint32_t err = HAL_UART_GetError(&huart7);

  if (err != HAL_UART_ERROR_NONE)
  {
    __HAL_UART_CLEAR_OREFLAG(&huart7);
    __HAL_UART_CLEAR_FEFLAG(&huart7);
    __HAL_UART_CLEAR_NEFLAG(&huart7);
    __HAL_UART_CLEAR_PEFLAG(&huart7);
    huart7.ErrorCode = HAL_UART_ERROR_NONE;
    huart7.RxState = HAL_UART_STATE_READY;
  }
}

static void console_print_status(const char *prefix)
{
  char ip[20];
  char msg[160];

  XC001_Config_FormatIp(XC001_NetConfig.ip, ip, sizeof(ip));
  if (strcmp(prefix, "HEARTBEAT") == 0)
  {
    snprintf(msg, sizeof(msg), "\r\n[%s] tick=%lu IP=%s UDP=%u",
             prefix,
             (unsigned long)osKernelGetTickCount(),
             ip,
             XC001_NetConfig.udp_port);
    console_write(msg);
    return;
  }
  snprintf(msg, sizeof(msg),
           "\r\n[%s] RUN tick=%lu IP=%s HTTP=http://%s/ UDP=%u UART7=115200,8N1\r\nType *IDN?, STAT? or SYST:HELP?",
           prefix,
           (unsigned long)osKernelGetTickCount(),
           ip,
           ip,
           XC001_NetConfig.udp_port);
  console_write(msg);
  console_print_prompt();
}

void XC001_Console_Init(void)
{
  if (s_inited != 0U)
  {
    return;
  }
  s_inited = 1U;
  s_len = 0U;
  s_next_heartbeat = osKernelGetTickCount() + 5000U;
  console_write("\r\n\r\n========================================\r\n");
  console_write("XC001 Control Board UART7 Console Ready\r\n");
  console_write("SCPI command mode enabled\r\n");
  console_write("========================================");
  console_print_status("BOOT");
  console_start_rx_it();

  if (s_console_thread == 0)
  {
    const osThreadAttr_t attr = {
      .name = "xc001_uart7",
      .stack_size = 1024 * 4,
      .priority = (osPriority_t)osPriorityAboveNormal
    };
    s_console_thread = osThreadNew((osThreadFunc_t)XC001_Console_Task, 0, &attr);
  }
}

void XC001_Console_WriteRaw(const char *text)
{
  console_write(text);
}

void XC001_Console_UartRxCpltCallback(UART_HandleTypeDef *huart)
{
  uint16_t next;

  if (huart != &huart7)
  {
    return;
  }
  next = (uint16_t)((s_rx_head + 1U) % CONSOLE_RX_RING_SIZE);
  if (next != s_rx_tail)
  {
    s_rx_ring[s_rx_head] = s_rx_byte;
    s_rx_head = next;
  }
  else
  {
    s_rx_overflow++;
  }
  console_start_rx_it();
}

void XC001_Console_UartErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != &huart7)
  {
    return;
  }
  console_start_rx_it();
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  XC001_Console_UartRxCpltCallback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  XC001_Console_UartErrorCallback(huart);
}

static void console_execute_line(void)
{
  char reply[XC001_SCPI_REPLY_SIZE];

  if (s_len == 0U)
  {
    s_drop_line_end = 0U;
    return;
  }
  s_line[s_len] = '\0';
  XC001_SCPI_Execute(s_line, reply, sizeof(reply));
  console_write("\r\n");
  console_write(reply);
  console_print_prompt();
  s_len = 0U;
  s_drop_line_end = 1U;
}

void XC001_Console_Task(void)
{
  uint8_t ch;

  for (;;)
  {
    console_recover_uart();
    while (s_rx_tail != s_rx_head)
    {
      ch = s_rx_ring[s_rx_tail];
      s_rx_tail = (uint16_t)((s_rx_tail + 1U) % CONSOLE_RX_RING_SIZE);
      if (ch == '\r' || ch == '\n')
      {
        if (s_drop_line_end != 0U)
        {
          s_drop_line_end = 0U;
        }
        else
        {
          console_execute_line();
        }
      }
      else if (ch == 0x08U || ch == 0x7FU)
      {
        if (s_len > 0U)
        {
          s_len--;
        }
      }
      else if (s_len + 1U < sizeof(s_line))
      {
        s_drop_line_end = 0U;
        s_line[s_len++] = (char)ch;
        if (ch == '?')
        {
          console_execute_line();
        }
      }
      else
      {
        s_len = 0U;
        console_write("\r\nERR,-350,\"Command line too long\"");
        console_print_prompt();
      }
    }

    if (s_len == 0U && (int32_t)(osKernelGetTickCount() - s_next_heartbeat) >= 0)
    {
      s_next_heartbeat = osKernelGetTickCount() + 5000U;
      console_print_status("HEARTBEAT");
    }
    osDelay(1);
  }
}
