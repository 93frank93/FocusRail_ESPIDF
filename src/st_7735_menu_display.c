// st7735_display.c
#include "st_7735_menu_display.hplay.h"
#include "Adafruit_ST7735.h"
#include "Adafruit_GFX.h"

#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void display_init(void) {
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(1);
    tft.fillScreen(ST77XX_BLACK);
    tft.setTextWrap(true);
}

void display_main_menu(const char **items, int num_items, int selected_index) {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextSize(1);

    for (int i = 0; i < num_items; ++i) {
        if (i == selected_index) {
            tft.setTextColor(ST77XX_YELLOW);
            tft.print("> ");
        } else {
            tft.setTextColor(ST77XX_WHITE);
            tft.print("  ");
        }
        tft.println(items[i]);
    }
}
