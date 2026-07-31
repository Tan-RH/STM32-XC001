#include "lcd_st7735.h"

#include "main.h"
#include <ctype.h>

#define LCD_WIDTH  128U
#define LCD_HEIGHT 160U

static void spi_delay(void) { __NOP(); __NOP(); }

static void write_byte(uint8_t value)
{
  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1)
  {
    HAL_GPIO_WritePin(LCD_SCK_GPIO_Port, LCD_SCK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_MOSI_GPIO_Port, LCD_MOSI_Pin,
                      (value & mask) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    spi_delay();
    HAL_GPIO_WritePin(LCD_SCK_GPIO_Port, LCD_SCK_Pin, GPIO_PIN_SET);
    spi_delay();
  }
}

static void command(uint8_t value)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  write_byte(value);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void data(uint8_t value)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  write_byte(value);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
  command(0x2AU); data(0); data(x0 + 2U); data(0); data(x1 + 2U);
  command(0x2BU); data(0); data(y0 + 1U); data(0); data(y1 + 1U);
  command(0x2CU);
}

void LCD_Init(void)
{
  HAL_GPIO_WritePin(FONT_CS_GPIO_Port, FONT_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(100U);
  HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(100U);
  HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
  HAL_Delay(100U);
  command(0x11U); HAL_Delay(120U);
  command(0xB1U); data(0x05U); data(0x3CU); data(0x3CU);
  command(0xB2U); data(0x05U); data(0x3CU); data(0x3CU);
  command(0xB3U); data(0x05U); data(0x3CU); data(0x3CU); data(0x05U); data(0x3CU); data(0x3CU);
  command(0xB4U); data(0x03U);
  command(0xC0U); data(0x28U); data(0x08U); data(0x04U);
  command(0xC1U); data(0xC0U);
  command(0xC2U); data(0x0DU); data(0x00U);
  command(0xC3U); data(0x8DU); data(0x2AU);
  command(0xC4U); data(0x8DU); data(0xEEU);
  command(0xC5U); data(0x1AU);
  command(0x36U); data(0xC0U);
  command(0xE0U);
  data(0x04U); data(0x22U); data(0x07U); data(0x0AU); data(0x2EU); data(0x30U); data(0x25U); data(0x2AU);
  data(0x28U); data(0x26U); data(0x2EU); data(0x3AU); data(0x00U); data(0x01U); data(0x03U); data(0x13U);
  command(0xE1U);
  data(0x04U); data(0x16U); data(0x06U); data(0x0DU); data(0x2DU); data(0x26U); data(0x23U); data(0x27U);
  data(0x27U); data(0x25U); data(0x2DU); data(0x3BU); data(0x00U); data(0x01U); data(0x04U); data(0x13U);
  command(0x3AU); data(0x05U);
  command(0x29U); HAL_Delay(20U);
  LCD_Fill(LCD_COLOR_BLACK);
}

void LCD_FillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t color)
{
  if ((width == 0U) || (height == 0U) || (x >= LCD_WIDTH) || (y >= LCD_HEIGHT)) return;
  if ((uint16_t)x + width > LCD_WIDTH) width = LCD_WIDTH - x;
  if ((uint16_t)y + height > LCD_HEIGHT) height = LCD_HEIGHT - y;
  window(x, y, x + width - 1U, y + height - 1U);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
  for (uint32_t i = 0; i < (uint32_t)width * height; ++i)
  {
    write_byte((uint8_t)(color >> 8)); write_byte((uint8_t)color);
  }
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

void LCD_Fill(uint16_t color) { LCD_FillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, color); }

static const uint8_t *glyph(char input)
{
  static const uint8_t blank[5] = {0,0,0,0,0};
  static const uint8_t digits[10][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},
    {0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E}};
  static const uint8_t letters[26][5] = {
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
    {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0,0x41,0x7F,0x41,0},
    {0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
    {0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}};
  static const uint8_t colon[5]={0,0x36,0x36,0,0}, dot[5]={0,0x60,0x60,0,0};
  static const uint8_t slash[5]={0x20,0x10,0x08,0x04,0x02}, dash[5]={0x08,0x08,0x08,0x08,0x08};
  static const uint8_t greater[5]={0x00,0x41,0x22,0x14,0x08};
  char c = (char)toupper((unsigned char)input);
  if (c >= '0' && c <= '9') return digits[c-'0'];
  if (c >= 'A' && c <= 'Z') return letters[c-'A'];
  if (c == ':') return colon;
  if (c == '.') return dot;
  if (c == '/') return slash;
  if (c == '-') return dash;
  if (c == '>') return greater;
  return blank;
}

void LCD_DrawString(uint8_t x, uint8_t y, const char *text, uint16_t fg,
                    uint16_t bg, uint8_t scale)
{
  if ((text == NULL) || (scale == 0U)) return;
  while (*text != '\0' && ((uint16_t)x + 6U*scale) <= LCD_WIDTH &&
         ((uint16_t)y + 8U*scale) <= LCD_HEIGHT)
  {
    const uint8_t *g = glyph(*text++);
    window(x, y, (uint8_t)(x + 6U*scale - 1U), (uint8_t)(y + 8U*scale - 1U));
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    for (uint8_t row = 0; row < 8U; ++row)
      for (uint8_t sy = 0; sy < scale; ++sy)
        for (uint8_t col = 0; col < 6U; ++col)
        {
          uint16_t color = (col < 5U && (g[col] & (1U << row))) ? fg : bg;
          for (uint8_t sx = 0; sx < scale; ++sx)
          {
            write_byte((uint8_t)(color >> 8));
            write_byte((uint8_t)color);
          }
        }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    x = (uint8_t)(x + 6U*scale);
  }
}
