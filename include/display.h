#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// ST7735 Display pins
#define TFT_MOSI_PIN        GPIO_NUM_13
#define TFT_SCLK_PIN        GPIO_NUM_14
#define TFT_CS_PIN          GPIO_NUM_15
#define TFT_DC_PIN          GPIO_NUM_2
#define TFT_RST_PIN         GPIO_NUM_4

// Display dimensions
#define TFT_WIDTH           128
#define TFT_HEIGHT          160

// Colors (RGB565)
#define BLACK               0x0000
#define BLUE                0x001F
#define RED                 0xF800
#define GREEN               0x07E0
#define CYAN                0x07FF
#define MAGENTA             0xF81F
#define YELLOW              0xFFE0
#define WHITE               0xFFFF

// Function prototypes
void display_init(void);
void display_fill_screen(uint16_t color);
void display_draw_pixel(int16_t x, int16_t y, uint16_t color);
void display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void display_print_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);
void display_welcome(void);

#endif // DISPLAY_H