#ifndef XC001_STORAGE_H
#define XC001_STORAGE_H

#include "xc001_config.h"
#include <stdint.h>

void XC001_Storage_Init(void);
uint8_t XC001_Storage_Load(XC001_NetworkConfig *cfg);
uint8_t XC001_Storage_Save(const XC001_NetworkConfig *cfg);
uint8_t XC001_Storage_Reset(void);
uint32_t XC001_Storage_FirmwareMaxSize(void);
uint8_t XC001_Storage_FirmwareBegin(uint32_t image_size);
uint8_t XC001_Storage_FirmwareWrite(const uint8_t *data, uint32_t length);
uint8_t XC001_Storage_FirmwareFinishWithSize(uint32_t image_size, uint32_t *image_crc);
uint8_t XC001_Storage_FirmwareFinish(uint32_t *image_crc);
void XC001_Storage_FirmwareAbort(void);
uint8_t XC001_Storage_ConfirmRunningFirmware(void);

#endif
