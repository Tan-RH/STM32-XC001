#ifndef XC001_CONSOLE_H
#define XC001_CONSOLE_H

#include "stm32h7xx_hal.h"

void XC001_Console_Init(void);
void XC001_Console_Task(void);
void XC001_Console_WriteRaw(const char *text);
void XC001_Console_UartRxCpltCallback(UART_HandleTypeDef *huart);
void XC001_Console_UartErrorCallback(UART_HandleTypeDef *huart);
uint32_t XC001_Console_GetRxOverflow(void);

#endif
