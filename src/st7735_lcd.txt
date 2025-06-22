// st7735_lcd.c
#include "st7735_lcd.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

// ST7735 Commands
#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13
#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36
#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6
#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5
#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD
#define ST7735_PWCTR6  0xFC
#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// Buffer size for batch operations
#define SPI_BUFFER_SIZE 4096
#define MAX_PIXELS_PER_TRANSACTION (SPI_BUFFER_SIZE / 2)

// Global variables
static st7735_pins_t lcd_pins;
static spi_device_handle_t spi_handle;
static uint8_t current_rotation = 0;
static uint8_t spi_buffer[SPI_BUFFER_SIZE] __attribute__((aligned(4)));

// Simple 5x8 font (first 32 are non-printable)
static const uint8_t font5x8[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x7F,0x20,0x18,0x20,0x7F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x08,0x08,0x2A,0x1C,0x08}  // '~'
};

// Private function prototypes
static void spi_write_command(uint8_t cmd);
static void spi_write_data(uint8_t data);
static void spi_write_data_buffer(uint8_t* buffer, size_t len);
//static void spi_write_pixels_buffered(uint16_t* pixels, size_t count);
static void set_addr_window(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
static void spi_begin_transaction(void);
static void spi_end_transaction(void);

// Initialize the display
void st7735_init(st7735_pins_t pins) {
    lcd_pins = pins;
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << lcd_pins.cs_pin) | (1ULL << lcd_pins.dc_pin) | (1ULL << lcd_pins.rst_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Initialize SPI with higher clock speed and larger transfer size
    spi_bus_config_t buscfg = {
        .mosi_io_num = lcd_pins.mosi_pin,
        .miso_io_num = -1,
        .sclk_io_num = lcd_pins.sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ST7735_WIDTH * ST7735_HEIGHT * 2 + 64  // Full screen + some overhead
    };
    
spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10000000,
    .mode = 0,
    .spics_io_num = -1,  // Change this to -1 to disable hardware CS
    .queue_size = 1,
    .flags = SPI_DEVICE_NO_DUMMY
};

    
    
    spi_bus_initialize(HSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);  // Enable DMA
    spi_bus_add_device(HSPI_HOST, &devcfg, &spi_handle);
    
    // Reset and initialize display
    st7735_reset();
    
    // Initialization sequence
    spi_write_command(ST7735_SWRESET);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    spi_write_command(ST7735_SLPOUT);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    spi_write_command(ST7735_FRMCTR1);
    spi_write_data(0x01);
    spi_write_data(0x2C);
    spi_write_data(0x2D);
    
    spi_write_command(ST7735_FRMCTR2);
    spi_write_data(0x01);
    spi_write_data(0x2C);
    spi_write_data(0x2D);
    
    spi_write_command(ST7735_FRMCTR3);
    spi_write_data(0x01);
    spi_write_data(0x2C);
    spi_write_data(0x2D);
    spi_write_data(0x01);
    spi_write_data(0x2C);
    spi_write_data(0x2D);
    
    spi_write_command(ST7735_INVCTR);
    spi_write_data(0x07);
    
    spi_write_command(ST7735_PWCTR1);
    spi_write_data(0xA2);
    spi_write_data(0x02);
    spi_write_data(0x84);
    
    spi_write_command(ST7735_PWCTR2);
    spi_write_data(0xC5);
    
    spi_write_command(ST7735_PWCTR3);
    spi_write_data(0x0A);
    spi_write_data(0x00);
    
    spi_write_command(ST7735_PWCTR4);
    spi_write_data(0x8A);
    spi_write_data(0x2A);
    
    spi_write_command(ST7735_PWCTR5);
    spi_write_data(0x8A);
    spi_write_data(0xEE);
    
    spi_write_command(ST7735_VMCTR1);
    spi_write_data(0x0E);
    
    spi_write_command(ST7735_INVOFF);
    
    spi_write_command(ST7735_MADCTL);
    spi_write_data(0xC8);
    
    spi_write_command(ST7735_COLMOD);
    spi_write_data(0x05);
    
    spi_write_command(ST7735_CASET);
    spi_write_data(0x00);
    spi_write_data(0x00);
    spi_write_data(0x00);
    spi_write_data(0x7F);
    
    spi_write_command(ST7735_RASET);
    spi_write_data(0x00);
    spi_write_data(0x00);
    spi_write_data(0x00);
    spi_write_data(0x9F);
    
    spi_write_command(ST7735_GMCTRP1);
    spi_write_data(0x02);
    spi_write_data(0x1C);
    spi_write_data(0x07);
    spi_write_data(0x12);
    spi_write_data(0x37);
    spi_write_data(0x32);
    spi_write_data(0x29);
    spi_write_data(0x2D);
    spi_write_data(0x29);
    spi_write_data(0x25);
    spi_write_data(0x2B);
    spi_write_data(0x39);
    spi_write_data(0x00);
    spi_write_data(0x01);
    spi_write_data(0x03);
    spi_write_data(0x10);
    
    spi_write_command(ST7735_GMCTRN1);
    spi_write_data(0x03);
    spi_write_data(0x1D);
    spi_write_data(0x07);
    spi_write_data(0x06);
    spi_write_data(0x2E);
    spi_write_data(0x2C);
    spi_write_data(0x29);
    spi_write_data(0x2D);
    spi_write_data(0x2E);
    spi_write_data(0x2E);
    spi_write_data(0x37);
    spi_write_data(0x3F);
    spi_write_data(0x00);
    spi_write_data(0x00);
    spi_write_data(0x02);
    spi_write_data(0x10);
    
    spi_write_command(ST7735_NORON);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    spi_write_command(ST7735_DISPON);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    st7735_fill_screen(ST7735_BLACK);
}

// Hardware reset
void st7735_reset(void) {
    gpio_set_level(lcd_pins.rst_pin, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(lcd_pins.rst_pin, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(lcd_pins.rst_pin, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

// Set display rotation (0-3)
void st7735_set_rotation(uint8_t rotation) {
    current_rotation = rotation % 4;
    spi_write_command(ST7735_MADCTL);
    
    switch (current_rotation) {
        case 0:
            spi_write_data(0xC8);
            break;
        case 1:
            spi_write_data(0xC8);
            break;
        case 2:
            spi_write_data(0x08);
            break;
        case 3:
            spi_write_data(0x68);
            break;
    }
}

// OPTIMIZED: Fill entire screen with color using bulk transfer
void st7735_fill_screen(uint16_t color) {
    set_addr_window(0, 0, ST7735_WIDTH - 1, ST7735_HEIGHT - 1);
    
    // Prepare color data in 16-bit format
uint8_t pixel_data[MAX_PIXELS_PER_TRANSACTION * 2];
for (int i = 0; i < MAX_PIXELS_PER_TRANSACTION; i++) {
    pixel_data[i*2] = color >> 8;
    pixel_data[i*2 + 1] = color & 0xFF;
}
    
    size_t total_pixels = ST7735_WIDTH * ST7735_HEIGHT;
    size_t pixels_sent = 0;
    
    gpio_set_level(lcd_pins.dc_pin, 1);  // Data mode
    gpio_set_level(lcd_pins.cs_pin, 0);  // Select device
    
    while (pixels_sent < total_pixels) {
        size_t pixels_to_send = (total_pixels - pixels_sent > MAX_PIXELS_PER_TRANSACTION) 
                               ? MAX_PIXELS_PER_TRANSACTION 
                               : (total_pixels - pixels_sent);
        
        spi_transaction_t trans = {
    .length = pixels_to_send * 16,  // Keep this the same
    .tx_buffer = pixel_data
};
        
        spi_device_transmit(spi_handle, &trans);
        vTaskDelay(1);
        pixels_sent += pixels_to_send;
    }
    
    gpio_set_level(lcd_pins.cs_pin, 1);  // Deselect device
}

// Draw a single pixel (kept for compatibility, but slower than bulk operations)
void st7735_draw_pixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= ST7735_WIDTH || y < 0 || y >= ST7735_HEIGHT) return;
    
    set_addr_window(x, y, x, y);
    
    uint16_t pixel_data = __builtin_bswap16(color);
    gpio_set_level(lcd_pins.dc_pin, 1);  // Data mode
    spi_write_data_buffer((uint8_t*)&pixel_data, 2);
    spi_end_transaction();
}

// OPTIMIZED: Draw multiple pixels at once
void st7735_draw_pixels(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* colors) {
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
    if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
    
    set_addr_window(x, y, x + w - 1, y + h - 1);
    
    size_t total_pixels = w * h;
    size_t pixels_sent = 0;
    
    gpio_set_level(lcd_pins.dc_pin, 1);  // Data mode
    gpio_set_level(lcd_pins.cs_pin, 0);  // Select device
    
    while (pixels_sent < total_pixels) {
        size_t pixels_to_send = (total_pixels - pixels_sent > MAX_PIXELS_PER_TRANSACTION) 
                               ? MAX_PIXELS_PER_TRANSACTION 
                               : (total_pixels - pixels_sent);
        
        // Convert to big-endian in buffer
        for (size_t i = 0; i < pixels_to_send; i++) {
            ((uint16_t*)spi_buffer)[i] = __builtin_bswap16(colors[pixels_sent + i]);
        }
        
        spi_transaction_t trans = {
            .length = pixels_to_send * 16,  // 16 bits per pixel
            .tx_buffer = spi_buffer
        };
        
        spi_device_transmit(spi_handle, &trans);
        pixels_sent += pixels_to_send;
    }
    
    gpio_set_level(lcd_pins.cs_pin, 1);  // Deselect device
}

// OPTIMIZED: Draw a line using Bresenham's algorithm with pixel buffering
void st7735_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    // For short lines, use direct pixel drawing
    if (dx + dy < 50) {
        while (true) {
            st7735_draw_pixel(x0, y0, color);
            
            if (x0 == x1 && y0 == y1) break;
            
            int16_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx) {
                err += dx;
                y0 += sy;
            }
        }
        return;
    }
    
    // For longer lines, collect pixels and draw in batches
    uint16_t line_pixels[MAX_PIXELS_PER_TRANSACTION];
    int16_t line_x[MAX_PIXELS_PER_TRANSACTION];
    int16_t line_y[MAX_PIXELS_PER_TRANSACTION];
    size_t pixel_count = 0;
    
    while (true) {
        line_x[pixel_count] = x0;
        line_y[pixel_count] = y0;
        line_pixels[pixel_count] = color;
        pixel_count++;
        
        if (x0 == x1 && y0 == y1) break;
        
        // Flush buffer if full
        if (pixel_count >= MAX_PIXELS_PER_TRANSACTION - 1) {
            for (size_t i = 0; i < pixel_count; i++) {
                st7735_draw_pixel(line_x[i], line_y[i], line_pixels[i]);
            }
            pixel_count = 0;
        }
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
    
    // Draw remaining pixels
    for (size_t i = 0; i < pixel_count; i++) {
        st7735_draw_pixel(line_x[i], line_y[i], line_pixels[i]);
    }
}

// Draw rectangle outline
void st7735_draw_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    st7735_draw_line(x, y, x + w - 1, y, color);
    st7735_draw_line(x, y, x, y + h - 1, color);
    st7735_draw_line(x + w - 1, y, x + w - 1, y + h - 1, color);
    st7735_draw_line(x, y + h - 1, x + w - 1, y + h - 1, color);
}


// Fill rectangle
void st7735_fill_rect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if (x + w > ST7735_WIDTH) w = ST7735_WIDTH - x;
    if (y + h > ST7735_HEIGHT) h = ST7735_HEIGHT - y;
    
    set_addr_window(x, y, x + w - 1, y + h - 1);
    
    size_t total_pixels = w * h;
    uint16_t pixel_data = __builtin_bswap16(color);
    
    // Fill buffer with the color
    size_t buffer_pixels = SPI_BUFFER_SIZE / 2;
    for (size_t i = 0; i < buffer_pixels && i < total_pixels; i++) {
        ((uint16_t*)spi_buffer)[i] = pixel_data;
    }
    
    spi_begin_transaction();
    gpio_set_level(lcd_pins.dc_pin, 1);  // Data mode
    
    size_t pixels_sent = 0;
    while (pixels_sent < total_pixels) {
        size_t pixels_to_send = (total_pixels - pixels_sent > buffer_pixels) 
                               ? buffer_pixels 
                               : (total_pixels - pixels_sent);
        
        spi_transaction_t trans = {
            .length = pixels_to_send * 16,
            .tx_buffer = spi_buffer
        };
        
        spi_device_transmit(spi_handle, &trans);
        pixels_sent += pixels_to_send;
    }
    
    spi_end_transaction();
}

// Draw circle outline
void st7735_draw_circle(int16_t x0, int16_t y0, int16_t r, uint16_t color) {
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    
    st7735_draw_pixel(x0, y0 + r, color);
    st7735_draw_pixel(x0, y0 - r, color);
    st7735_draw_pixel(x0 + r, y0, color);
    st7735_draw_pixel(x0 - r, y0, color);
    
    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        
        st7735_draw_pixel(x0 + x, y0 + y, color);
        st7735_draw_pixel(x0 - x, y0 + y, color);
        st7735_draw_pixel(x0 + x, y0 - y, color);
        st7735_draw_pixel(x0 - x, y0 - y, color);
        st7735_draw_pixel(x0 + y, y0 + x, color);
        st7735_draw_pixel(x0 - y, y0 + x, color);
        st7735_draw_pixel(x0 + y, y0 - x, color);
        st7735_draw_pixel(x0 - y, y0 - x, color);
    }
}

// Print text
void st7735_print_text(int16_t x, int16_t y, const char* text, uint16_t color, uint16_t bg_color) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;
    
    while (*text) {
        char c = *text++;
        
        if (c == '\n') {
            cursor_x = x;
            cursor_y += 8;
            continue;
        }
        
        if (c < 32 || c > 126) c = 32; // Replace non-printable chars with space
        
        const uint8_t* char_data = font5x8[c - 32];
        
        for (int col = 0; col < 5; col++) {
            for (int row = 0; row < 8; row++) {
                if (char_data[col] & (1 << row)) {
                    st7735_draw_pixel(cursor_x + col, cursor_y + row, color);
                } else if (bg_color != color) {
                    st7735_draw_pixel(cursor_x + col, cursor_y + row, bg_color);
                }
            }
        }
        
        cursor_x += 6;
        if (cursor_x >= ST7735_WIDTH - 5) {
            cursor_x = x;
            cursor_y += 8;
        }
    }
}


/*void st7735_print_text(int16_t x, int16_t y, const char* text, uint16_t color, uint16_t bg_color) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    while (*text) {
        char c = *text++;

        if (c == '\n') {
            cursor_x = x;
            cursor_y += 8;
            continue;
        }

        if (c < 32 || c > 126) c = 32;
        const uint8_t* char_data = font5x8[c - 32];

        // 6x8 pixels per char (5 glyph + 1 spacing)
        uint8_t pixel_data[6 * 8 * 2];  // RGB565, 2 bytes per pixel
        int idx = 0;

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 6; col++) {
                uint16_t px;

                if (col < 5 && (char_data[col] & (1 << row))) {
                    px = color;
                } else {
                    px = bg_color;
                }

                pixel_data[idx++] = px >> 8;
                pixel_data[idx++] = px & 0xFF;
            }
        }

        // Set window: x to x+5, y to y+7
        set_addr_window(cursor_x, cursor_y, cursor_x + 5, cursor_y + 7);
        assert(sizeof(pixel_data) == 96);  // 6 x 8 x 2
        spi_write_data_buffer(pixel_data, sizeof(pixel_data));

        cursor_x += 6;
        if (cursor_x + 5 >= ST7735_WIDTH) {
            cursor_x = x;
            cursor_y += 8;
        }
    }
}
*/
void st7735_print_line(int16_t x, int16_t y, const char* text, uint16_t color, uint16_t bg_color) {
    const int CHAR_W = 6;  // 5 + spacing
    const int CHAR_H = 8;
    int len = strlen(text);

    if (len * CHAR_W > ST7735_WIDTH) {
        len = ST7735_WIDTH / CHAR_W;  // Clip to screen
    }

    int line_width = len * CHAR_W;
    uint8_t line_buffer[line_width * CHAR_H * 2];  // 2 bytes per pixel (RGB565)
    memset(line_buffer, 0, sizeof(line_buffer));

    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c < 32 || c > 126) c = 32;
        const uint8_t* glyph = font5x8[c - 32];

        for (int row = 0; row < CHAR_H; row++) {
            for (int col = 0; col < CHAR_W; col++) {
                uint16_t pixel = bg_color;
                if (col < 5 && (glyph[col] & (1 << row))) {
                    pixel = color;
                }
                int x_offset = i * CHAR_W + col;
                int pixel_index = (row * line_width + x_offset) * 2;
                line_buffer[pixel_index]     = pixel >> 8;
                line_buffer[pixel_index + 1] = pixel & 0xFF;
            }
        }
    }

    // Draw the full line buffer
    set_addr_window(x, y, x + line_width - 1, y + CHAR_H - 1);
    spi_write_data_buffer(line_buffer, sizeof(line_buffer));
}

// Convert RGB to 565 format
uint16_t st7735_color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Demo function - call this periodically to show different patterns
void st7735_display_demo(void) {
    static int demo_step = 0;
    static uint32_t last_update = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Update every 2 seconds
    if (current_time - last_update < 2000) return;
    last_update = current_time;
    
    switch (demo_step) {
        case 0:
            st7735_fill_screen(ST7735_RED);
            st7735_print_text(10, 10, "ST7735 Demo", ST7735_WHITE, ST7735_RED);
            break;
        case 1:
            st7735_fill_screen(ST7735_GREEN);
            st7735_draw_circle(64, 80, 30, ST7735_BLUE);
            break;
        case 2:
            st7735_fill_screen(ST7735_BLUE);
            st7735_draw_rect(20, 30, 88, 100, ST7735_YELLOW);
            break;
        case 3:
            st7735_fill_screen(ST7735_BLACK);
            for (int i = 0; i < 20; i++) {
                st7735_draw_pixel(rand() % ST7735_WIDTH, rand() % ST7735_HEIGHT, ST7735_WHITE);
            }
            break;
        case 4:
            st7735_fill_screen(ST7735_MAGENTA);
            st7735_print_text(5, 50, "Color Test", ST7735_BLACK, ST7735_MAGENTA);
            st7735_print_text(5, 70, "ESP32 + ST7735", ST7735_WHITE, ST7735_MAGENTA);
            break;
    }
    
    demo_step = (demo_step + 1) % 5;
}

// Private functions
static void spi_write_command(uint8_t cmd) {
    gpio_set_level(lcd_pins.dc_pin, 0);  // Command mode
    gpio_set_level(lcd_pins.cs_pin, 0);  // Add this back
    
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd
    };
    
    spi_device_transmit(spi_handle, &trans);
    gpio_set_level(lcd_pins.cs_pin, 1);  // Add this back
}

static void spi_write_data(uint8_t data) {
    gpio_set_level(lcd_pins.dc_pin, 1);  // Data mode
    // Remove these lines:
    // gpio_set_level(lcd_pins.cs_pin, 0);  
    // gpio_set_level(lcd_pins.cs_pin, 1);  
    
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &data
    };
    
    spi_device_transmit(spi_handle, &trans);
}

static void spi_write_data_buffer(uint8_t* buffer, size_t len) {
    gpio_set_level(lcd_pins.dc_pin, 1);  // Data mode
    // Remove these lines:
    // gpio_set_level(lcd_pins.cs_pin, 0);  
    // gpio_set_level(lcd_pins.cs_pin, 1);  
    
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = buffer
    };
    
    spi_device_transmit(spi_handle, &trans);
}

static void spi_begin_transaction(void) {
    gpio_set_level(lcd_pins.cs_pin, 0);  // Select device
}

static void spi_end_transaction(void) {
    gpio_set_level(lcd_pins.cs_pin, 1);  // Deselect device
}


static void set_addr_window(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    spi_begin_transaction();
    
    spi_write_command(ST7735_CASET);
    spi_write_data(0x00);
    spi_write_data(x0);
    spi_write_data(0x00);
    spi_write_data(x1);
    
    spi_write_command(ST7735_RASET);
    spi_write_data(0x00);
    spi_write_data(y0);
    spi_write_data(0x00);
    spi_write_data(y1);
    
    spi_write_command(ST7735_RAMWR);
    // Don't end transaction here - let calling function handle it
}