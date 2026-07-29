#include "xc001_spi_bus.h"
#include "xc001_config.h"
#include "spi.h"
#include "xc001_board.h"
#include <stdio.h>

static uint8_t s_ready;
static uint32_t s_transfer_count;

void XC001_SPIBus_Init(void)
{
#if (XC001_SPI5_BUS_ENABLE != 0U)
  if (hspi5.Init.DataSize != SPI_DATASIZE_8BIT)
  {
    HAL_SPI_DeInit(&hspi5);
    hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
    if (HAL_SPI_Init(&hspi5) == HAL_OK)
    {
      s_ready = 1U;
    }
    else
    {
      XC001_Board_SetStatusOk(0U);
    }
  }
  else
  {
    s_ready = 1U;
  }
#else
  s_ready = 0U;
  s_transfer_count = 0UL;
#endif
}

uint8_t XC001_SPIBus_Transfer(const uint8_t *tx, uint8_t *rx, uint8_t len)
{
#if (XC001_SPI5_BUS_ENABLE != 0U)
  if (s_ready == 0U || tx == 0 || rx == 0 || len == 0U)
  {
    return 0U;
  }
  if (HAL_SPI_TransmitReceive(&hspi5, (uint8_t *)tx, rx, len, 500) == HAL_OK)
  {
    s_transfer_count++;
    return 1U;
  }
  return 0U;
#else
  (void)tx;
  (void)rx;
  (void)len;
  return 0U;
#endif
}

void XC001_SPIBus_Status(char *out, size_t out_size)
{
#if (XC001_SPI5_BUS_ENABLE != 0U)
  snprintf(out, out_size, "SPI5:READY=%u,COUNT=%lu", s_ready, (unsigned long)s_transfer_count);
#else
  snprintf(out, out_size, "SPI5:READY=0,COUNT=%lu,DISABLED=PF7_PF8_PF9_RF_SWITCH",
           (unsigned long)s_transfer_count);
#endif
}
