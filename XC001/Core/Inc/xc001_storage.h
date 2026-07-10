#ifndef XC001_STORAGE_H
#define XC001_STORAGE_H

#include "xc001_config.h"
#include <stdint.h>

void XC001_Storage_Init(void);
uint8_t XC001_Storage_Load(XC001_NetworkConfig *cfg);
uint8_t XC001_Storage_Save(const XC001_NetworkConfig *cfg);
uint8_t XC001_Storage_Reset(void);

#endif
