#include "xc001_config.h"
#include <stdio.h>

XC001_NetworkConfig XC001_NetConfig;

static uint8_t is_valid_host_ip(const uint8_t ip[4])
{
  if (ip == 0)
  {
    return 0U;
  }
  if (ip[0] == 0U || ip[0] >= 224U || ip[3] == 0U || ip[3] == 255U)
  {
    return 0U;
  }
  return 1U;
}

static uint8_t is_valid_netmask(const uint8_t mask[4])
{
  uint32_t value;
  uint8_t zero_seen = 0U;

  if (mask == 0)
  {
    return 0U;
  }
  value = ((uint32_t)mask[0] << 24) |
          ((uint32_t)mask[1] << 16) |
          ((uint32_t)mask[2] << 8) |
          (uint32_t)mask[3];
  if (value == 0UL || value == 0xFFFFFFFFUL)
  {
    return 0U;
  }
  for (int8_t bit = 31; bit >= 0; bit--)
  {
    uint8_t one = ((value & (1UL << (uint8_t)bit)) != 0UL) ? 1U : 0U;
    if (one == 0U)
    {
      zero_seen = 1U;
    }
    else if (zero_seen != 0U)
    {
      return 0U;
    }
  }
  return 1U;
}

void XC001_Config_LoadDefaults(void)
{
  XC001_NetConfig.ip[0] = XC001_DEFAULT_IP0;
  XC001_NetConfig.ip[1] = XC001_DEFAULT_IP1;
  XC001_NetConfig.ip[2] = XC001_DEFAULT_IP2;
  XC001_NetConfig.ip[3] = XC001_DEFAULT_IP3;
  XC001_NetConfig.gateway[0] = XC001_DEFAULT_GW0;
  XC001_NetConfig.gateway[1] = XC001_DEFAULT_GW1;
  XC001_NetConfig.gateway[2] = XC001_DEFAULT_GW2;
  XC001_NetConfig.gateway[3] = XC001_DEFAULT_GW3;
  XC001_NetConfig.netmask[0] = XC001_DEFAULT_MASK0;
  XC001_NetConfig.netmask[1] = XC001_DEFAULT_MASK1;
  XC001_NetConfig.netmask[2] = XC001_DEFAULT_MASK2;
  XC001_NetConfig.netmask[3] = XC001_DEFAULT_MASK3;
  XC001_NetConfig.udp_port = XC001_DEFAULT_UDP_PORT;
}

uint8_t XC001_Config_SetNetwork(const uint8_t ip[4], uint16_t udp_port)
{
  return XC001_Config_SetNetworkFull(ip, XC001_NetConfig.netmask, XC001_NetConfig.gateway, udp_port);
}

uint8_t XC001_Config_SetNetworkFull(const uint8_t ip[4], const uint8_t netmask[4], const uint8_t gateway[4], uint16_t udp_port)
{
  if (ip == 0 || udp_port == 0U)
  {
    return 0U;
  }
  if (!is_valid_host_ip(ip) || !is_valid_netmask(netmask))
  {
    return 0U;
  }

  for (uint8_t i = 0; i < 4U; i++)
  {
    XC001_NetConfig.ip[i] = ip[i];
    XC001_NetConfig.netmask[i] = netmask[i];
    XC001_NetConfig.gateway[i] = (gateway == 0) ? 0U : gateway[i];
  }
  XC001_NetConfig.udp_port = udp_port;
  return 1U;
}

uint8_t XC001_Config_ParseIp(const char *text, uint8_t ip[4])
{
  unsigned int a, b, c, d;

  if (text == 0 || ip == 0)
  {
    return 0U;
  }
  if (sscanf(text, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
  {
    return 0U;
  }
  if (a > 255U || b > 255U || c > 255U || d > 255U)
  {
    return 0U;
  }

  ip[0] = (uint8_t)a;
  ip[1] = (uint8_t)b;
  ip[2] = (uint8_t)c;
  ip[3] = (uint8_t)d;
  return 1U;
}

void XC001_Config_FormatIp(const uint8_t ip[4], char *out, size_t out_size)
{
  if (out == 0 || out_size == 0U)
  {
    return;
  }
  if (ip == 0)
  {
    out[0] = '\0';
    return;
  }
  snprintf(out, out_size, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}
