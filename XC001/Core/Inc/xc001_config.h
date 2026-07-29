#ifndef XC001_CONFIG_H
#define XC001_CONFIG_H

#include <stdint.h>
#include <stddef.h>

#define XC001_SOFTWARE_VERSION   "V1.3.6"
#define XC001_DEVICE_ID          "XC001,CONTROL-BOARD,H743," XC001_SOFTWARE_VERSION

/* 240 MHz core / 120 MHz peripheral buses. This substantially reduces the
 * idle power while preserving Ethernet, FDCAN and UART peripheral clocks. */
#define XC001_LOW_POWER_PROFILE  1U
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
#define XC001_COMMAND_ERROR_MS   2000U
#define XC001_NET_SERVICES_AUTOSTART 1U

/* Keep a short window after reset so ST-LINK/CubeProgrammer can attach again
 * after flashing without requiring the user to hold NRST manually.
 * Set to 0U for production builds that need the fastest boot. */
#define XC001_DEBUG_ATTACH_DELAY_MS 0U

/* IWDG is disabled by default while the bootloader/web-upgrade path is being
 * commissioned. Re-enable only after the firmware is stable and the debugger
 * is configured for connect-under-reset. */
#define XC001_WATCHDOG_ENABLE    0U

#define XC001_SCPI_LINE_SIZE     192U
#define XC001_SCPI_REPLY_SIZE    384U
#define XC001_NET_RX_SIZE        256U
#define XC001_HTTP_RX_SIZE       1024U

/* PF7/PF8/PF9 are used as RF switch V1/V2/V3 control lines in this hardware
 * revision, so the former SPI5 bus on these pins must stay disabled at the
 * application layer. */
#define XC001_SPI5_BUS_ENABLE    0U

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
