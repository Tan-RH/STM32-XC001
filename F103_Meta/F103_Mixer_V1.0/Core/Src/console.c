#include "console.h"

#include "app_version.h"
#include "lmx2592.h"
#include "control_panel.h"
#include "gpio.h"
#include "usart.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONSOLE_LINE_SIZE 96U
#define CONSOLE_RX_BUFFER_SIZE 256U

static char line_buffer[CONSOLE_LINE_SIZE];
static uint32_t line_length;
static uint8_t rx_buffer[CONSOLE_RX_BUFFER_SIZE];
static uint8_t rx_byte;
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile bool rx_overflow;

static void write_text(const char *text)
{
  HAL_UART_Transmit(&huart1, (const uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

static void write_format(const char *format, ...)
{
  char buffer[192];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  write_text(buffer);
}

static void uppercase(char *text)
{
  while (*text != '\0')
  {
    *text = (char)toupper((unsigned char)*text);
    ++text;
  }
}

static bool parse_frequency(const char *text, uint64_t *frequency)
{
  char *end;
  uint64_t whole = 0U;
  uint64_t fraction = 0U;
  uint64_t fraction_scale = 1U;
  uint64_t multiplier = UINT64_C(1000000);

  if (!isdigit((unsigned char)*text)) return false;
  while (isdigit((unsigned char)*text)) whole = whole * 10U + (uint64_t)(*text++ - '0');
  if (*text == '.')
  {
    ++text;
    while (isdigit((unsigned char)*text) && (fraction_scale < UINT64_C(1000000000)))
    {
      fraction = fraction * 10U + (uint64_t)(*text++ - '0');
      fraction_scale *= 10U;
    }
    while (isdigit((unsigned char)*text)) ++text;
  }
  end = (char *)text;
  while (isspace((unsigned char)*end)) ++end;
  if (strcmp(end, "HZ") == 0) multiplier = 1U;
  else if (strcmp(end, "KHZ") == 0) multiplier = UINT64_C(1000);
  else if ((*end == '\0') || (strcmp(end, "MHZ") == 0)) multiplier = UINT64_C(1000000);
  else if (strcmp(end, "GHZ") == 0) multiplier = UINT64_C(1000000000);
  else return false;
  if (whole > (UINT64_MAX / multiplier)) return false;
  *frequency = whole * multiplier + (fraction * multiplier) / fraction_scale;
  return true;
}

static LMX2592_Output parse_output(const char *text, bool *valid)
{
  *valid = true;
  if (strcmp(text, "A") == 0) return LMX2592_OUTPUT_A;
  if (strcmp(text, "B") == 0) return LMX2592_OUTPUT_B;
  *valid = false;
  return LMX2592_OUTPUT_A;
}

static void show_help(void)
{
  write_text("Commands:\r\n"
             "  FREQ <value>[HZ|KHZ|MHZ|GHZ]  (plain value is MHz)\r\n"
             "  TX <A|B> <frequency> <power>\r\n"
             "  POWER <A|B> <0..31|48..63>\r\n"
             "  OUT <A|B|ALL> <ON|OFF>\r\n"
             "  CE <ON|OFF>\r\n"
             "  REF <value>[HZ|KHZ|MHZ]       (plain value is MHz)\r\n"
             "  SUPPLIER                    (dual-output supplier test profile)\r\n"
             "  STATUS | PINS | REGS | VERSION | INIT | HELP\r\n");
}

static void show_diagnostics(const char *command)
{
  if (strcmp(command, "PINS") == 0)
  {
    write_format("PINS CE=%u MUX=%u LE=%u CLK=%u DATA=%u\r\n",
                 (unsigned)HAL_GPIO_ReadPin(LMX2592_CE_GPIO_Port, LMX2592_CE_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LMX2592_MUX_GPIO_Port, LMX2592_MUX_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LMX2592_LE_GPIO_Port, LMX2592_LE_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LMX2592_CLK_GPIO_Port, LMX2592_CLK_Pin),
                 (unsigned)HAL_GPIO_ReadPin(LMX2592_DATA_GPIO_Port, LMX2592_DATA_Pin));
  }
  else
  {
    write_format("REGS R0=%04X R30=%04X R31=%04X R34=%04X R35=%04X R36=%04X\r\n"
                 "     R37=%04X R38=%04X R39=%04X R46=%04X R47=%04X R48=%04X\r\n",
                 LMX2592_GetRegister(0U), LMX2592_GetRegister(30U),
                 LMX2592_GetRegister(31U), LMX2592_GetRegister(34U),
                 LMX2592_GetRegister(35U), LMX2592_GetRegister(36U),
                 LMX2592_GetRegister(37U), LMX2592_GetRegister(38U),
                 LMX2592_GetRegister(39U), LMX2592_GetRegister(46U),
                 LMX2592_GetRegister(47U), LMX2592_GetRegister(48U));
  }
}

static void show_status(void)
{
  uint64_t frequency = LMX2592_GetFrequency();
  write_format("FREQ=%lu.%06lu MHz REF=%lu.%06lu MHz LOCK=%s CE=%s\r\n"
               "OUTA=%s POWER=%u OUTB=%s POWER=%u\r\n",
               (unsigned long)(frequency / UINT64_C(1000000)),
               (unsigned long)(frequency % UINT64_C(1000000)),
               (unsigned long)(LMX2592_GetReference() / 1000000U),
               (unsigned long)(LMX2592_GetReference() % 1000000U),
               LMX2592_IsLocked() ? "YES" : "NO",
               LMX2592_IsChipEnabled() ? "ON" : "OFF",
               LMX2592_IsOutputEnabled(LMX2592_OUTPUT_A) ? "ON" : "OFF",
               LMX2592_GetPower(LMX2592_OUTPUT_A),
               LMX2592_IsOutputEnabled(LMX2592_OUTPUT_B) ? "ON" : "OFF",
               LMX2592_GetPower(LMX2592_OUTPUT_B));
}

static void execute_line(char *line)
{
  char *command;
  char *argument1;
  char *argument2;
  char *argument3;
  bool valid;

  uppercase(line);
  command = strtok(line, " \t");
  argument1 = strtok(NULL, " \t");
  argument2 = strtok(NULL, " \t");
  argument3 = strtok(NULL, " \t");
  if (command == NULL) return;

  if (strcmp(command, "HELP") == 0) show_help();
  else if (strcmp(command, "VERSION") == 0) write_text(APP_VERSION_STRING "\r\n");
  else if (strcmp(command, "STATUS") == 0) show_status();
  else if ((strcmp(command, "PINS") == 0) || (strcmp(command, "REGS") == 0))
    show_diagnostics(command);
  else if (strcmp(command, "SUPPLIER") == 0)
  {
    LMX2592_ApplySupplierOutputProfile();
    write_text("OK SUPPLIER PROFILE: OUTB ENABLED\r\n");
  }
  else if (strcmp(command, "INIT") == 0)
  {
    LMX2592_Init();
    ControlPanel_SyncFromDevice(LMX2592_OUTPUT_A, true);
    write_text("OK\r\n");
  }
  else if ((strcmp(command, "FREQ") == 0) && (argument1 != NULL))
  {
    uint64_t frequency;
    if (!parse_frequency(argument1, &frequency)) write_text("ERR bad frequency\r\n");
    else if (LMX2592_SetFrequency(frequency) != LMX2592_OK) write_text("ERR range 20MHz..9.8GHz\r\n");
    else { ControlPanel_SyncFromDevice(LMX2592_OUTPUT_A, false); write_text("OK\r\n"); }
  }
  else if ((strcmp(command, "TX") == 0) && (argument1 != NULL) &&
           (argument2 != NULL) && (argument3 != NULL))
  {
    LMX2592_Output output = parse_output(argument1, &valid);
    uint64_t frequency;
    char *end;
    unsigned long power = strtoul(argument3, &end, 0);
    if (!valid || !parse_frequency(argument2, &frequency) || (*end != '\0') ||
        (power > 63U) ||
        (LMX2592_ConfigureTx(output, frequency, (uint8_t)power) != LMX2592_OK))
      write_text("ERR use: TX <A|B> <20MHz..9.8GHz> <0..31|48..63>\r\n");
    else { ControlPanel_SyncFromDevice(output, true); write_text("OK\r\n"); }
  }
  else if ((strcmp(command, "REF") == 0) && (argument1 != NULL))
  {
    uint64_t reference;
    if (!parse_frequency(argument1, &reference) || (reference > UINT32_MAX)) write_text("ERR bad reference\r\n");
    else if (LMX2592_SetReference((uint32_t)reference) != LMX2592_OK) write_text("ERR range 5MHz..200MHz\r\n");
    else { ControlPanel_SyncFromDevice(LMX2592_OUTPUT_A, false); write_text("OK\r\n"); }
  }
  else if ((strcmp(command, "POWER") == 0) && (argument1 != NULL) && (argument2 != NULL))
  {
    LMX2592_Output output = parse_output(argument1, &valid);
    char *end;
    unsigned long power = strtoul(argument2, &end, 0);
    if (!valid || (*end != '\0') || (power > 63U) ||
        (LMX2592_SetPower(output, (uint8_t)power) != LMX2592_OK))
      write_text("ERR power code must be 0..31 or 48..63\r\n");
    else { ControlPanel_SyncFromDevice(output, true); write_text("OK\r\n"); }
  }
  else if ((strcmp(command, "OUT") == 0) && (argument1 != NULL) && (argument2 != NULL))
  {
    bool enable = strcmp(argument2, "ON") == 0;
    if (!enable && (strcmp(argument2, "OFF") != 0)) write_text("ERR expected ON or OFF\r\n");
    else if (strcmp(argument1, "ALL") == 0)
    {
      LMX2592_SetOutput(LMX2592_OUTPUT_A, enable);
      LMX2592_SetOutput(LMX2592_OUTPUT_B, enable);
      ControlPanel_SyncFromDevice(LMX2592_OUTPUT_A, false);
      write_text("OK\r\n");
    }
    else
    {
      LMX2592_Output output = parse_output(argument1, &valid);
      if (!valid) write_text("ERR expected A, B or ALL\r\n");
      else
      {
        LMX2592_SetOutput(output, enable);
        ControlPanel_SyncFromDevice(output, true);
        write_text("OK\r\n");
      }
    }
  }
  else if ((strcmp(command, "CE") == 0) && (argument1 != NULL))
  {
    if (strcmp(argument1, "ON") == 0)
    {
      LMX2592_SetChipEnabled(true);
      ControlPanel_SyncFromDevice(LMX2592_OUTPUT_A, false);
      write_text("OK\r\n");
    }
    else if (strcmp(argument1, "OFF") == 0)
    {
      LMX2592_SetChipEnabled(false);
      ControlPanel_SyncFromDevice(LMX2592_OUTPUT_A, false);
      write_text("OK\r\n");
    }
    else write_text("ERR expected ON or OFF\r\n");
  }
  else write_text("ERR unknown command; use HELP\r\n");
}

void Console_Init(void)
{
  if (HAL_UART_Receive_IT(&huart1, &rx_byte, 1U) != HAL_OK) Error_Handler();
  write_text("\r\nLMX2592 controller " APP_VERSION_STRING " ready\r\n");
  show_status();
}

void Console_Process(void)
{
  uint8_t byte;
  if (rx_overflow)
  {
    __disable_irq();
    rx_tail = rx_head;
    rx_overflow = false;
    __enable_irq();
    line_length = 0U;
    write_text("\r\nERR serial receive overflow; resend command\r\n");
    return;
  }
  if (rx_tail == rx_head) return;
  byte = rx_buffer[rx_tail++];
  if ((byte == '\r') || (byte == '\n'))
  {
    if (line_length != 0U)
    {
      line_buffer[line_length] = '\0';
      write_text("\r\n");
      execute_line(line_buffer);
      line_length = 0U;
    }
  }
  else if ((byte == 0x08U) || (byte == 0x7FU))
  {
    if (line_length != 0U) { --line_length; write_text("\b \b"); }
  }
  else if (isprint(byte) && (line_length < (CONSOLE_LINE_SIZE - 1U)))
  {
    line_buffer[line_length++] = (char)byte;
    HAL_UART_Transmit(&huart1, &byte, 1U, HAL_MAX_DELAY);
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1)
  {
    uint8_t next = (uint8_t)(rx_head + 1U);
    if (next == rx_tail) rx_overflow = true;
    else
    {
      rx_buffer[rx_head] = rx_byte;
      rx_head = next;
    }
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
  if (uart->Instance == USART1)
  {
    __HAL_UART_CLEAR_OREFLAG(uart);
    (void)HAL_UART_Receive_IT(&huart1, &rx_byte, 1U);
  }
}
