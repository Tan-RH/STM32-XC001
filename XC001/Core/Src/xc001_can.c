#include "xc001_can.h"
#include "fdcan.h"
#include "xc001_utils.h"
#include <stdio.h>
#include <string.h>

static uint8_t s_ready;
static uint32_t s_tx_count;
static uint32_t s_rx_count;
static uint32_t s_last_id;
static uint8_t s_last_data[8];
static uint8_t s_last_len;

static uint32_t dlc_from_len(uint8_t len)
{
  static const uint32_t dlc[9] = {
    FDCAN_DLC_BYTES_0, FDCAN_DLC_BYTES_1, FDCAN_DLC_BYTES_2, FDCAN_DLC_BYTES_3,
    FDCAN_DLC_BYTES_4, FDCAN_DLC_BYTES_5, FDCAN_DLC_BYTES_6, FDCAN_DLC_BYTES_7,
    FDCAN_DLC_BYTES_8
  };
  return dlc[(len <= 8U) ? len : 8U];
}

static uint8_t len_from_dlc(uint32_t dlc)
{
  if (dlc >= FDCAN_DLC_BYTES_8)
  {
    return 8U;
  }
  return (uint8_t)(dlc >> 16);
}

void XC001_CAN_Init(void)
{
  FDCAN_FilterTypeDef filter = {0};

  HAL_FDCAN_Stop(&hfdcan1);
  HAL_FDCAN_DeInit(&hfdcan1);
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 8;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_8;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 8;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_8;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    s_ready = 0U;
    return;
  }

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0x000;
  filter.FilterID2 = 0x000;
  (void)HAL_FDCAN_ConfigFilter(&hfdcan1, &filter);
  (void)HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
  s_ready = (HAL_FDCAN_Start(&hfdcan1) == HAL_OK) ? 1U : 0U;
}

void XC001_CAN_Task(void)
{
  while (s_ready != 0U && HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0U)
  {
    FDCAN_RxHeaderTypeDef hdr;
    uint8_t data[8] = {0};
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &hdr, data) == HAL_OK)
    {
      s_last_id = hdr.Identifier;
      s_last_len = len_from_dlc(hdr.DataLength);
      memcpy(s_last_data, data, s_last_len);
      s_rx_count++;
    }
  }
}

uint8_t XC001_CAN_Send(uint32_t id, const uint8_t *data, uint8_t len)
{
  FDCAN_TxHeaderTypeDef hdr = {0};
  uint8_t tx[8] = {0};

  if (s_ready == 0U || data == 0 || len > 8U)
  {
    return 0U;
  }
  memcpy(tx, data, len);
  hdr.Identifier = id;
  hdr.IdType = (id > 0x7FFUL) ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
  hdr.TxFrameType = FDCAN_DATA_FRAME;
  hdr.DataLength = dlc_from_len(len);
  hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  hdr.BitRateSwitch = FDCAN_BRS_OFF;
  hdr.FDFormat = FDCAN_CLASSIC_CAN;
  hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  hdr.MessageMarker = 0;
  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &hdr, tx) == HAL_OK)
  {
    s_tx_count++;
    return 1U;
  }
  return 0U;
}

void XC001_CAN_Status(char *out, size_t out_size)
{
  snprintf(out, out_size, "CAN1:READY=%u,TX=%lu,RX=%lu", s_ready, (unsigned long)s_tx_count, (unsigned long)s_rx_count);
}

void XC001_CAN_LastRx(char *out, size_t out_size)
{
  char hex[32];

  XC001_FormatHexBytes(s_last_data, s_last_len, hex, sizeof(hex));
  if (s_rx_count == 0U)
  {
    snprintf(out, out_size, "EMPTY");
  }
  else
  {
    snprintf(out, out_size, "ID=0x%lX,LEN=%u,DATA=%s", (unsigned long)s_last_id, s_last_len, hex);
  }
}
