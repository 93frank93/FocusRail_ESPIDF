// main.c (refactored with st7735_print_line using original menu structure)
#include "macro_rail_menu.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ENCODER_SWITCH_ACTIVE_LOW true

static const char* main_menu_items[] = {
    "Manual Control",
    "Auto Stack",
    "Settings",
    "Calibration",
    "Advanced"
};

static const char* manual_menu_items[] = {
    "Position:",
    "Move to:",
    "Step Size:",
    "Home Rail",
    "Take Photo"
};

static const char* auto_stack_items[] = {
    "Start Pos:",
    "End Pos:",
    "Step Size:",
    "Delay (ms):",
    "Total Shots:",
    "Start Stack",
    "Set Current as Start",
    "Set Current as End"
};

static const char* settings_items[] = {
    "Brightness:",
    "Trigger Duration:",
    "Settling Time:",
    "Beep:",
    "Backlash Comp:",
    "Encoder Sens:",
    "Save Settings"
};

void display_main_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    st7735_print_line(20, 5, "MACRO RAIL", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);

    for (int i = 0; i < 5; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;

        char line[32];
        snprintf(line, sizeof(line), "%s%s",
                 (i == menu_selection) ? "> " : "  ",
                 main_menu_items[i]);

        st7735_print_line(5, 25 + i * 12, line, color, bg_color);
    }

    char status[32];
    snprintf(status, sizeof(status), "Pos: %.3fmm %s",
             rail_config.current_position_mm,
             rail_config.homed ? "HOMED" : "NOT HOMED");
    st7735_print_line(5, 145, status, ST7735_GREEN, ST7735_BLACK);
}

void display_manual_control(void) {
    st7735_fill_screen(ST7735_BLACK);
    st7735_print_line(15, 5, "MANUAL CONTROL", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);

    for (int i = 0; i < 5; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;

        char line[32];
        switch (i) {
            case 0:
                snprintf(line, sizeof(line), "%sPosition: %.3fmm",
                         (i == menu_selection) ? "> " : "  ",
                         rail_config.current_position_mm);
                break;
            case 1:
                snprintf(line, sizeof(line), "%sMove to: %.3fmm",
                         (i == menu_selection) ? "> " : "  ",
                         0.0f);
                break;
            case 2:
                snprintf(line, sizeof(line), "%sStep: %.1fum",
                         (i == menu_selection) ? "> " : "  ",
                         rail_config.step_size_microns);
                break;
            default:
                snprintf(line, sizeof(line), "%s%s",
                         (i == menu_selection) ? "> " : "  ",
                         manual_menu_items[i]);
                break;
        }

        st7735_print_line(5, 45 + i * 12, line, color, bg_color);
    }

    st7735_print_line(5, 120, "Rotate: Move/Adjust", ST7735_GREEN, ST7735_BLACK);
    st7735_print_line(5, 130, "Press: Select/Execute", ST7735_GREEN, ST7735_BLACK);
    st7735_print_line(5, 140, "Long Press: Back", ST7735_GREEN, ST7735_BLACK);
}

void display_auto_stack_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    st7735_print_line(20, 5, "AUTO STACK", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);

    for (int i = 0; i < 8; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;

        char line[32];
        switch (i) {
            case 0:
                snprintf(line, sizeof(line), "%sStart: %.3fmm",
                         (i == menu_selection) ? "> " : "  ",
                         stack_config.start_position_mm);
                break;
            case 1:
                snprintf(line, sizeof(line), "%sEnd: %.3fmm",
                         (i == menu_selection) ? "> " : "  ",
                         stack_config.end_position_mm);
                break;
            case 2:
                snprintf(line, sizeof(line), "%sStep: %.1fum",
                         (i == menu_selection) ? "> " : "  ",
                         stack_config.step_size_microns);
                break;
            case 3:
                snprintf(line, sizeof(line), "%sDelay: %dms",
                         (i == menu_selection) ? "> " : "  ",
                         stack_config.delay_ms);
                break;
            case 4:
                snprintf(line, sizeof(line), "%sShots: %d",
                         (i == menu_selection) ? "> " : "  ",
                         stack_config.total_shots);
                break;
            default:
                snprintf(line, sizeof(line), "%s%s",
                         (i == menu_selection) ? "> " : "  ",
                         auto_stack_items[i]);
                break;
        }

        st7735_print_line(5, 25 + i * 12, line, color, bg_color);
    }

    float distance = fabs(stack_config.end_position_mm - stack_config.start_position_mm);
    char info[32];
    snprintf(info, sizeof(info), "Distance: %.3fmm", distance);
    st7735_print_line(5, 135, info, ST7735_CYAN, ST7735_BLACK);
}

void display_settings_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    st7735_print_line(30, 5, "SETTINGS", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);

    for (int i = 0; i < 7; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;

        char line[32];
        switch (i) {
            case 0:
                snprintf(line, sizeof(line), "%sBright: %d%%",
                         (i == menu_selection) ? "> " : "  ",
                         system_config.lcd_brightness);
                break;
            case 1:
                snprintf(line, sizeof(line), "%sTrigger: %dms",
                         (i == menu_selection) ? "> " : "  ",
                         system_config.camera_trigger_duration);
                break;
            case 2:
                snprintf(line, sizeof(line), "%sSettle: %dms",
                         (i == menu_selection) ? "> " : "  ",
                         system_config.settling_time);
                break;
            case 3:
                snprintf(line, sizeof(line), "%sBeep: %s",
                         (i == menu_selection) ? "> " : "  ",
                         system_config.beep_enabled ? "ON" : "OFF");
                break;
            case 4:
                snprintf(line, sizeof(line), "%sBacklash: %.1fum",
                         (i == menu_selection) ? "> " : "  ",
                         system_config.backlash_compensation);
                break;
            case 5:
                snprintf(line, sizeof(line), "%sEnc Sens: %dx",
                         (i == menu_selection) ? "> " : "  ",
                         system_config.encoder_sensitivity);
                break;
            default:
                snprintf(line, sizeof(line), "%s%s",
                         (i == menu_selection) ? "> " : "  ",
                         settings_items[i]);
                break;
        }

        st7735_print_line(5, 25 + i * 12, line, color, bg_color);
    }
}
