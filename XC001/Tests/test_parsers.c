#include "xc001_config.h"
#include "xc001_utils.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_ip_parser(void)
{
  uint8_t ip[4];

  assert(XC001_Config_ParseIp("192.168.1.10", ip) == 1U);
  assert(memcmp(ip, (uint8_t[]){192U, 168U, 1U, 10U}, sizeof(ip)) == 0);
  assert(XC001_Config_ParseIp("192.168.1.10  ", ip) == 1U);
  assert(XC001_Config_ParseIp("192.168.1.10junk", ip) == 0U);
  assert(XC001_Config_ParseIp("192.168.1", ip) == 0U);
  assert(XC001_Config_ParseIp("256.168.1.10", ip) == 0U);
}

static void test_network_validation(void)
{
  const uint8_t mask[4] = {255U, 255U, 255U, 0U};
  const uint8_t ip[4] = {192U, 168U, 1U, 10U};
  const uint8_t gateway[4] = {192U, 168U, 1U, 1U};
  const uint8_t zero_gateway[4] = {0U, 0U, 0U, 0U};
  const uint8_t wrong_gateway[4] = {192U, 168U, 2U, 1U};
  const uint8_t network[4] = {192U, 168U, 1U, 0U};
  const uint8_t broadcast[4] = {192U, 168U, 1U, 255U};
  const uint8_t bad_mask[4] = {255U, 0U, 255U, 0U};

  assert(XC001_Config_ValidateNetworkFull(ip, mask, gateway, 4000U) == 1U);
  assert(XC001_Config_ValidateNetworkFull(ip, mask, zero_gateway, 4000U) == 1U);
  assert(XC001_Config_ValidateNetworkFull(ip, mask, wrong_gateway, 4000U) == 0U);
  assert(XC001_Config_ValidateNetworkFull(network, mask, gateway, 4000U) == 0U);
  assert(XC001_Config_ValidateNetworkFull(broadcast, mask, gateway, 4000U) == 0U);
  assert(XC001_Config_ValidateNetworkFull(ip, bad_mask, gateway, 4000U) == 0U);
  assert(XC001_Config_ValidateNetworkFull(ip, mask, gateway, 0U) == 0U);
}

static void test_numeric_and_byte_parsers(void)
{
  uint32_t value;
  uint8_t bytes[8];
  uint8_t length;
  char decoded[16];

  assert(XC001_ParseU32("0x1FFFFFFF", 0x1FFFFFFFUL, &value) == 1U);
  assert(value == 0x1FFFFFFFUL);
  assert(XC001_ParseU32("0x20000000", 0x1FFFFFFFUL, &value) == 0U);
  assert(XC001_ParseU32("123junk", 0x1FFFFFFFUL, &value) == 0U);
  assert(XC001_ParseU32("-1", 0x1FFFFFFFUL, &value) == 0U);

  assert(XC001_ParseHexBytes("11 22,FF", bytes, sizeof(bytes), &length) == 1U);
  assert(length == 3U && bytes[0] == 0x11U && bytes[1] == 0x22U && bytes[2] == 0xFFU);
  assert(XC001_ParseHexBytes("11 ZZ", bytes, sizeof(bytes), &length) == 0U);

  assert(XC001_UrlDecode("IP%3D192.168.1.10", decoded, sizeof(decoded)) == 1U);
  assert(strcmp(decoded, "IP=192.168.1.10") == 0);
  assert(XC001_UrlDecode("1234567890123456", decoded, sizeof(decoded)) == 0U);
}

int main(void)
{
  test_ip_parser();
  test_network_validation();
  test_numeric_and_byte_parsers();
  puts("XC001 parser tests passed");
  return 0;
}
