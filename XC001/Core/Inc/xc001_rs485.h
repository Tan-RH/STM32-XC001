#ifndef XC001_RS485_H
#define XC001_RS485_H

#include <stddef.h>
#include <stdint.h>

void XC001_RS485_Init(void);
void XC001_RS485_Task(void);
uint8_t XC001_RS485_SendText(const char *text);
void XC001_RS485_Status(char *out, size_t out_size);
void XC001_RS485_LastRx(char *out, size_t out_size);

#endif
