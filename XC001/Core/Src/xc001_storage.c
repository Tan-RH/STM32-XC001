#include "xc001_storage.h"
#include "xc001_board.h"
#include "xc001_update.h"
#include "stm32h7xx_hal.h"
#include "cmsis_os2.h"
#include <stddef.h>
#include <string.h>

#define XC001_STORAGE_MAGIC        0x58433031UL
#define XC001_STORAGE_VERSION      2U
#define XC001_STORAGE_SLOT_SIZE    XC001_FLASH_WORD_SIZE
#define XC001_STORAGE_SLOT_COUNT   (XC001_FLASH_SECTOR_SIZE / XC001_STORAGE_SLOT_SIZE)
#define XC001_UPDATE_SLOT_COUNT    (XC001_FLASH_SECTOR_SIZE / XC001_FLASH_WORD_SIZE)

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

typedef union
{
  XC001_StorageRecord rec;
  uint32_t words[8];
} XC001_StorageFlashWord;

typedef union
{
  XC001_UpdateRecord rec;
  uint32_t words[8];
} XC001_UpdateFlashWord;

typedef struct
{
  uint8_t active;
  uint32_t expected_size;
  uint32_t written;
  uint32_t address;
  uint32_t crc;
  uint8_t fill;
  uint8_t flashword[XC001_FLASH_WORD_SIZE] __attribute__((aligned(32)));
} XC001_FirmwareWriter;

_Static_assert(sizeof(XC001_StorageFlashWord) == XC001_FLASH_WORD_SIZE,
               "configuration record must occupy one flash word");
_Static_assert(sizeof(XC001_UpdateFlashWord) == XC001_FLASH_WORD_SIZE,
               "update record must occupy one flash word");

static osMutexId_t s_storage_mutex;
static XC001_FirmwareWriter s_firmware;

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

static uint8_t flashword_is_erased(const void *address)
{
  const uint32_t *words = (const uint32_t *)address;

  for (uint32_t i = 0U; i < (XC001_FLASH_WORD_SIZE / sizeof(uint32_t)); i++)
  {
    if (words[i] != 0xFFFFFFFFUL)
    {
      return 0U;
    }
  }
  return 1U;
}

static HAL_StatusTypeDef erase_bank2(uint32_t first_sector, uint32_t sector_count)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t sector_error = 0UL;

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Banks = FLASH_BANK_2;
  erase.Sector = first_sector;
  erase.NbSectors = sector_count;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  return HAL_FLASHEx_Erase(&erase, &sector_error);
}

static uint8_t config_record_valid(const XC001_StorageRecord *rec)
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

static void scan_config(const XC001_StorageRecord **latest, uint32_t *empty_address)
{
  const XC001_StorageRecord *best = 0;

  *empty_address = 0UL;
  for (uint32_t i = 0U; i < XC001_STORAGE_SLOT_COUNT; i++)
  {
    const XC001_StorageRecord *rec =
        (const XC001_StorageRecord *)(XC001_CONFIG_ADDRESS + (i * XC001_STORAGE_SLOT_SIZE));
    if (*empty_address == 0UL && flashword_is_erased(rec))
    {
      *empty_address = (uint32_t)rec;
    }
    if (config_record_valid(rec) &&
        (best == 0 || (int32_t)(rec->sequence - best->sequence) > 0))
    {
      best = rec;
    }
  }
  *latest = best;
}

static void scan_update(const XC001_UpdateRecord **latest, uint32_t *empty_address)
{
  const XC001_UpdateRecord *best = 0;

  *empty_address = 0UL;
  for (uint32_t i = 0U; i < XC001_UPDATE_SLOT_COUNT; i++)
  {
    const XC001_UpdateRecord *rec =
        (const XC001_UpdateRecord *)(XC001_UPDATE_META_ADDRESS + (i * XC001_FLASH_WORD_SIZE));
    if (*empty_address == 0UL && flashword_is_erased(rec))
    {
      *empty_address = (uint32_t)rec;
    }
    if (XC001_Update_RecordValid(rec) &&
        (best == 0 || (int32_t)(rec->sequence - best->sequence) > 0))
    {
      best = rec;
    }
  }
  *latest = best;
}

static HAL_StatusTypeDef append_update_record(uint16_t state, uint32_t sequence,
                                               uint32_t image_size, uint32_t image_crc)
{
  const XC001_UpdateRecord *latest;
  XC001_UpdateFlashWord block __attribute__((aligned(32)));
  uint32_t address;

  scan_update(&latest, &address);
  if (address == 0UL)
  {
    return HAL_ERROR;
  }
  memset(&block, 0, sizeof(block));
  block.rec.magic = XC001_UPDATE_MAGIC;
  block.rec.version = XC001_UPDATE_RECORD_VERSION;
  block.rec.state = state;
  block.rec.sequence = sequence;
  block.rec.image_size = image_size;
  block.rec.image_crc = image_crc;
  block.rec.record_crc = XC001_Update_Crc32(&block.rec,
                                             offsetof(XC001_UpdateRecord, record_crc));
  return HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address,
                           (uint32_t)(uintptr_t)block.words);
}

static uint8_t firmware_program_word(void)
{
  HAL_StatusTypeDef status;

  status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, s_firmware.address,
                             (uint32_t)(uintptr_t)s_firmware.flashword);
  if (status != HAL_OK)
  {
    return 0U;
  }
  s_firmware.address += XC001_FLASH_WORD_SIZE;
  s_firmware.fill = 0U;
  memset(s_firmware.flashword, 0xFF, sizeof(s_firmware.flashword));
  return 1U;
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
  uint32_t empty_address;
  uint8_t ok = 0U;

  if (cfg == 0 || !lock_storage())
  {
    return 0U;
  }
  scan_config(&latest, &empty_address);
  if (latest != 0)
  {
    *cfg = latest->cfg;
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
  uint32_t next_sequence;
  uint8_t sector_erased = 0U;
  HAL_StatusTypeDef status = HAL_ERROR;

  if (cfg == 0 ||
      !XC001_Config_ValidateNetworkFull(cfg->ip, cfg->netmask, cfg->gateway, cfg->udp_port) ||
      !lock_storage())
  {
    return 0U;
  }

  scan_config(&latest, &address);
  next_sequence = (latest == 0) ? 1UL : (latest->sequence + 1UL);
  if (HAL_FLASH_Unlock() == HAL_OK)
  {
    status = HAL_OK;
    if (address == 0UL)
    {
      status = erase_bank2(FLASH_SECTOR_6, 1U);
      address = XC001_CONFIG_ADDRESS;
      sector_erased = 1U;
    }
    if (status == HAL_OK)
    {
      memset(&block, 0, sizeof(block));
      block.rec.magic = XC001_STORAGE_MAGIC;
      block.rec.version = XC001_STORAGE_VERSION;
      block.rec.record_size = sizeof(XC001_StorageRecord);
      block.rec.sequence = next_sequence;
      block.rec.cfg = *cfg;
      block.rec.crc = checksum32((const uint8_t *)&block.rec,
                                 offsetof(XC001_StorageRecord, crc));
      status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address,
                                 (uint32_t)(uintptr_t)block.words);
    }
    (void)HAL_FLASH_Lock();
  }

  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_InvalidateDCache_by_Addr((void *)(sector_erased ? XC001_CONFIG_ADDRESS : address),
                                 sector_erased ? (int32_t)XC001_FLASH_SECTOR_SIZE :
                                                  (int32_t)XC001_FLASH_WORD_SIZE);
  }
  if (status == HAL_OK && !config_record_valid((const XC001_StorageRecord *)address))
  {
    status = HAL_ERROR;
  }
  if (status != HAL_OK)
  {
    XC001_Board_SetStatusOk(0U);
  }
  unlock_storage();
  return (status == HAL_OK) ? 1U : 0U;
}

uint8_t XC001_Storage_Reset(void)
{
  HAL_StatusTypeDef status = HAL_ERROR;

  if (!lock_storage())
  {
    return 0U;
  }
  if (HAL_FLASH_Unlock() == HAL_OK)
  {
    status = erase_bank2(FLASH_SECTOR_6, 1U);
    (void)HAL_FLASH_Lock();
  }
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_InvalidateDCache_by_Addr((void *)XC001_CONFIG_ADDRESS,
                                 (int32_t)XC001_FLASH_SECTOR_SIZE);
  }
  unlock_storage();
  return (status == HAL_OK) ? 1U : 0U;
}

uint32_t XC001_Storage_FirmwareMaxSize(void)
{
  return XC001_APPLICATION_MAX_SIZE;
}

uint8_t XC001_Storage_FirmwareBegin(uint32_t image_size)
{
  if (image_size < 8U || image_size > XC001_APPLICATION_MAX_SIZE ||
      s_firmware.active != 0U || !lock_storage())
  {
    return 0U;
  }
  memset(&s_firmware, 0, sizeof(s_firmware));
  memset(s_firmware.flashword, 0xFF, sizeof(s_firmware.flashword));
  if (HAL_FLASH_Unlock() != HAL_OK || erase_bank2(FLASH_SECTOR_0, 6U) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    unlock_storage();
    XC001_Board_SetStatusOk(0U);
    return 0U;
  }
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_InvalidateDCache_by_Addr((void *)XC001_STAGING_ADDRESS,
                                 (int32_t)(6UL * XC001_FLASH_SECTOR_SIZE));
  }
  s_firmware.active = 1U;
  s_firmware.expected_size = image_size;
  s_firmware.address = XC001_STAGING_ADDRESS;
  s_firmware.crc = 0xFFFFFFFFUL;
  return 1U;
}

uint8_t XC001_Storage_FirmwareWrite(const uint8_t *data, uint32_t length)
{
  if (s_firmware.active == 0U || data == 0 ||
      length > (s_firmware.expected_size - s_firmware.written))
  {
    return 0U;
  }
  s_firmware.crc = XC001_Update_Crc32Update(s_firmware.crc, data, length);
  while (length > 0U)
  {
    uint32_t room = XC001_FLASH_WORD_SIZE - s_firmware.fill;
    uint32_t copy_length = (length < room) ? length : room;

    memcpy(&s_firmware.flashword[s_firmware.fill], data, copy_length);
    s_firmware.fill = (uint8_t)(s_firmware.fill + copy_length);
    s_firmware.written += copy_length;
    data += copy_length;
    length -= copy_length;
    if (s_firmware.fill == XC001_FLASH_WORD_SIZE && !firmware_program_word())
    {
      return 0U;
    }
  }
  return 1U;
}

uint8_t XC001_Storage_FirmwareFinishWithSize(uint32_t image_size, uint32_t *image_crc)
{
  if (s_firmware.active == 0U || image_size < 8U ||
      image_size > XC001_APPLICATION_MAX_SIZE || image_size != s_firmware.written)
  {
    XC001_Storage_FirmwareAbort();
    return 0U;
  }
  s_firmware.expected_size = image_size;
  return XC001_Storage_FirmwareFinish(image_crc);
}
uint8_t XC001_Storage_FirmwareFinish(uint32_t *image_crc)
{
  const uint32_t *vectors = (const uint32_t *)XC001_STAGING_ADDRESS;
  uint32_t crc;
  HAL_StatusTypeDef status;

  if (s_firmware.active == 0U || s_firmware.written != s_firmware.expected_size)
  {
    XC001_Storage_FirmwareAbort();
    return 0U;
  }
  if (s_firmware.fill > 0U && !firmware_program_word())
  {
    XC001_Storage_FirmwareAbort();
    return 0U;
  }
  if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U)
  {
    SCB_InvalidateDCache_by_Addr((void *)XC001_STAGING_ADDRESS,
                                 (int32_t)s_firmware.expected_size);
  }
  crc = s_firmware.crc ^ 0xFFFFFFFFUL;
  if (!XC001_Update_IsVectorValid(vectors, s_firmware.expected_size) ||
      XC001_Update_Crc32((const void *)XC001_STAGING_ADDRESS,
                         s_firmware.expected_size) != crc)
  {
    XC001_Storage_FirmwareAbort();
    return 0U;
  }
  status = append_update_record(XC001_UPDATE_STATE_PENDING, 1UL,
                                s_firmware.expected_size, crc);
  (void)HAL_FLASH_Lock();
  s_firmware.active = 0U;
  unlock_storage();
  if (image_crc != 0)
  {
    *image_crc = crc;
  }
  return (status == HAL_OK) ? 1U : 0U;
}

void XC001_Storage_FirmwareAbort(void)
{
  if (s_firmware.active != 0U)
  {
    (void)HAL_FLASH_Lock();
    s_firmware.active = 0U;
    unlock_storage();
  }
}

uint8_t XC001_Storage_ConfirmRunningFirmware(void)
{
  const XC001_UpdateRecord *latest;
  uint32_t empty_address;
  HAL_StatusTypeDef status = HAL_OK;

  if (!lock_storage())
  {
    return 0U;
  }
  scan_update(&latest, &empty_address);
  if (latest != 0 && latest->state == XC001_UPDATE_STATE_PENDING)
  {
    const uint32_t *vectors = (const uint32_t *)XC001_APPLICATION_ADDRESS;
    if (!XC001_Update_IsVectorValid(vectors, latest->image_size) ||
        XC001_Update_Crc32((const void *)XC001_APPLICATION_ADDRESS,
                           latest->image_size) != latest->image_crc ||
        HAL_FLASH_Unlock() != HAL_OK)
    {
      status = HAL_ERROR;
    }
    else
    {
      status = append_update_record(XC001_UPDATE_STATE_APPLIED,
                                    latest->sequence + 1UL,
                                    latest->image_size, latest->image_crc);
      (void)HAL_FLASH_Lock();
    }
  }
  unlock_storage();
  return (status == HAL_OK) ? 1U : 0U;
}
