#ifndef LCD_ST7735_H
#define LCD_ST7735_H

#include <stdint.h>

#define LCD_COLOR_BLACK  0x0000U
#define LCD_COLOR_WHITE  0xFFFFU
#define LCD_COLOR_BLUE   0x03DFU
#define LCD_COLOR_RED    0xF800U
#define LCD_COLOR_GREEN  0x07E0U
#define LCD_COLOR_GRAY   0x8410U

void LCD_Init(void);
void LCD_Fill(uint16_t color);
void LCD_FillRect(uint8_t x, uint8_t y, uint8_t width, uint8_t height, uint16_t color);
void LCD_DrawString(uint8_t x, uint8_t y, const char *text, uint16_t foreground,
                    uint16_t background, uint8_t scale);

#endif
