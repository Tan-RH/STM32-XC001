#include "xc001_can.h"
#include "fdcan.h"
#include "xc001_utils.h"
#include "xc001_board.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

static uint8_t s_ready;
static uint32_t s_tx_count;
static uint32_t s_rx_count;
static uint32_t s_last_id;
static uint8_t s_last_data[64];
static uint8_t s_last_len;
static uint32_t s_power_ok_count;
static uint32_t s_power_timeout_count;

typedef struct
{
  uint8_t active;
  uint8_t complete;
  uint8_t node_id;
  XC001_CAN_RxPower result;
} XC001_CAN_PowerTransaction;

static XC001_CAN_PowerTransaction s_power_transaction;

static uint32_t dlc_from_len(uint8_t len)
{
  static const uint32_t dlc[16] = {
    FDCAN_DLC_BYTES_0,  FDCAN_DLC_BYTES_1,  FDCAN_DLC_BYTES_2,  FDCAN_DLC_BYTES_3,
    FDCAN_DLC_BYTES_4,  FDCAN_DLC_BYTES_5,  FDCAN_DLC_BYTES_6,  FDCAN_DLC_BYTES_7,
    FDCAN_DLC_BYTES_8,  FDCAN_DLC_BYTES_12, FDCAN_DLC_BYTES_16, FDCAN_DLC_BYTES_20,
    FDCAN_DLC_BYTES_24, FDCAN_DLC_BYTES_32, FDCAN_DLC_BYTES_48, FDCAN_DLC_BYTES_64
  };
  static const uint8_t lengths[16] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U
  };

  for (uint8_t i = 0U; i < 16U; i++)
  {
    if (len <= lengths[i])
    {
      return dlc[i];
    }
  }
  return FDCAN_DLC_BYTES_64;
}

static uint8_t len_from_dlc(uint32_t dlc)
{
  static const uint8_t lengths[16] = {
    0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 12U, 16U, 20U, 24U, 32U, 48U, 64U
  };

  if (dlc > FDCAN_DLC_BYTES_64)
  {
    return 0U;
  }
  return lengths[dlc];
}

void XC001_CAN_Init(void)
{
  FDCAN_FilterTypeDef filter = {0};

  HAL_FDCAN_Stop(&hfdcan1);
  HAL_FDCAN_DeInit(&hfdcan1);
  hfdcan1.Init.FrameFormat = FDCAN_FRAME_FD_NO_BRS;
  hfdcan1.Init.StdFiltersNbr = 1;
  hfdcan1.Init.ExtFiltersNbr = 0;
  hfdcan1.Init.RxFifo0ElmtsNbr = 8;
  hfdcan1.Init.RxFifo0ElmtSize = FDCAN_DATA_BYTES_64;
  hfdcan1.Init.TxFifoQueueElmtsNbr = 8;
  hfdcan1.Init.TxFifoQueueMode = FDCAN_TX_FIFO_OPERATION;
  hfdcan1.Init.TxElmtSize = FDCAN_DATA_BYTES_64;
  if (HAL_FDCAN_Init(&hfdcan1) != HAL_OK)
  {
    s_ready = 0U;
    XC001_Board_SetStatusOk(0U);
    return;
  }

  filter.IdType = FDCAN_STANDARD_ID;
  filter.FilterIndex = 0;
  filter.FilterType = FDCAN_FILTER_MASK;
  filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  filter.FilterID1 = 0x000;
  filter.FilterID2 = 0x000;
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &filter) != HAL_OK ||
      HAL_FDCAN_ConfigGlobalFilter(&hfdcan1, FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_ACCEPT_IN_RX_FIFO0,
                                   FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK)
  {
    s_ready = 0U;
    XC001_Board_SetStatusOk(0U);
    return;
  }
  s_ready = (HAL_FDCAN_Start(&hfdcan1) == HAL_OK) ? 1U : 0U;
  memset(&s_power_transaction, 0, sizeof(s_power_transaction));
  if (s_ready == 0U)
  {
    XC001_Board_SetStatusOk(0U);
  }
}

void XC001_CAN_Task(void)
{
  uint32_t budget = 32U;

  while (budget-- > 0U && s_ready != 0U &&
         HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0U)
  {
    FDCAN_RxHeaderTypeDef hdr;
    uint8_t data[64] = {0};
    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &hdr, data) == HAL_OK)
    {
      uint8_t len = len_from_dlc(hdr.DataLength);

      taskENTER_CRITICAL();
      s_last_id = hdr.Identifier;
      s_last_len = len;
      memcpy(s_last_data, data, len);
      s_rx_count++;
      if (s_power_transaction.active != 0U &&
          hdr.IdType == FDCAN_STANDARD_ID &&
          hdr.Identifier == s_power_transaction.node_id &&
          len >= 8U && data[0] == 0x1BU)
      {
        s_power_transaction.result.device_status = data[1];
        s_power_transaction.result.h_power_cdbm =
            (int16_t)(((uint16_t)data[2] << 8) | data[3]);
        s_power_transaction.result.v_power_cdbm =
            (int16_t)(((uint16_t)data[4] << 8) | data[5]);
        s_power_transaction.result.sample_mode = data[6];
        s_power_transaction.result.sample_state = data[7];
        s_power_transaction.complete = 1U;
      }
      taskEXIT_CRITICAL();
    }
  }
}

uint8_t XC001_CAN_Send(uint32_t id, const uint8_t *data, uint8_t len)
{
  FDCAN_TxHeaderTypeDef hdr = {0};
  uint8_t tx[64] = {0};

  if (s_ready == 0U || data == 0 || len > 64U || id > 0x1FFFFFFFUL)
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
  hdr.FDFormat = (len > 8U) ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
  hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
  hdr.MessageMarker = 0;
  if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &hdr, tx) == HAL_OK)
  {
    taskENTER_CRITICAL();
    s_tx_count++;
    taskEXIT_CRITICAL();
    return 1U;
  }
  return 0U;
}

XC001_CAN_QueryResult XC001_CAN_QueryRxPower(uint8_t node_id,
                                             uint32_t frequency_khz,
                                             uint8_t sample_time_ms,
                                             uint8_t sample_mode,
                                             uint16_t trigger_threshold,
                                             uint16_t trigger_timeout_ms,
                                             uint8_t antenna_compensation,
                                             XC001_CAN_RxPower *result)
{
  uint8_t request[12] = {0};
  uint32_t started_at;
  uint32_t wait_ms;

  if (s_ready == 0U)
  {
    return XC001_CAN_QUERY_NOT_READY;
  }
  if (result == 0 || node_id < 1U || node_id > 40U ||
      frequency_khz > 0xFFFFFFUL || sample_time_ms == 0U ||
      sample_mode > 2U || antenna_compensation > 1U)
  {
    return XC001_CAN_QUERY_SEND_FAILED;
  }

  taskENTER_CRITICAL();
  if (s_power_transaction.active != 0U)
  {
    taskEXIT_CRITICAL();
    return XC001_CAN_QUERY_BUSY;
  }
  memset(&s_power_transaction, 0, sizeof(s_power_transaction));
  s_power_transaction.active = 1U;
  s_power_transaction.node_id = node_id;
  s_power_transaction.result.node_id = node_id;
  s_power_transaction.result.frequency_khz = frequency_khz;
  taskEXIT_CRITICAL();

  request[0] = node_id;
  request[1] = (uint8_t)(frequency_khz >> 16);
  request[2] = (uint8_t)(frequency_khz >> 8);
  request[3] = (uint8_t)frequency_khz;
  request[4] = sample_time_ms;
  request[5] = sample_mode;
  request[6] = (uint8_t)(trigger_threshold >> 8);
  request[7] = (uint8_t)trigger_threshold;
  request[8] = (uint8_t)(trigger_timeout_ms >> 8);
  request[9] = (uint8_t)trigger_timeout_ms;
  request[10] = antenna_compensation;

  if (XC001_CAN_Send(0x11BU, request, sizeof(request)) == 0U)
  {
    taskENTER_CRITICAL();
    s_power_transaction.active = 0U;
    taskEXIT_CRITICAL();
    return XC001_CAN_QUERY_SEND_FAILED;
  }

  wait_ms = 1500U + (uint32_t)sample_time_ms;
  if (sample_mode == 1U)
  {
    wait_ms += trigger_timeout_ms;
  }
  started_at = HAL_GetTick();
  while ((HAL_GetTick() - started_at) < wait_ms)
  {
    uint8_t complete;

    taskENTER_CRITICAL();
    complete = s_power_transaction.complete;
    if (complete != 0U)
    {
      *result = s_power_transaction.result;
      s_power_transaction.active = 0U;
      s_power_ok_count++;
    }
    taskEXIT_CRITICAL();
    if (complete != 0U)
    {
      return XC001_CAN_QUERY_OK;
    }
    vTaskDelay(pdMS_TO_TICKS(1U));
  }

  taskENTER_CRITICAL();
  s_power_transaction.active = 0U;
  s_power_timeout_count++;
  taskEXIT_CRITICAL();
  return XC001_CAN_QUERY_TIMEOUT;
}

void XC001_CAN_Status(char *out, size_t out_size)
{
  uint8_t ready;
  uint32_t tx_count;
  uint32_t rx_count;
  uint32_t power_ok_count;
  uint32_t power_timeout_count;

  taskENTER_CRITICAL();
  ready = s_ready;
  tx_count = s_tx_count;
  rx_count = s_rx_count;
  power_ok_count = s_power_ok_count;
  power_timeout_count = s_power_timeout_count;
  taskEXIT_CRITICAL();
  snprintf(out, out_size,
           "CAN1:READY=%u,MODE=FD_NO_BRS,BAUD=2500000,TX=%lu,RX=%lu,PWR_OK=%lu,PWR_TO=%lu",
           ready, (unsigned long)tx_count, (unsigned long)rx_count,
           (unsigned long)power_ok_count, (unsigned long)power_timeout_count);
}

void XC001_CAN_LastRx(char *out, size_t out_size)
{
  char hex[196];
  uint8_t data[64];
  uint8_t len;
  uint32_t id;
  uint32_t rx_count;

  taskENTER_CRITICAL();
  memcpy(data, s_last_data, sizeof(data));
  len = s_last_len;
  id = s_last_id;
  rx_count = s_rx_count;
  taskEXIT_CRITICAL();
  XC001_FormatHexBytes(data, len, hex, sizeof(hex));
  if (rx_count == 0U)
  {
    snprintf(out, out_size, "EMPTY");
  }
  else
  {
    snprintf(out, out_size, "ID=0x%lX,LEN=%u,DATA=%s", (unsigned long)id, len, hex);
  }
}
