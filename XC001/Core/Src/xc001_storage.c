#include "xc001_storage.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define XC001_STORAGE_MAGIC      0x58433031UL
#define XC001_STORAGE_VERSION    1U
#define XC001_STORAGE_ADDR       0x081E0000UL

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  XC001_NetworkConfig cfg;
  uint32_t crc;
} XC001_StorageRecord;

typedef union
{
  XC001_StorageRecord rec;
  uint32_t words[8];
} XC001_StorageFlashWord;

static uint32_t checksum32(const uint8_t *data, uint32_t len)
{
  uint32_t h = 2166136261UL;

  for (uint32_t i = 0; i < len; i++)
  {
    h ^= data[i];
    h *= 16777619UL;
  }
  return h;
}

uint8_t XC001_Storage_Load(XC001_NetworkConfig *cfg)
{
  const XC001_StorageRecord *rec = (const XC001_StorageRecord *)XC001_STORAGE_ADDR;
  uint32_t crc;

  if (cfg == 0 || rec->magic != XC001_STORAGE_MAGIC || rec->version != XC001_STORAGE_VERSION)
  {
    return 0U;
  }
  crc = checksum32((const uint8_t *)&rec->cfg, sizeof(rec->cfg));
  if (crc != rec->crc)
  {
    return 0U;
  }
  if (rec->cfg.udp_port == 0U || rec->cfg.ip[0] == 0U || rec->cfg.ip[0] >= 224U)
  {
    return 0U;
  }
  *cfg = rec->cfg;
  return 1U;
}

uint8_t XC001_Storage_Save(const XC001_NetworkConfig *cfg)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0;
  XC001_StorageFlashWord block __attribute__((aligned(32)));
  HAL_StatusTypeDef st;

  if (cfg == 0)
  {
    return 0U;
  }
  memset(&block, 0xFF, sizeof(block));
  block.rec.magic = XC001_STORAGE_MAGIC;
  block.rec.version = XC001_STORAGE_VERSION;
  block.rec.cfg = *cfg;
  block.rec.crc = checksum32((const uint8_t *)&block.rec.cfg, sizeof(block.rec.cfg));

  HAL_FLASH_Unlock();
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_2;
  erase.Sector = FLASH_SECTOR_7;
  erase.NbSectors = 1;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  st = HAL_FLASHEx_Erase(&erase, &sector_error);
  if (st == HAL_OK)
  {
    st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, XC001_STORAGE_ADDR, (uint32_t)block.words);
  }
  HAL_FLASH_Lock();
  return (st == HAL_OK) ? 1U : 0U;
}

uint8_t XC001_Storage_Reset(void)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0;
  HAL_StatusTypeDef st;

  HAL_FLASH_Unlock();
  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_2;
  erase.Sector = FLASH_SECTOR_7;
  erase.NbSectors = 1;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  st = HAL_FLASHEx_Erase(&erase, &sector_error);
  HAL_FLASH_Lock();
  return (st == HAL_OK) ? 1U : 0U;
}
