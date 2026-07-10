#ifndef XC001_SCPI_H
#define XC001_SCPI_H

#include <stddef.h>
#include <stdint.h>

void XC001_SCPI_Init(void);
void XC001_SCPI_Execute(const char *command, char *reply, size_t reply_size);
uint8_t XC001_SCPI_IsReadOnly(const char *command);

#endif
