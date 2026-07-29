#ifndef XC001_BOARD_H
#define XC001_BOARD_H

#include "main.h"
#include <stdint.h>
#include <stddef.h>

typedef struct
{
  const char *name;
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t writable;
} XC001_GpioItem;

void XC001_Board_Init(void);
void XC001_Board_Task(void);
void XC001_Board_SetStatusOk(uint8_t ok);
void XC001_Board_IndicateCommandError(void);
void XC001_Board_WatchdogInit(void);
void XC001_Board_WatchdogRefresh(void);
uint32_t XC001_Board_GetAndClearResetFlags(void);
uint8_t XC001_Board_IsEthResetPressed(void);
void XC001_Board_PhyResetPulse(void);
const XC001_GpioItem *XC001_Board_FindGpio(const char *name);
uint8_t XC001_Board_WriteGpio(const char *name, uint8_t value, uint8_t toggle);
uint8_t XC001_Board_ReadGpio(const char *name, uint8_t *value);
void XC001_Board_GpioList(char *out, size_t out_size);
uint8_t XC001_Board_SetRfChannel(uint8_t channel);
uint8_t XC001_Board_GetRfChannel(void);
void XC001_Board_RfSwitchStatus(char *out, size_t out_size);

#endif
