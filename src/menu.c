#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "menu.h"
#include "display.h"  // Assuming you'll create a display module

static const char *TAG = "MENU";

// Global menu configuration
static menu_config_t menu_config = {
    .focus_position = 0,
    .step_size = 1,
    .motor_enabled = false,
    .current_menu = MENU_MAIN,
    .menu_selection = 0
};

// Callback functions for hardware control
static stepper_move_callback_t stepper_move_cb = NULL;
static stepper_enable_callback_t stepper_enable_cb = NULL;

// Private function prototypes
static void handle_main_menu_input(encoder_event_t *event);
static void handle_move_menu_input(encoder_event_t *event);
static void handle_settings_menu_input(encoder_event_t *event);
static void handle_auto_stack_menu_input(encoder_event_t *event);
static void display_main_menu(void);
static void display_move_menu(void);
static void display_settings_menu(void);
static void display_auto_stack_menu(void);

void menu_init(void) {
    ESP_LOGI(TAG, "Menu system initialized");
}

void menu_set_stepper_callbacks(stepper_move_callback_t move_cb, stepper_enable_callback_t enable_cb) {
    stepper_move_cb = move_cb;
    stepper_enable_cb = enable_cb;
}

void menu_handle_input(encoder_event_t *event) {
    if (event == NULL) return;
    
    switch (menu_config.current_menu) {
        case MENU_MAIN:
            handle_main_menu_input(event);
            break;
        case MENU_MOVE:
            handle_move_menu_input(event);
            break;
        case MENU_SETTINGS:
            handle_settings_menu_input(event);
            break;
        case MENU_AUTO_STACK:
            handle_auto_stack_menu_input(event);
            break;
    }
}

void menu_display(void) {
    switch (menu_config.current_menu) {
        case MENU_MAIN:
            display_main_menu();
            break;
        case MENU_MOVE:
            display_move_menu();
            break;
        case MENU_SETTINGS:
            display_settings_menu();
            break;
        case MENU_AUTO_STACK:
            display_auto_stack_menu();
            break;
    }
}

// Main menu input handling
static void handle_main_menu_input(encoder_event_t *event) {
    if (event->button_pressed) {
        switch (menu_config.menu_selection) {
            case 0: // Move
                menu_config.current_menu = MENU_MOVE;
                break;
            case 1: // Settings
                menu_config.current_menu = MENU_SETTINGS;
                menu_config.menu_selection = 0;
                break;
            case 2: // Auto Stack
                menu_config.current_menu = MENU_AUTO_STACK;
                break;
        }
        menu_display();
        return;
    }
    
    if (event->direction != 0) {
        menu_config.menu_selection += event->direction;
        if (menu_config.menu_selection < 0) menu_config.menu_selection = 2;
        if (menu_config.menu_selection > 2) menu_config.menu_selection = 0;
        menu_display();
    }
}

// Move menu input handling
static void handle_move_menu_input(encoder_event_t *event) {
    if (event->button_pressed) {
        menu_config.current_menu = MENU_MAIN;
        menu_config.menu_selection = 0;
        menu_display();
        return;
    }
    
    if (event->direction != 0) {
        if (menu_config.motor_enabled && stepper_move_cb) {
            int steps = event->direction * menu_config.step_size;
            stepper_move_cb(steps);
            menu_config.focus_position += steps;
        }
        menu_display();
    }
}

// Settings menu input handling
static void handle_settings_menu_input(encoder_event_t *event) {
    if (event->button_pressed) {
        switch (menu_config.menu_selection) {
            case 0: // Step Size
                // Cycle through step sizes: 1, 5, 10, 50, 100
                if (menu_config.step_size == 1) menu_config.step_size = 5;
                else if (menu_config.step_size == 5) menu_config.step_size = 10;
                else if (menu_config.step_size == 10) menu_config.step_size = 50;
                else if (menu_config.step_size == 50) menu_config.step_size = 100;
                else menu_config.step_size = 1;
                break;
            case 1: // Motor Enable
                menu_config.motor_enabled = !menu_config.motor_enabled;
                if (stepper_enable_cb) {
                    stepper_enable_cb(menu_config.motor_enabled);
                }
                break;
            case 2: // Reset Position
                menu_config.focus_position = 0;
                break;
            case 3: // Back
                menu_config.current_menu = MENU_MAIN;
                menu_config.menu_selection = 0;
                break;
        }
        menu_display();
        return;
    }
    
    if (event->direction != 0) {
        menu_config.menu_selection += event->direction;
        if (menu_config.menu_selection < 0) menu_config.menu_selection = 3;
        if (menu_config.menu_selection > 3) menu_config.menu_selection = 0;
        menu_display();
    }
}

// Auto stack menu input handling
static void handle_auto_stack_menu_input(encoder_event_t *event) {
    if (event->button_pressed) {
        menu_config.current_menu = MENU_MAIN;
        menu_config.menu_selection = 0;
        menu_display();
    }
    // No encoder movement handling in auto stack menu for now
}

// Main menu display
static void display_main_menu(void) {
    display_fill_screen(BLACK);      
    
    display_print_string(10, 10, "FOCUS RAIL", WHITE, TRANSPARENT, 2);
    display_print_string(10, 30, "----------", WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 45, menu_config.menu_selection == 0 ? ">Move" : " Move", 
                       menu_config.menu_selection == 0 ? YELLOW : WHITE, TRANSPARENT, 1);
    display_print_string(10, 55, menu_config.menu_selection == 1 ? ">Settings" : " Settings", 
                       menu_config.menu_selection == 1 ? YELLOW : WHITE, TRANSPARENT, 1);
    display_print_string(10, 65, menu_config.menu_selection == 2 ? ">Auto Stack" : " Auto Stack", 
                       menu_config.menu_selection == 2 ? YELLOW : WHITE, TRANSPARENT, 1);
    
    char buffer[32];
    sprintf(buffer, "Pos: %d", menu_config.focus_position);
    display_print_string(10, 85, buffer, GREEN, TRANSPARENT, 1);
    
    sprintf(buffer, "Step: %d", menu_config.step_size);
    display_print_string(10, 95, buffer, GREEN, TRANSPARENT, 1);
    
    display_print_string(10, 105, menu_config.motor_enabled ? "Motor: ON" : "Motor: OFF", 
                       menu_config.motor_enabled ? GREEN : RED, TRANSPARENT, 1);
}

// Move menu display
static void display_move_menu(void) {
    display_fill_screen(BLACK);
    
    display_print_string(10, 10, "MOVE MODE", WHITE, TRANSPARENT, 2);
    display_print_string(10, 30, "---------", WHITE, TRANSPARENT, 1);
    
    char buffer[32];
    sprintf(buffer, "Position: %d", menu_config.focus_position);
    display_print_string(10, 45, buffer, WHITE, TRANSPARENT, 1);
    
    sprintf(buffer, "Step Size: %d", menu_config.step_size);
    display_print_string(10, 55, buffer, WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 70, "Rotate: Move", YELLOW, TRANSPARENT, 1);
    display_print_string(10, 80, "Press: Menu", YELLOW, TRANSPARENT, 1);
    
    if (menu_config.motor_enabled) {
        display_print_string(10, 100, "Motor: ENABLED", GREEN, TRANSPARENT, 1);
    } else {
        display_print_string(10, 100, "Motor: DISABLED", RED, TRANSPARENT, 1);
        display_print_string(10, 110, "Enable in Settings", RED, TRANSPARENT, 1);
    }
}

// Settings menu display
static void display_settings_menu(void) {
    display_fill_screen(BLACK);
    
    display_print_string(10, 10, "SETTINGS", WHITE, TRANSPARENT, 2);
    display_print_string(10, 30, "--------", WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 45, menu_config.menu_selection == 0 ? ">Step Size" : " Step Size", 
                       menu_config.menu_selection == 0 ? YELLOW : WHITE, TRANSPARENT, 1);
    char buffer[32];
    sprintf(buffer, "  Current: %d", menu_config.step_size);
    display_print_string(10, 55, buffer, WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 70, menu_config.menu_selection == 1 ? ">Motor Enable" : " Motor Enable", 
                       menu_config.menu_selection == 1 ? YELLOW : WHITE, TRANSPARENT, 1);
    sprintf(buffer, "  Status: %s", menu_config.motor_enabled ? "ON" : "OFF");
    display_print_string(10, 80, buffer, menu_config.motor_enabled ? GREEN : RED, TRANSPARENT, 1);
    
    display_print_string(10, 95, menu_config.menu_selection == 2 ? ">Reset Position" : " Reset Position", 
                       menu_config.menu_selection == 2 ? YELLOW : WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 110, menu_config.menu_selection == 3 ? ">Back" : " Back", 
                       menu_config.menu_selection == 3 ? YELLOW : WHITE, TRANSPARENT, 1);
}

// Auto stack menu display
static void display_auto_stack_menu(void) {
    display_fill_screen(BLACK);
    
    display_print_string(10, 10, "AUTO STACK", WHITE, TRANSPARENT, 2);
    display_print_string(10, 30, "----------", WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 45, "Coming Soon!", YELLOW, TRANSPARENT, 1);
    display_print_string(10, 60, "- Set start/end", WHITE, TRANSPARENT, 1);
    display_print_string(10, 70, "- Set intervals", WHITE, TRANSPARENT, 1);
    display_print_string(10, 80, "- Auto capture", WHITE, TRANSPARENT, 1);
    
    display_print_string(10, 100, "Press to return", YELLOW, TRANSPARENT, 1);
}

// Menu task
void menu_task(void *pvParameters) {
    menu_display();
    
    while (1) {
        // Update display every 100ms if in move mode to show real-time position
        if (menu_config.current_menu == MENU_MOVE) {
            menu_display();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Getter and setter functions
void menu_set_motor_enabled(bool enabled) {
    menu_config.motor_enabled = enabled;
}

void menu_set_focus_position(int position) {
    menu_config.focus_position = position;
}

void menu_update_focus_position(int delta) {
    menu_config.focus_position += delta;
}

int menu_get_focus_position(void) {
    return menu_config.focus_position;
}

int menu_get_step_size(void) {
    return menu_config.step_size;
}

bool menu_get_motor_enabled(void) {
    return menu_config.motor_enabled;
}