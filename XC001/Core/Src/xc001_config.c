#include "xc001_config.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

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

static uint32_t ip_to_u32(const uint8_t ip[4])
{
  return ((uint32_t)ip[0] << 24) |
         ((uint32_t)ip[1] << 16) |
         ((uint32_t)ip[2] << 8) |
         (uint32_t)ip[3];
}

static uint8_t is_zero_ip(const uint8_t ip[4])
{
  return (ip == 0 || (ip[0] == 0U && ip[1] == 0U && ip[2] == 0U && ip[3] == 0U)) ? 1U : 0U;
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
  if (!XC001_Config_ValidateNetworkFull(ip, netmask, gateway, udp_port))
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

uint8_t XC001_Config_ValidateNetworkFull(const uint8_t ip[4], const uint8_t netmask[4], const uint8_t gateway[4], uint16_t udp_port)
{
  uint32_t ip_value;
  uint32_t mask_value;
  uint32_t host_part;

  if (ip == 0 || netmask == 0 || udp_port == 0U ||
      !is_valid_host_ip(ip) || !is_valid_netmask(netmask))
  {
    return 0U;
  }

  ip_value = ip_to_u32(ip);
  mask_value = ip_to_u32(netmask);
  host_part = ip_value & ~mask_value;
  if (host_part == 0UL || host_part == (~mask_value))
  {
    return 0U;
  }

  if (!is_zero_ip(gateway))
  {
    uint32_t gateway_value;
    if (!is_valid_host_ip(gateway))
    {
      return 0U;
    }
    gateway_value = ip_to_u32(gateway);
    if ((gateway_value & mask_value) != (ip_value & mask_value))
    {
      return 0U;
    }
  }
  return 1U;
}

uint8_t XC001_Config_ParseIp(const char *text, uint8_t ip[4])
{
  const char *p = text;

  if (text == 0 || ip == 0)
  {
    return 0U;
  }

  while (isspace((unsigned char)*p))
  {
    p++;
  }
  for (uint8_t i = 0U; i < 4U; i++)
  {
    char *end;
    unsigned long value;

    if (!isdigit((unsigned char)*p))
    {
      return 0U;
    }
    value = strtoul(p, &end, 10);
    if (end == p || value > 255UL)
    {
      return 0U;
    }
    ip[i] = (uint8_t)value;
    p = end;
    if (i < 3U)
    {
      if (*p != '.')
      {
        return 0U;
      }
      p++;
    }
  }
  while (isspace((unsigned char)*p))
  {
    p++;
  }
  if (*p != '\0')
  {
    return 0U;
  }
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
