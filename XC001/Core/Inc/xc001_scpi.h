#ifndef XC001_SCPI_H
#define XC001_SCPI_H

#include <stddef.h>

void XC001_SCPI_Init(void);
void XC001_SCPI_Execute(const char *command, char *reply, size_t reply_size);

#endif
