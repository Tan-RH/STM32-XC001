#ifndef XC001_SPI_BUS_H
#define XC001_SPI_BUS_H

#include <stddef.h>
#include <stdint.h>

void XC001_SPIBus_Init(void);
uint8_t XC001_SPIBus_Transfer(const uint8_t *tx, uint8_t *rx, uint8_t len);
void XC001_SPIBus_Status(char *out, size_t out_size);

#endif
