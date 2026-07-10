#ifndef XC001_NET_H
#define XC001_NET_H

#include <stdint.h>
#include <stddef.h>

void XC001_Net_Init(void);
void XC001_Net_StartServices(void);
void XC001_Net_RequestStartServices(void);
void XC001_Net_Task(void);
uint8_t XC001_Net_ApplyConfig(uint8_t save_to_flash);
void XC001_Net_Diag(char *out, uint32_t out_size);
void XC001_Net_PhyDiag(char *out, uint32_t out_size);
void XC001_Net_FormatMac(char *out, size_t out_size);
void XC001_Net_FormatRemoteKey(char *out, size_t out_size);

#endif
