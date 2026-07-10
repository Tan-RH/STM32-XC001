#ifndef XC001_CONFIG_H
#define XC001_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define XC001_DEVICE_ID          "XC001,CONTROL-BOARD,H743,V1.0"
#define XC001_DEFAULT_IP0        192U
#define XC001_DEFAULT_IP1        168U
#define XC001_DEFAULT_IP2        1U
#define XC001_DEFAULT_IP3        10U
#define XC001_DEFAULT_GW0        192U
#define XC001_DEFAULT_GW1        168U
#define XC001_DEFAULT_GW2        1U
#define XC001_DEFAULT_GW3        1U
#define XC001_DEFAULT_MASK0      255U
#define XC001_DEFAULT_MASK1      255U
#define XC001_DEFAULT_MASK2      255U
#define XC001_DEFAULT_MASK3      0U
#define XC001_DEFAULT_UDP_PORT   4000U
#define XC001_HTTP_PORT          80U
#define XC001_STATUS_BLINK_MS    250U
#define XC001_NET_SERVICES_AUTOSTART 1U

#define XC001_SCPI_LINE_SIZE     192U
#define XC001_SCPI_REPLY_SIZE    384U
#define XC001_NET_RX_SIZE        256U
#define XC001_HTTP_RX_SIZE       1024U

typedef struct
{
  uint8_t ip[4];
  uint8_t gateway[4];
  uint8_t netmask[4];
  uint16_t udp_port;
} XC001_NetworkConfig;

extern XC001_NetworkConfig XC001_NetConfig;

void XC001_Config_LoadDefaults(void);
uint8_t XC001_Config_SetNetwork(const uint8_t ip[4], uint16_t udp_port);
uint8_t XC001_Config_SetNetworkFull(const uint8_t ip[4], const uint8_t netmask[4], const uint8_t gateway[4], uint16_t udp_port);
uint8_t XC001_Config_ValidateNetworkFull(const uint8_t ip[4], const uint8_t netmask[4], const uint8_t gateway[4], uint16_t udp_port);
uint8_t XC001_Config_ParseIp(const char *text, uint8_t ip[4]);
void XC001_Config_FormatIp(const uint8_t ip[4], char *out, size_t out_size);

#endif
