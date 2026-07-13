#ifndef XC001_UPDATE_H
#define XC001_UPDATE_H

#include <stddef.h>
#include <stdint.h>

#define XC001_STAGE0_ADDRESS          0x08000000UL
#define XC001_APPLICATION_ADDRESS     0x08020000UL
#define XC001_APPLICATION_MAX_SIZE    0x000A0000UL
#define XC001_STAGING_ADDRESS         0x08100000UL
#define XC001_UPDATE_META_ADDRESS     0x081A0000UL
#define XC001_CONFIG_ADDRESS          0x081C0000UL
#define XC001_BOOTLOADER_ADDRESS      0x081E0000UL
#define XC001_FLASH_SECTOR_SIZE       0x00020000UL
#define XC001_FLASH_WORD_SIZE         32UL

#define XC001_UPDATE_MAGIC            0x58555044UL
#define XC001_UPDATE_RECORD_VERSION   1U
#define XC001_UPDATE_STATE_PENDING    1U
#define XC001_UPDATE_STATE_APPLIED    2U

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t state;
  uint32_t sequence;
  uint32_t image_size;
  uint32_t image_crc;
  uint32_t reserved[2];
  uint32_t record_crc;
} XC001_UpdateRecord;

static inline uint32_t XC001_Update_Crc32Update(uint32_t crc, const uint8_t *data, size_t length)
{
  while (length-- > 0U)
  {
    crc ^= *data++;
    for (uint32_t bit = 0U; bit < 8U; bit++)
    {
      crc = (crc >> 1U) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
    }
  }
  return crc;
}

static inline uint32_t XC001_Update_Crc32(const void *data, size_t length)
{
  return XC001_Update_Crc32Update(0xFFFFFFFFUL, (const uint8_t *)data, length) ^ 0xFFFFFFFFUL;
}

static inline uint8_t XC001_Update_IsStackPointerValid(uint32_t stack_pointer)
{
  uint8_t in_dtcm = (stack_pointer > 0x20000000UL && stack_pointer <= 0x20020000UL) ? 1U : 0U;
  uint8_t in_axi = (stack_pointer > 0x24000000UL && stack_pointer <= 0x24080000UL) ? 1U : 0U;
  uint8_t in_d2 = (stack_pointer > 0x30000000UL && stack_pointer <= 0x30048000UL) ? 1U : 0U;
  uint8_t in_d3 = (stack_pointer > 0x38000000UL && stack_pointer <= 0x38010000UL) ? 1U : 0U;
  return (((stack_pointer & 0x7UL) == 0UL) && (in_dtcm || in_axi || in_d2 || in_d3)) ? 1U : 0U;
}

static inline uint8_t XC001_Update_IsVectorValid(const uint32_t vectors[2],
                                                  uint32_t image_size)
{
  uint32_t reset_handler;

  if (vectors == 0 || image_size < 8U || image_size > XC001_APPLICATION_MAX_SIZE ||
      !XC001_Update_IsStackPointerValid(vectors[0]) || (vectors[1] & 1UL) == 0UL)
  {
    return 0U;
  }
  reset_handler = vectors[1] & ~1UL;
  return (reset_handler >= XC001_APPLICATION_ADDRESS &&
          reset_handler < (XC001_APPLICATION_ADDRESS + image_size)) ? 1U : 0U;
}

static inline uint8_t XC001_Update_RecordValid(const XC001_UpdateRecord *record)
{
  if (record == 0 || record->magic != XC001_UPDATE_MAGIC ||
      record->version != XC001_UPDATE_RECORD_VERSION ||
      (record->state != XC001_UPDATE_STATE_PENDING &&
       record->state != XC001_UPDATE_STATE_APPLIED) ||
      record->image_size == 0U || record->image_size > XC001_APPLICATION_MAX_SIZE)
  {
    return 0U;
  }
  return (XC001_Update_Crc32(record, offsetof(XC001_UpdateRecord, record_crc)) ==
          record->record_crc) ? 1U : 0U;
}

#endif
