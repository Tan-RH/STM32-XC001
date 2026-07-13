#include "xc001_config.h"
#include "xc001_multipart.h"
#include "xc001_utils.h"
#include "xc001_update.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
  uint8_t data[64];
  uint32_t length;
  uint8_t begin_count;
  uint8_t end_count;
} TestMultipartSink;

static uint8_t test_file_begin(void *ctx, const char *filename)
{
  TestMultipartSink *sink = (TestMultipartSink *)ctx;

  assert(filename != 0);
  sink->begin_count++;
  return 1U;
}

static uint8_t test_file_data(void *ctx, const uint8_t *data, uint32_t length)
{
  TestMultipartSink *sink = (TestMultipartSink *)ctx;

  assert(sink->length + length <= sizeof(sink->data));
  memcpy(&sink->data[sink->length], data, length);
  sink->length += length;
  return 1U;
}

static void test_file_end(void *ctx)
{
  TestMultipartSink *sink = (TestMultipartSink *)ctx;

  sink->end_count++;
}

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

static void test_firmware_metadata(void)
{
  static const uint8_t crc_text[] = "123456789";
  uint32_t vectors[2] = {0x24080000UL, XC001_APPLICATION_ADDRESS + 0x101UL};
  XC001_UpdateRecord record = {0};

  assert(XC001_Update_Crc32(crc_text, sizeof(crc_text) - 1U) == 0xCBF43926UL);
  assert(XC001_Update_IsVectorValid(vectors, 0x200U) == 1U);
  vectors[1] = XC001_STAGE0_ADDRESS + 1U;
  assert(XC001_Update_IsVectorValid(vectors, 0x200U) == 0U);

  record.magic = XC001_UPDATE_MAGIC;
  record.version = XC001_UPDATE_RECORD_VERSION;
  record.state = XC001_UPDATE_STATE_PENDING;
  record.sequence = 1UL;
  record.image_size = 0x200U;
  record.image_crc = 0x12345678UL;
  record.record_crc = XC001_Update_Crc32(&record,
                                         offsetof(XC001_UpdateRecord, record_crc));
  assert(XC001_Update_RecordValid(&record) == 1U);
  record.image_crc ^= 1UL;
  assert(XC001_Update_RecordValid(&record) == 0U);
}

static void test_multipart_parser(void)
{
  static const char body[] =
      "------xc001\r\n"
      "Content-Disposition: form-data; name=\"note\"\r\n"
      "\r\n"
      "ignored\r\n"
      "------xc001\r\n"
      "Content-Disposition: form-data; name=\"file\"; filename=\"fw.bin\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "\x01\x02""ABC\r\n"
      "------xc001--\r\n";
  TestMultipartSink sink = {0};
  XC001_MultipartParser parser;
  XC001_MultipartCallbacks callbacks = {
    .on_file_begin = test_file_begin,
    .on_file_data = test_file_data,
    .on_file_end = test_file_end,
    .ctx = &sink
  };

  assert(XC001_Multipart_Init(&parser, "----xc001", &callbacks) == 1U);
  for (size_t i = 0U; i < sizeof(body) - 1U; i++)
  {
    assert(XC001_Multipart_Execute(&parser, (const uint8_t *)&body[i], 1U) == 1U);
  }
  assert(XC001_Multipart_IsDone(&parser) == 1U);
  assert(XC001_Multipart_SawFile(&parser) == 1U);
  assert(XC001_Multipart_FileBytes(&parser) == 5U);
  assert(sink.begin_count == 1U);
  assert(sink.end_count == 1U);
  assert(sink.length == 5U);
  assert(memcmp(sink.data, (const uint8_t *)"\x01\x02""ABC", 5U) == 0);
}

static void test_multipart_parser_chaos_fw_field(void)
{
  static const char body[] =
      "--chaos-boundary\r\n"
      "Content-Disposition: form-data; name=\"fw\"; filename=\"XC001.bin\"\r\n"
      "Content-Type: application/octet-stream\r\n"
      "\r\n"
      "BIN\0\1"
      "\r\n--chaos-boundary--\r\n";
  TestMultipartSink sink = {0};
  XC001_MultipartParser parser;
  XC001_MultipartCallbacks callbacks = {
    .on_file_begin = test_file_begin,
    .on_file_data = test_file_data,
    .on_file_end = test_file_end,
    .ctx = &sink
  };

  assert(XC001_Multipart_Init(&parser, "chaos-boundary", &callbacks) == 1U);
  assert(XC001_Multipart_Execute(&parser, (const uint8_t *)body,
                                 (uint32_t)(sizeof(body) - 1U)) == 1U);
  assert(XC001_Multipart_IsDone(&parser) == 1U);
  assert(XC001_Multipart_SawFile(&parser) == 1U);
  assert(XC001_Multipart_FileBytes(&parser) == 5U);
  assert(sink.begin_count == 1U);
  assert(sink.end_count == 1U);
  assert(sink.length == 5U);
  assert(memcmp(sink.data, (const uint8_t *)"BIN\0\1", 5U) == 0);
}
int main(void)
{
  test_ip_parser();
  test_network_validation();
  test_numeric_and_byte_parsers();
  test_firmware_metadata();
  test_multipart_parser();
  test_multipart_parser_chaos_fw_field();
  puts("XC001 parser tests passed");
  return 0;
}
