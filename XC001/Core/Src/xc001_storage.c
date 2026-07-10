#include "xc001_storage.h"
#include "xc001_board.h"
#include "stm32h7xx_hal.h"
#include "cmsis_os2.h"
#include <stddef.h>
#include <string.h>

#define XC001_STORAGE_MAGIC        0x58433031UL
#define XC001_STORAGE_VERSION      2U
#define XC001_STORAGE_BASE         0x081C0000UL
#define XC001_STORAGE_LEGACY_ADDR  0x081E0000UL
#define XC001_STORAGE_SECTOR_SIZE  0x00020000UL
#define XC001_STORAGE_SIZE         (2UL * XC001_STORAGE_SECTOR_SIZE)
#define XC001_STORAGE_SLOT_SIZE    32UL
#define XC001_STORAGE_SLOT_COUNT   (XC001_STORAGE_SIZE / XC001_STORAGE_SLOT_SIZE)

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t record_size;
  uint32_t sequence;
  XC001_NetworkConfig cfg;
  uint16_t reserved;
  uint32_t crc;
} XC001_StorageRecord;

typedef struct
{
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  XC001_NetworkConfig cfg;
  uint32_t crc;
} XC001_LegacyRecord;

typedef union
{
  XC001_StorageRecord rec;
  uint32_t words[8];
} XC001_StorageFlashWord;

_Static_assert(sizeof(XC001_StorageFlashWord) == XC001_STORAGE_SLOT_SIZE,
               "storage record must occupy one STM32H7 flash word");

static osMutexId_t s_storage_mutex;

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

static uint8_t lock_storage(void)
{
  return (s_storage_mutex != 0 && osMutexAcquire(s_storage_mutex, osWaitForever) == osOK) ? 1U : 0U;
}

static void unlock_storage(void)
{
  if (s_storage_mutex != 0)
  {
    (void)osMutexRelease(s_storage_mutex);
  }
}

static uint8_t record_is_erased(const XC001_StorageRecord *rec)
{
  const uint32_t *words = (const uint32_t *)rec;

  for (uint32_t i = 0U; i < (XC001_STORAGE_SLOT_SIZE / sizeof(uint32_t)); i++)
  {
    if (words[i] != 0xFFFFFFFFUL)
    {
      return 0U;
    }
  }
  return 1U;
}

static uint8_t record_is_valid(const XC001_StorageRecord *rec)
{
  uint32_t crc;

  if (rec->magic != XC001_STORAGE_MAGIC || rec->version != XC001_STORAGE_VERSION ||
      rec->record_size != sizeof(XC001_StorageRecord))
  {
    return 0U;
  }
  crc = checksum32((const uint8_t *)rec, offsetof(XC001_StorageRecord, crc));
  return (crc == rec->crc &&
          XC001_Config_ValidateNetworkFull(rec->cfg.ip, rec->cfg.netmask,
                                           rec->cfg.gateway, rec->cfg.udp_port)) ? 1U : 0U;
}

static uint8_t legacy_is_valid(const XC001_LegacyRecord *rec)
{
  if (rec->magic != XC001_STORAGE_MAGIC || rec->version != 1U ||
      checksum32((const uint8_t *)&rec->cfg, sizeof(rec->cfg)) != rec->crc)
  {
    return 0U;
  }
  return XC001_Config_ValidateNetworkFull(rec->cfg.ip, rec->cfg.netmask,
                                          rec->cfg.gateway, rec->cfg.udp_port);
}

static void scan_records(const XC001_StorageRecord **latest, uint32_t *empty_addr)
{
  const XC001_StorageRecord *best = 0;

  *empty_addr = 0UL;
  for (uint32_t i = 0U; i < XC001_STORAGE_SLOT_COUNT; i++)
  {
    const XC001_StorageRecord *rec =
        (const XC001_StorageRecord *)(XC001_STORAGE_BASE + (i * XC001_STORAGE_SLOT_SIZE));
    if (*empty_addr == 0UL && record_is_erased(rec))
    {
      *empty_addr = (uint32_t)rec;
    }
    if (record_is_valid(rec) &&
        (best == 0 || (int32_t)(rec->sequence - best->sequence) > 0))
    {
      best = rec;
    }
  }
  *latest = best;
}

static HAL_StatusTypeDef erase_storage_sector(uint32_t address)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0UL;

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_2;
  erase.Sector = (address < XC001_STORAGE_LEGACY_ADDR) ? FLASH_SECTOR_6 : FLASH_SECTOR_7;
  erase.NbSectors = 1U;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  return HAL_FLASHEx_Erase(&erase, &sector_error);
}

void XC001_Storage_Init(void)
{
  if (s_storage_mutex == 0)
  {
    const osMutexAttr_t attr = {.name = "xc001_storage"};
    s_storage_mutex = osMutexNew(&attr);
    if (s_storage_mutex == 0)
    {
      XC001_Board_SetStatusOk(0U);
    }
  }
}

uint8_t XC001_Storage_Load(XC001_NetworkConfig *cfg)
{
  const XC001_StorageRecord *latest;
  const XC001_LegacyRecord *legacy = (const XC001_LegacyRecord *)XC001_STORAGE_LEGACY_ADDR;
  uint32_t empty_addr;
  uint8_t ok = 0U;

  if (cfg == 0 || !lock_storage())
  {
    return 0U;
  }
  scan_records(&latest, &empty_addr);
  if (latest != 0)
  {
    *cfg = latest->cfg;
    ok = 1U;
  }
  else if (legacy_is_valid(legacy))
  {
    *cfg = legacy->cfg;
    ok = 1U;
  }
  unlock_storage();
  return ok;
}

uint8_t XC001_Storage_Save(const XC001_NetworkConfig *cfg)
{
  const XC001_StorageRecord *latest;
  XC001_StorageFlashWord block __attribute__((aligned(32)));
  uint32_t address;
  uint32_t empty_addr;
  HAL_StatusTypeDef st = HAL_ERROR;

  if (cfg == 0 ||
      !XC001_Config_ValidateNetworkFull(cfg->ip, cfg->netmask, cfg->gateway, cfg->udp_port) ||
      !lock_storage())
  {
    return 0U;
  }

  scan_records(&latest, &empty_addr);
  address = empty_addr;
  if (address == 0UL)
  {
    address = (latest != 0 && (uint32_t)latest < XC001_STORAGE_LEGACY_ADDR) ?
              XC001_STORAGE_LEGACY_ADDR : XC001_STORAGE_BASE;
  }

  memset(&block, 0, sizeof(block));
  block.rec.magic = XC001_STORAGE_MAGIC;
  block.rec.version = XC001_STORAGE_VERSION;
  block.rec.record_size = sizeof(XC001_StorageRecord);
  block.rec.sequence = (latest == 0) ? 1UL : (latest->sequence + 1UL);
  block.rec.cfg = *cfg;
  block.rec.crc = checksum32((const uint8_t *)&block.rec, offsetof(XC001_StorageRecord, crc));

  if (HAL_FLASH_Unlock() == HAL_OK)
  {
    st = HAL_OK;
    if (empty_addr == 0UL)
    {
      st = erase_storage_sector(address);
    }
    if (st == HAL_OK)
    {
      st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address,
                             (uint32_t)(uintptr_t)block.words);
    }
    (void)HAL_FLASH_Lock();
  }

  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    uint32_t invalidate_addr = (empty_addr == 0UL) ?
        (address & ~(XC001_STORAGE_SECTOR_SIZE - 1UL)) : address;
    int32_t invalidate_size = (empty_addr == 0UL) ?
        (int32_t)XC001_STORAGE_SECTOR_SIZE : (int32_t)XC001_STORAGE_SLOT_SIZE;
    SCB_InvalidateDCache_by_Addr((void *)invalidate_addr, invalidate_size);
  }
  if (st == HAL_OK && !record_is_valid((const XC001_StorageRecord *)address))
  {
    st = HAL_ERROR;
  }
  if (st != HAL_OK)
  {
    XC001_Board_SetStatusOk(0U);
  }
  unlock_storage();
  return (st == HAL_OK) ? 1U : 0U;
}

uint8_t XC001_Storage_Reset(void)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0UL;
  HAL_StatusTypeDef st = HAL_ERROR;

  if (!lock_storage())
  {
    return 0U;
  }
  if (HAL_FLASH_Unlock() == HAL_OK)
  {
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Banks = FLASH_BANK_2;
    erase.Sector = FLASH_SECTOR_6;
    erase.NbSectors = 2U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    st = HAL_FLASHEx_Erase(&erase, &sector_error);
    (void)HAL_FLASH_Lock();
  }
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_InvalidateDCache_by_Addr((void *)XC001_STORAGE_BASE, (int32_t)XC001_STORAGE_SIZE);
  }
  unlock_storage();
  return (st == HAL_OK) ? 1U : 0U;
}
