// st7735_lcd.h
#ifndef ST7735_LCD_H
#define ST7735_LCD_H

#include <stdint.h>
#include <stdbool.h>

// Display dimensions
#define ST7735_WIDTH  128
#define ST7735_HEIGHT 160

// Common 16-bit colors (RGB565 format)
#define ST7735_BLACK   0x0000
#define ST7735_BLUE    0x001F
#define ST7735_RED     0xF800
#define ST7735_GREEN   0x07E0
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_YELLOW  0xFFE0
#define ST7735_WHITE   0xFFFF

// Pin configuration structure
typedef struct {
    int sclk_pin;   // SPI Clock
    int mosi_pin;   // SPI MOSI (Data)
    int cs_pin;     // Chip Select
    int dc_pin;     // Data/Command
    int rst_pin;    // Reset
} st7735_pins_t;

// Function prototypes
void st7735_init(st7735_pins_t pins);
void st7735_reset(void);
void st7735_set_rotation(uint8_t rotation);
void st7735_fill_screen(uint16_t color);
void st7735_draw_pixel(int16_t x, int16_t y, uint16_t color);
void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void st7735_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void st7735_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
void st7735_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void st7735_print_text(int16_t x, int16_t y, const char* text, uint16_t color, uint16_t bg_color);
void st7735_print_line(int16_t x, int16_t y, const char* text, uint16_t color, uint16_t bg_color);
void st7735_display_demo(void);
uint16_t st7735_color565(uint8_t r, uint8_t g, uint8_t b);

#endif // ST7735_LCD_H