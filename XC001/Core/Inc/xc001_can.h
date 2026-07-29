#ifndef XC001_CAN_H
#define XC001_CAN_H

#include <stddef.h>
#include <stdint.h>

typedef enum
{
  XC001_CAN_QUERY_OK = 0,
  XC001_CAN_QUERY_NOT_READY,
  XC001_CAN_QUERY_BUSY,
  XC001_CAN_QUERY_SEND_FAILED,
  XC001_CAN_QUERY_TIMEOUT
} XC001_CAN_QueryResult;

typedef struct
{
  uint8_t node_id;
  uint32_t frequency_khz;
  uint8_t device_status;
  int16_t h_power_cdbm;
  int16_t v_power_cdbm;
  uint8_t sample_mode;
  uint8_t sample_state;
} XC001_CAN_RxPower;

void XC001_CAN_Init(void);
void XC001_CAN_Task(void);
uint8_t XC001_CAN_Send(uint32_t id, const uint8_t *data, uint8_t len);
XC001_CAN_QueryResult XC001_CAN_QueryRxPower(uint8_t node_id,
                                             uint32_t frequency_khz,
                                             uint8_t sample_time_ms,
                                             uint8_t sample_mode,
                                             uint16_t trigger_threshold,
                                             uint16_t trigger_timeout_ms,
                                             uint8_t antenna_compensation,
                                             XC001_CAN_RxPower *result);
void XC001_CAN_Status(char *out, size_t out_size);
void XC001_CAN_LastRx(char *out, size_t out_size);

#endif
