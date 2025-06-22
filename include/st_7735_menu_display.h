// st7735_display.h
#pragma once

#include "Adafruit_ST7735.h"

#define TFT_CS    5
#define TFT_DC    2
#define TFT_RST   4

extern Adafruit_ST7735 tft;

void display_init(void);
void display_main_menu(const char **items, int num_items, int selected_index);
