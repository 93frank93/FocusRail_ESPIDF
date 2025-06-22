#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "display.h"

static const char *TAG = "DISPLAY";

// ST7735 Commands  
#define ST7735_NOP          0x00
#define ST7735_SWRESET      0x01
#define ST7735_RDDID        0x04
#define ST7735_RDDST        0x09
#define ST7735_SLPIN        0x10
#define ST7735_SLPOUT       0x11
#define ST7735_PTLON        0x12
#define ST7735_NORON        0x13
#define ST7735_INVOFF       0x20
#define ST7735_INVON        0x21
#define ST7735_DISPOFF      0x28
#define ST7735_DISPON       0x29
#define ST7735_CASET        0x2A
#define ST7735_RASET        0x2B
#define ST7735_RAMWR        0x2C
#define ST7735_RAMRD        0x2E
#define ST7735_PTLAR        0x30
#define ST7735_COLMOD       0x3A
#define ST7735_MADCTL       0x36
#define ST7735_FRMCTR1      0xB1
#define ST7735_FRMCTR2      0xB2
#define ST7735_FRMCTR3      0xB3
#define ST7735_INVCTR       0xB4
#define ST7735_DISSET5      0xB6
#define ST7735_PWCTR1       0xC0
#define ST7735_PWCTR2       0xC1
#define ST7735_PWCTR3       0xC2
#define ST7735_PWCTR4       0xC3
#define ST7735_PWCTR5       0xC4
#define ST7735_VMCTR1       0xC5
#define ST7735_RDID1        0xDA
#define ST7735_RDID2        0xDB
#define ST7735_RDID3        0xDC
#define ST7735_RDID4        0xDD
#define ST7735_PWCTR6       0xFC
#define ST7735_GMCTRP1      0xE0
#define ST7735_GMCTRN1      0xE1

// Simple 5x7 font
static const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space
    {0x00, 0x00, 0x5f, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7f, 0x14, 0x7f, 0x14}, // #
    {0x24, 0x2a, 0x7f, 0x2a, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1c, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1c, 0x00}, // )
    {0x14, 0x08, 0x3e, 0x08, 0x14}, // *
    {0x08, 0x08, 0x3e, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, // 0
    {0x00, 0x42, 0x7f, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4b, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7f, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1e}, // 9
};

// Global variables
static spi_device_handle_t spi;

// Function prototypes
static void tft_write_command(uint8_t cmd);
static void tft_write_data(uint8_t data);
static void tft_write_data_16(uint16_t data);
static void tft_set_addr_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1);

void display_init(void) {
    esp_err_t ret;
    
    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = TFT_MOSI_PIN,
        .sclk_io_num = TFT_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * TFT_HEIGHT * 2
    };
    
    ret = spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    
    // Initialize SPI device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 10 * 1000 * 1000,  // 10 MHz
        .mode = 0,
        .spics_io_num = TFT_CS_PIN,
        .queue_size = 7,
    };
    
    ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);
    
    // Configure control pins
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << TFT_DC_PIN) | (1ULL << TFT_RST_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Reset display
    gpio_set_level(TFT_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(TFT_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Initialize ST7735 display
    tft_write_command(ST7735_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    
    tft_write_command(ST7735_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    tft_write_command(ST7735_FRMCTR1);
    tft_write_data(0x01);
    tft_write_data(0x2C);
    tft_write_data(0x2D);
    
    tft_write_command(ST7735_FRMCTR2);
    tft_write_data(0x01);
    tft_write_data(0x2C);
    tft_write_data(0x2D);
    
    tft_write_command(ST7735_FRMCTR3);
    tft_write_data(0x01);
    tft_write_data(0x2C);
    tft_write_data(0x2D);
    tft_write_data(0x01);
    tft_write_data(0x2C);
    tft_write_data(0x2D);
    
    tft_write_command(ST7735_INVCTR);
    tft_write_data(0x07);
    
    tft_write_command(ST7735_PWCTR1);
    tft_write_data(0xA2);
    tft_write_data(0x02);
    tft_write_data(0x84);
    
    tft_write_command(ST7735_PWCTR2);
    tft_write_data(0xC5);
    
    tft_write_command(ST7735_PWCTR3);
    tft_write_data(0x0A);
    tft_write_data(0x00);
    
    tft_write_command(ST7735_PWCTR4);
    tft_write_data(0x8A);
    tft_write_data(0x2A);
    
    tft_write_command(ST7735_PWCTR5);
    tft_write_data(0x8A);
    tft_write_data(0xEE);
    
    tft_write_command(ST7735_VMCTR1);
    tft_write_data(0x0E);
    
    tft_write_command(ST7735_INVOFF);
    
    tft_write_command(ST7735_MADCTL);
    tft_write_data(0xC8);
    
    tft_write_command(ST7735_COLMOD);
    tft_write_data(0x05);
    
    tft_write_command(ST7735_CASET);
    tft_write_data(0x00);
    tft_write_data(0x00);
    tft_write_data(0x00);
    tft_write_data(0x7F);
    
    tft_write_command(ST7735_RASET);
    tft_write_data(0x00);
    tft_write_data(0x00);
    tft_write_data(0x00);
    tft_write_data(0x9F);
    
    tft_write_command(ST7735_GMCTRP1);
    tft_write_data(0x02);
    tft_write_data(0x1c);
    tft_write_data(0x07);
    tft_write_data(0x12);
    tft_write_data(0x37);
    tft_write_data(0x32);
    tft_write_data(0x29);
    tft_write_data(0x2d);
    tft_write_data(0x29);
    tft_write_data(0x25);
    tft_write_data(0x2B);
    tft_write_data(0x39);
    tft_write_data(0x00);
    tft_write_data(0x01);
    tft_write_data(0x03);
    tft_write_data(0x10);
    
    tft_write_command(ST7735_GMCTRN1);
    tft_write_data(0x03);
    tft_write_data(0x1d);
    tft_write_data(0x07);
    tft_write_data(0x06);
    tft_write_data(0x2E);
    tft_write_data(0x2C);
    tft_write_data(0x29);
    tft_write_data(0x2D);
    tft_write_data(0x2E);
    tft_write_data(0x2E);
    tft_write_data(0x37);
    tft_write_data(0x3F);
    tft_write_data(0x00);
    tft_write_data(0x00);
    tft_write_data(0x02);
    tft_write_data(0x10);
    
    tft_write_command(ST7735_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    tft_write_command(ST7735_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Clear screen
    display_fill_screen(BLACK);
    
    ESP_LOGI(TAG, "Display initialized");
}

static void tft_write_command(uint8_t cmd) {
    gpio_set_level(TFT_DC_PIN, 0);  // Command mode
    
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    
    spi_device_polling_transmit(spi, &t);
}

static void tft_write_data(uint8_t data) {
    gpio_set_level(TFT_DC_PIN, 1);  // Data mode
    
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    
    spi_device_polling_transmit(spi, &t);
}

static void tft_write_data_16(uint16_t data) {
    gpio_set_level(TFT_DC_PIN, 1);  // Data mode
    
    uint8_t buffer[2] = {data >> 8, data & 0xFF};
    
    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = buffer;
    
    spi_device_polling_transmit(spi, &t);
}

static void tft_set_addr_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    tft_write_command(ST7735_CASET);
    tft_write_data(0x00);
    tft_write_data(x0);
    tft_write_data(0x00);
    tft_write_data(x1);
    
    tft_write_command(ST7735_RASET);
    tft_write_data(0x00);
    tft_write_data(y0);
    tft_write_data(0x00);
    tft_write_data(y1);
    
    tft_write_command(ST7735_RAMWR);
}

void display_fill_screen(uint16_t color) {
    tft_set_addr_window(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
    
    for (int i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
        tft_write_data_16(color);
    }
}

void display_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= TFT_WIDTH || y < 0 || y >= TFT_HEIGHT) return;
    
    tft_set_addr_window(x, y, x, y);
    tft_write_data_16(color);
}

void display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
    if (c < 32 || c > 126) c = 32; // Space for invalid chars
    
    int char_index = c - 32;
    
    for (int i = 0; i < 5; i++) {
        uint8_t line = font5x7[char_index][i];
        for (int j = 0; j < 7; j++) {
            if (line & 0x01) {
                if (size == 1) {
                    display_draw_pixel(x + i, y + j, color);
                } else {
                    for (int a = 0; a < size; a++) {
                        for (int b = 0; b < size; b++) {
                            display_draw_pixel(x + (i * size) + a, y + (j * size) + b, color);
                        }
                    }
                }
            } else if (bg != color) {
                if (size == 1) {
                    display_draw_pixel(x + i, y + j, bg);
                } else {
                    for (int a = 0; a < size; a++) {
                        for (int b = 0; b < size; b++) {
                            display_draw_pixel(x + (i * size) + a, y + (j * size) + b, bg);
                        }
                    }
                }
            }
            line >>= 1;
        }
    }
}

void display_print_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;
    
    while (*str) {
        if (*str == '\n') {
            cursor_y += size * 8;
            cursor_x = x;
        } else if (*str == '\r') {
            cursor_x = x;
        } else {
            display_draw_char(cursor_x, cursor_y, *str, color, bg, size);
            cursor_x += size * 6;
            if (cursor_x > (TFT_WIDTH - size * 6)) {
                cursor_x = x;
                cursor_y += size * 8;
            }
        }
        str++;
    }
}

void display_welcome(void) {
    display_fill_screen(BLACK);
    display_print_string(10, 50, "FOCUS RAIL", WHITE, BLACK, 2);
    display_print_string(10, 70, "CONTROLLER", WHITE, BLACK, 2);
    display_print_string(10, 100, "Initializing...", YELLOW, BLACK, 1);
}