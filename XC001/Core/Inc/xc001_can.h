#ifndef XC001_CAN_H
#define XC001_CAN_H

#include <stddef.h>
#include <stdint.h>

void XC001_CAN_Init(void);
void XC001_CAN_Task(void);
uint8_t XC001_CAN_Send(uint32_t id, const uint8_t *data, uint8_t len);
void XC001_CAN_Status(char *out, size_t out_size);
void XC001_CAN_LastRx(char *out, size_t out_size);

#endif
