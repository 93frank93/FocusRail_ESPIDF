// macro_rail_menu.c
#include "macro_rail_menu.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "st_7735_menu_display.hplay.h"


// Global variables
menu_state_t current_menu = MENU_MAIN;
int menu_selection = 0;
int sub_menu_selection = 0;
bool menu_changed = true;
bool in_edit_mode = false;
bool stack_running = false;

// Configuration structures
rail_config_t rail_config = {
    .step_size_microns = 0.625f,  // Default for common steppers
    .max_travel_mm = 100.0f,
    .current_position_mm = 0.0f,
    .total_steps = 0,
    .steps_per_mm = 1600,  // 1.6mm pitch, 1/10 microstepping
    .homed = false
};

stack_config_t stack_config = {
    .start_position_mm = 0.0f,
    .end_position_mm = 10.0f,
    .step_size_microns = 50.0f,
    .total_shots = 0,
    .shots_taken = 0,
    .delay_ms = 2000,
    .reverse_direction = false,
    .return_to_start = true
};

system_config_t system_config = {
    .lcd_brightness = 80,
    .camera_trigger_duration = 100,
    .settling_time = 500,
    .beep_enabled = true,
    .backlash_compensation = 25.0f,
    .encoder_sensitivity = 1
};

// Encoder variables
static volatile int encoder_counter = 0;
static volatile bool encoder_pressed = false;
static volatile uint32_t last_encoder_time = 0;


// Menu text arrays
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

static void encoder_isr(void* arg);
static void encoder_sw_isr(void* arg);
void auto_stack_task(void *pvParameters);

// Initialize menu system
void menu_init(void) {
    encoder_init();
    stepper_init();
    camera_init();
    load_settings();
    calculate_stack_shots();
    
    // Create menu task
    xTaskCreate(menu_task, "menu_task", 4096, NULL, 5, NULL);
}

// Main menu task
void menu_task(void *pvParameters) {
    while (1) {
        if (menu_changed) {
            switch (current_menu) {
                case MENU_MAIN:
                    display_main_menu();
                    break;
                case MENU_MANUAL_CONTROL:
                    display_manual_control();
                    break;
                case MENU_AUTO_STACK:
                    display_auto_stack_menu();
                    break;
                case MENU_SETTINGS:
                    display_settings_menu();
                    break;
                case MENU_CALIBRATION:
                    display_calibration_menu();
                    break;
                case MENU_STACK_PROGRESS:
                    display_stack_progress();
                    break;
                case MENU_ADVANCED:
                    display_advanced_menu();
                    break;
            }
            menu_changed = false;
        }
        
        // Handle encoder input
        if (encoder_counter != 0) {
            handle_encoder_rotation(encoder_counter > 0 ? 1 : -1);
            encoder_counter = 0;
        }
        
        if (encoder_pressed) {
            handle_encoder_press();
            encoder_pressed = false;
        }
        
        // Update position display if in manual mode
        if (current_menu == MENU_MANUAL_CONTROL) {
            update_position_display();
        }
        
        // Update stack progress if running
        if (stack_running && current_menu == MENU_STACK_PROGRESS) {
            display_stack_progress();
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Display main menu
void display_main_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    // Title
    st7735_print_text(20, 5, "MACRO RAIL", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    // Menu items
    for (int i = 0; i < 5; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;
        
        char line[32];
        snprintf(line, sizeof(line), "%s%s", 
                (i == menu_selection) ? "> " : "  ", 
                main_menu_items[i]);
        
        st7735_print_text(5, 25 + i * 12, line, color, bg_color);
    }
    
    // Status line
    char status[32];
    snprintf(status, sizeof(status), "Pos: %.3fmm %s", 
             rail_config.current_position_mm,
             rail_config.homed ? "HOMED" : "NOT HOMED");
    st7735_print_text(5, 145, status, ST7735_GREEN, ST7735_BLACK);
}

// Display manual control menu
void display_manual_control(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    // Title
    st7735_print_text(15, 5, "MANUAL CONTROL", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    // Current position (large display)
    char pos_str[32];
    snprintf(pos_str, sizeof(pos_str), "%.3f mm", rail_config.current_position_mm);
    st7735_print_text(25, 25, pos_str, ST7735_CYAN, ST7735_BLACK);
    
    // Menu items
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
                        0.0f); // Target position
                break;
            case 2:
                snprintf(line, sizeof(line), "%sStep: %.1fum", 
                        (i == menu_selection) ? "> " : "  ", 
                        rail_config.step_size_microns);
                break;
            case 3:
                snprintf(line, sizeof(line), "%sHome Rail", 
                        (i == menu_selection) ? "> " : "  ");
                break;
            case 4:
                snprintf(line, sizeof(line), "%sTake Photo", 
                        (i == menu_selection) ? "> " : "  ");
                break;
        }
        
        st7735_print_text(5, 45 + i * 12, line, color, bg_color);
    }
    
    // Controls help
    st7735_print_text(5, 120, "Rotate: Move/Adjust", ST7735_GREEN, ST7735_BLACK);
    st7735_print_text(5, 130, "Press: Select/Execute", ST7735_GREEN, ST7735_BLACK);
    st7735_print_text(5, 140, "Long Press: Back", ST7735_GREEN, ST7735_BLACK);
}

// Display auto stack menu
void display_auto_stack_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    // Title
    st7735_print_text(20, 5, "AUTO STACK", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    // Menu items
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
            case 5:
                snprintf(line, sizeof(line), "%sSTART STACK", 
                        (i == menu_selection) ? "> " : "  ");
                color = (i == menu_selection) ? ST7735_BLACK : ST7735_GREEN;
                bg_color = (i == menu_selection) ? ST7735_GREEN : ST7735_BLACK;
                break;
            case 6:
                snprintf(line, sizeof(line), "%sSet Start Here", 
                        (i == menu_selection) ? "> " : "  ");
                break;
            case 7:
                snprintf(line, sizeof(line), "%sSet End Here", 
                        (i == menu_selection) ? "> " : "  ");
                break;
        }
        
        st7735_print_text(5, 25 + i * 12, line, color, bg_color);
    }
    
    // Stack info
    float distance = fabs(stack_config.end_position_mm - stack_config.start_position_mm);
    char info[32];
    snprintf(info, sizeof(info), "Distance: %.3fmm", distance);
    st7735_print_text(5, 135, info, ST7735_CYAN, ST7735_BLACK);
}

// Display settings menu
void display_settings_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    // Title
    st7735_print_text(30, 5, "SETTINGS", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    // Menu items
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
            case 6:
                snprintf(line, sizeof(line), "%sSAVE SETTINGS", 
                        (i == menu_selection) ? "> " : "  ");
                color = (i == menu_selection) ? ST7735_BLACK : ST7735_GREEN;
                bg_color = (i == menu_selection) ? ST7735_GREEN : ST7735_BLACK;
                break;
        }
        
        st7735_print_text(5, 25 + i * 12, line, color, bg_color);
    }
}

// Display stack progress
void display_stack_progress(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    // Title
    st7735_print_text(25, 5, "STACKING...", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    // Progress bar
    int progress_width = (stack_config.shots_taken * 100) / stack_config.total_shots;
    st7735_draw_rect(10, 30, 108, 20, ST7735_WHITE);
    st7735_fill_rect(12, 32, progress_width, 16, ST7735_GREEN);
    
    // Progress text
    char progress_text[32];
    snprintf(progress_text, sizeof(progress_text), "%d / %d shots", 
             stack_config.shots_taken, stack_config.total_shots);
    st7735_print_text(25, 60, progress_text, ST7735_CYAN, ST7735_BLACK);
    
    // Current position
    char pos_text[32];
    snprintf(pos_text, sizeof(pos_text), "Position: %.3fmm", 
             rail_config.current_position_mm);
    st7735_print_text(15, 80, pos_text, ST7735_WHITE, ST7735_BLACK);
    
    // Estimated time remaining
    int remaining_shots = stack_config.total_shots - stack_config.shots_taken;
    int estimated_time = (remaining_shots * stack_config.delay_ms) / 1000;
    char time_text[32];
    snprintf(time_text, sizeof(time_text), "ETA: %d:%02d", 
             estimated_time / 60, estimated_time % 60);
    st7735_print_text(30, 100, time_text, ST7735_YELLOW, ST7735_BLACK);
    
    // Stop instruction
    st7735_print_text(15, 130, "Press to STOP", ST7735_RED, ST7735_BLACK);
}

// Handle encoder rotation
void handle_encoder_rotation(int direction) {
    direction *= system_config.encoder_sensitivity;
    
    if (in_edit_mode) {
        // Handle value editing
        switch (current_menu) {
            case MENU_MANUAL_CONTROL:
                if (menu_selection == 0) {
                    // Move rail
                    float move_distance = direction * rail_config.step_size_microns / 1000.0f;
                    move_rail(move_distance);
                }
                break;
                
            case MENU_AUTO_STACK:
                // Edit stack parameters
                switch (menu_selection) {
                    case 0: // Start position
                        stack_config.start_position_mm += direction * 0.1f;
                        break;
                    case 1: // End position
                        stack_config.end_position_mm += direction * 0.1f;
                        break;
                    case 2: // Step size
                        stack_config.step_size_microns += direction * 5.0f;
                        if (stack_config.step_size_microns < 1.0f) 
                            stack_config.step_size_microns = 1.0f;
                        break;
                    case 3: // Delay
                        stack_config.delay_ms += direction * 100;
                        if (stack_config.delay_ms < 100) 
                            stack_config.delay_ms = 100;
                        break;
                    default:
                        break;
                }
                calculate_stack_shots();
                break;
                
            case MENU_SETTINGS:
                // Edit settings
                switch (menu_selection) {
                    case 0: // Brightness
                        system_config.lcd_brightness += direction * 5;
                        if (system_config.lcd_brightness < 10) 
                            system_config.lcd_brightness = 10;
                        if (system_config.lcd_brightness > 100) 
                            system_config.lcd_brightness = 100;
                        break;
                    case 1: // Trigger duration
                        system_config.camera_trigger_duration += direction * 10;
                        if (system_config.camera_trigger_duration < 50) 
                            system_config.camera_trigger_duration = 50;
                        break;
                    case 2: // Settling time
                        system_config.settling_time += direction * 50;
                        if (system_config.settling_time < 100) 
                            system_config.settling_time = 100;
                        break;
                    case 4: // Backlash compensation
                        system_config.backlash_compensation += direction * 5.0f;
                        if (system_config.backlash_compensation < 0) 
                            system_config.backlash_compensation = 0;
                        break;
                    case 5: // Encoder sensitivity
                        system_config.encoder_sensitivity += direction;
                        if (system_config.encoder_sensitivity < 1) 
                            system_config.encoder_sensitivity = 1;
                        if (system_config.encoder_sensitivity > 10) 
                            system_config.encoder_sensitivity = 10;
                        break;
                    default:
                        break;
                }
                break;
                
            case MENU_CALIBRATION:
                // Handle calibration menu editing
                switch (menu_selection) {
                    case 0: // Steps per mm
                        rail_config.steps_per_mm += direction * 10;
                        if (rail_config.steps_per_mm < 100) 
                            rail_config.steps_per_mm = 100;
                        break;
                    case 1: // Max travel
                        rail_config.max_travel_mm += direction * 1.0f;
                        if (rail_config.max_travel_mm < 10.0f) 
                            rail_config.max_travel_mm = 10.0f;
                        break;
                    case 2: // Step size
                        rail_config.step_size_microns += direction * 0.1f;
                        if (rail_config.step_size_microns < 0.1f) 
                            rail_config.step_size_microns = 0.1f;
                        break;
                    default:
                        break;
                }
                break;
                
            // Add cases for other menu states that don't have edit functionality
            case MENU_MAIN:
            case MENU_STACK_PROGRESS:
            case MENU_ADVANCED:
            default:
                // These menus don't have edit mode functionality
                break;
        }
    } else {
        // Navigate menu
        int max_items = 0;
        switch (current_menu) {
            case MENU_MAIN:
                max_items = 5;
                break;
            case MENU_MANUAL_CONTROL:
                max_items = 5;
                break;
            case MENU_AUTO_STACK:
                max_items = 8;
                break;
            case MENU_SETTINGS:
                max_items = 7;
                break;
            case MENU_CALIBRATION:
                max_items = 7;
                break;
            case MENU_STACK_PROGRESS:
                max_items = 1; // Only stop option
                break;
            case MENU_ADVANCED:
                max_items = 7;
                break;
            default:
                max_items = 1;
                break;
        }
        
        menu_selection += direction;
        if (menu_selection < 0) menu_selection = max_items - 1;
        if (menu_selection >= max_items) menu_selection = 0;
    }
    
    menu_changed = true;
}

// Handle encoder press
void handle_encoder_press(void) {
    static uint32_t last_press_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Debounce
    if (current_time - last_press_time < 200) return;
    last_press_time = current_time;
    
    if (stack_running) {
        // Stop stack
        stop_auto_stack();
        return;
    }
    
    switch (current_menu) {
        case MENU_MAIN:
            switch (menu_selection) {
                case 0:
                    current_menu = MENU_MANUAL_CONTROL;
                    menu_selection = 0;
                    break;
                case 1:
                    current_menu = MENU_AUTO_STACK;
                    menu_selection = 0;
                    break;
                case 2:
                    current_menu = MENU_SETTINGS;
                    menu_selection = 0;
                    break;
                case 3:
                    current_menu = MENU_CALIBRATION;
                    menu_selection = 0;
                    break;
                case 4:
                    current_menu = MENU_ADVANCED;
                    menu_selection = 0;
                    break;
            }
            break;
            
        case MENU_MANUAL_CONTROL:
            switch (menu_selection) {
                case 0: // Position control
                    in_edit_mode = !in_edit_mode;
                    break;
                case 3: // Home rail
                    home_rail();
                    break;
                case 4: // Take photo
                    trigger_camera();
                    break;
            }
            break;
            
        case MENU_AUTO_STACK:
            switch (menu_selection) {
                case 0: // Start position
                case 1: // End position
                case 2: // Step size
                case 3: // Delay
                    in_edit_mode = !in_edit_mode;
                    break;
                case 5: // Start stack
                    start_auto_stack();
                    break;
                case 6: // Set start here
                    stack_config.start_position_mm = rail_config.current_position_mm;
                    calculate_stack_shots();
                    break;
                case 7: // Set end here
                    stack_config.end_position_mm = rail_config.current_position_mm;
                    calculate_stack_shots();
                    break;
            }
            break;
            
        case MENU_SETTINGS:
            switch (menu_selection) {
                case 0: // Brightness
                case 1: // Trigger duration
                case 2: // Settling time
                case 4: // Backlash compensation
                case 5: // Encoder sensitivity
                    in_edit_mode = !in_edit_mode;
                    break;
                case 3: // Beep toggle
                    system_config.beep_enabled = !system_config.beep_enabled;
                    break;
                case 6: // Save settings
                    save_settings();
                    break;
            }
            break;
            
        case MENU_CALIBRATION:
            switch (menu_selection) {
                case 0: // Steps per mm
                case 1: // Max travel
                case 2: // Step size
                    in_edit_mode = !in_edit_mode;
                    break;
                case 3: // Test move 1mm
                    move_rail(1.0f);
                    break;
                case 4: // Test move 10mm
                    move_rail(10.0f);
                    break;
                case 5: // Reset position
                    rail_config.current_position_mm = 0.0f;
                    rail_config.total_steps = 0;
                    break;
                case 6: // Back to main
                    current_menu = MENU_MAIN;
                    menu_selection = 0;
                    break;
            }
            break;
            
        case MENU_STACK_PROGRESS:
            // Stop stack if pressed during progress
            stop_auto_stack();
            break;
            
        case MENU_ADVANCED:
            // For now, just go back to main menu for any selection
            current_menu = MENU_MAIN;
            menu_selection = 0;
            break;
    }
    
    menu_changed = true;
}


// Initialize rotary encoder
void encoder_init(void) {
    // Configure encoder pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ENCODER_CLK_PIN) | (1ULL << ENCODER_DT_PIN) | (1ULL << ENCODER_SW_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);
    
    // Install interrupt service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_CLK_PIN, encoder_isr, NULL);
    gpio_isr_handler_add(ENCODER_SW_PIN, encoder_sw_isr, NULL);
}

// Encoder rotation ISR
static void IRAM_ATTR encoder_isr(void* arg) {
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    
    // Debounce
    if (current_time - last_encoder_time < 5) return;
    last_encoder_time = current_time;
    
    int clk_state = gpio_get_level(ENCODER_CLK_PIN);
    int dt_state = gpio_get_level(ENCODER_DT_PIN);
    
    if (clk_state != dt_state) {
        encoder_counter++;
    } else {
        encoder_counter--;
    }
}

// Encoder switch ISR
static void IRAM_ATTR encoder_sw_isr(void* arg) {
    if (gpio_get_level(ENCODER_SW_PIN) == 0) {
        encoder_pressed = true;
    }
}

// Initialize stepper motor
void stepper_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << STEPPER_STEP_PIN) | (1ULL << STEPPER_DIR_PIN) | (1ULL << STEPPER_EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Enable stepper driver
    gpio_set_level(STEPPER_EN_PIN, 0);
}

// Initialize camera trigger
void camera_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << CAMERA_TRIGGER_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    gpio_set_level(CAMERA_TRIGGER_PIN, 0);
}

// Move rail by specified distance in mm
void move_rail(float distance_mm) {
    if (!rail_config.homed && distance_mm != 0) {
        // Show warning message
        st7735_fill_rect(10, 60, 108, 40, ST7735_RED);
        st7735_print_text(15, 70, "RAIL NOT HOMED!", ST7735_WHITE, ST7735_RED);
        st7735_print_text(20, 85, "Home first", ST7735_WHITE, ST7735_RED);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        menu_changed = true;
        return;
    }
    
    int32_t steps = (int32_t)(distance_mm * rail_config.steps_per_mm);
    move_rail_steps(steps);
    rail_config.current_position_mm += distance_mm;
    
    // Bounds checking
    if (rail_config.current_position_mm < 0) {
        rail_config.current_position_mm = 0;
    }
    if (rail_config.current_position_mm > rail_config.max_travel_mm) {
        rail_config.current_position_mm = rail_config.max_travel_mm;
    }
}

// Move rail by specified number of steps
void move_rail_steps(int32_t steps) {
    if (steps == 0) return;
    
    // Set direction
    gpio_set_level(STEPPER_DIR_PIN, steps > 0 ? 1 : 0);
    vTaskDelay(pdMS_TO_TICKS(1));

    // Apply backlash compensation for direction changes
    static int last_direction = 0;
    int current_direction = steps > 0 ? 1 : -1;
    
    if (last_direction != 0 && last_direction != current_direction) {
        int32_t backlash_steps = (int32_t)(system_config.backlash_compensation * 
                                          rail_config.steps_per_mm / 1000.0f);
        
        // Move extra steps to take up backlash
        for (int32_t i = 0; i < backlash_steps; i++) {
            gpio_set_level(STEPPER_STEP_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(STEPPER_STEP_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
    last_direction = current_direction;
    
    // Move the requested steps
    int32_t abs_steps = abs(steps);
    for (int32_t i = 0; i < abs_steps; i++) {
        gpio_set_level(STEPPER_STEP_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(STEPPER_STEP_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    rail_config.total_steps += steps;
    
    // Settling time
    vTaskDelay(system_config.settling_time / portTICK_PERIOD_MS);
}

// Trigger camera
void trigger_camera(void) {
    // Show trigger indication
    st7735_fill_rect(30, 70, 68, 20, ST7735_GREEN);
    st7735_print_text(40, 78, "TRIGGER!", ST7735_BLACK, ST7735_GREEN);
    
    // Trigger camera
    gpio_set_level(CAMERA_TRIGGER_PIN, 1);
    vTaskDelay(system_config.camera_trigger_duration / portTICK_PERIOD_MS);
    gpio_set_level(CAMERA_TRIGGER_PIN, 0);
    
    // Clear indication
    vTaskDelay(500 / portTICK_PERIOD_MS);
    menu_changed = true;
}

// Home the rail
void home_rail(void) {
    st7735_fill_screen(ST7735_BLACK);
    st7735_print_text(30, 60, "HOMING...", ST7735_YELLOW, ST7735_BLACK);
    st7735_print_text(15, 80, "Move to position 0", ST7735_WHITE, ST7735_BLACK);
    st7735_print_text(20, 95, "Press when ready", ST7735_WHITE, ST7735_BLACK);
    
    // Wait for user confirmation
    while (!encoder_pressed) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    encoder_pressed = false;
    
    // Set home position
    rail_config.current_position_mm = 0.0f;
    rail_config.total_steps = 0;
    rail_config.homed = true;
    
    // Confirmation
    st7735_fill_rect(20, 110, 88, 15, ST7735_GREEN);
    st7735_print_text(35, 115, "HOMED!", ST7735_BLACK, ST7735_GREEN);
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    
    menu_changed = true;
}

// Start automatic focus stacking
void start_auto_stack(void) {
    if (!rail_config.homed) {
        st7735_fill_rect(10, 60, 108, 40, ST7735_RED);
        st7735_print_text(15, 70, "RAIL NOT HOMED!", ST7735_WHITE, ST7735_RED);
        st7735_print_text(20, 85, "Home first", ST7735_WHITE, ST7735_RED);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        menu_changed = true;
        return;
    }
    
    if (stack_config.total_shots <= 0) {
        st7735_fill_rect(10, 60, 108, 40, ST7735_RED);
        st7735_print_text(15, 70, "INVALID STACK", ST7735_WHITE, ST7735_RED);
        st7735_print_text(20, 85, "Check settings", ST7735_WHITE, ST7735_RED);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        menu_changed = true;
        return;
    }
    
    // Confirmation dialog
    st7735_fill_screen(ST7735_BLACK);
    st7735_print_text(25, 40, "START STACK?", ST7735_YELLOW, ST7735_BLACK);
    
    char info1[32];
    snprintf(info1, sizeof(info1), "%d shots", stack_config.total_shots);
    st7735_print_text(40, 60, info1, ST7735_WHITE, ST7735_BLACK);
    
    char info2[32];
    float distance = fabs(stack_config.end_position_mm - stack_config.start_position_mm);
    snprintf(info2, sizeof(info2), "%.2fmm range", distance);
    st7735_print_text(30, 75, info2, ST7735_WHITE, ST7735_BLACK);
    
    st7735_print_text(15, 100, "Press: Start", ST7735_GREEN, ST7735_BLACK);
    st7735_print_text(15, 115, "Long Press: Cancel", ST7735_RED, ST7735_BLACK);
    
    // Wait for confirmation
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    while (true) {
        if (encoder_pressed) {
            encoder_pressed = false;
            break;
        }
        
        // Timeout after 10 seconds
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (current_time - start_time > 10000) {
            menu_changed = true;
            return;
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    
    // Initialize stack
    stack_config.shots_taken = 0;
    stack_running = true;
    current_menu = MENU_STACK_PROGRESS;
    
    // Move to start position
    float move_distance = stack_config.start_position_mm - rail_config.current_position_mm;
    move_rail(move_distance);
    
    // Create stack task
    xTaskCreate(auto_stack_task, "auto_stack", 4096, NULL, 6, NULL);
    
    menu_changed = true;
}

// Auto stack task
void auto_stack_task(void *pvParameters) {
    float step_distance_mm = stack_config.step_size_microns / 1000.0f;
    bool direction_forward = stack_config.end_position_mm > stack_config.start_position_mm;
    
    if (stack_config.reverse_direction) {
        direction_forward = !direction_forward;
    }
    
    if (!direction_forward) {
        step_distance_mm = -step_distance_mm;
    }
    
    for (int shot = 0; shot < stack_config.total_shots && stack_running; shot++) {
        // Take photo
        trigger_camera();
        
        stack_config.shots_taken = shot + 1;
        
        // Move to next position (except for last shot)
        if (shot < stack_config.total_shots - 1) {
            move_rail(step_distance_mm);
            
            // Inter-shot delay
            vTaskDelay(stack_config.delay_ms / portTICK_PERIOD_MS);
        }
        
        // Check if we should stop
        if (!stack_running) break;
    }
    
    // Stack complete
    if (stack_running) {
        // Return to start if requested
        if (stack_config.return_to_start) {
            float return_distance = stack_config.start_position_mm - rail_config.current_position_mm;
            move_rail(return_distance);
        }
        
        // Show completion message
        st7735_fill_screen(ST7735_BLACK);
        st7735_print_text(25, 60, "STACK COMPLETE!", ST7735_GREEN, ST7735_BLACK);
        
        char complete_msg[32];
        snprintf(complete_msg, sizeof(complete_msg), "%d shots taken", stack_config.shots_taken);
        st7735_print_text(20, 80, complete_msg, ST7735_WHITE, ST7735_BLACK);
        
        st7735_print_text(20, 100, "Press to continue", ST7735_CYAN, ST7735_BLACK);
        
        // Wait for user acknowledgment
        encoder_pressed = false;
        while (!encoder_pressed) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        encoder_pressed = false;
    }
    
    // Return to auto stack menu
    stack_running = false;
    current_menu = MENU_AUTO_STACK;
    menu_changed = true;
    
    vTaskDelete(NULL);
}

// Stop auto stacking
void stop_auto_stack(void) {
    stack_running = false;
    
    // Show stop message
    st7735_fill_rect(20, 60, 88, 40, ST7735_RED);
    st7735_print_text(35, 70, "STOPPING...", ST7735_WHITE, ST7735_RED);
    st7735_print_text(30, 85, "Please wait", ST7735_WHITE, ST7735_RED);
    
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    current_menu = MENU_AUTO_STACK;
    menu_changed = true;
}

// Calculate total shots for stack
void calculate_stack_shots(void) {
    float distance = fabs(stack_config.end_position_mm - stack_config.start_position_mm);
    float step_distance = stack_config.step_size_microns / 1000.0f;
    
    if (distance > 0 && step_distance > 0) {
        stack_config.total_shots = (int)(distance / step_distance) + 1;
    } else {
        stack_config.total_shots = 0;
    }
}

// Update position display
void update_position_display(void) {
    static float last_position = -999.0f;
    
    if (fabs(rail_config.current_position_mm - last_position) > 0.001f) {
        last_position = rail_config.current_position_mm;
        menu_changed = true;
    }
}

// Display calibration menu
void display_calibration_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    st7735_print_text(20, 5, "CALIBRATION", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    const char* cal_items[] = {
        "Steps/mm:",
        "Max Travel:",
        "Step Size:",
        "Test Move 1mm",
        "Test Move 10mm",
        "Reset Position",
        "Back to Main"
    };
    
    for (int i = 0; i < 7; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;
        
        char line[32];
        switch (i) {
            case 0:
                snprintf(line, sizeof(line), "%s%s %ld", 
                        (i == menu_selection) ? "> " : "  ", 
                        cal_items[i], rail_config.steps_per_mm);
                break;
            case 1:
                snprintf(line, sizeof(line), "%s%s %.1fmm", 
                        (i == menu_selection) ? "> " : "  ", 
                        cal_items[i], rail_config.max_travel_mm);
                break;
            case 2:
                snprintf(line, sizeof(line), "%s%s %.2fum", 
                        (i == menu_selection) ? "> " : "  ", 
                        cal_items[i], rail_config.step_size_microns);
                break;
            default:
                snprintf(line, sizeof(line), "%s%s", 
                        (i == menu_selection) ? "> " : "  ", 
                        cal_items[i]);
                break;
        }
        
        st7735_print_text(5, 25 + i * 12, line, color, bg_color);
    }
    
    // Current settings info
    char info[32];
    snprintf(info, sizeof(info), "Pos: %.3fmm", rail_config.current_position_mm);
    st7735_print_text(5, 140, info, ST7735_GREEN, ST7735_BLACK);
}

// Display advanced menu
void display_advanced_menu(void) {
    st7735_fill_screen(ST7735_BLACK);
    
    st7735_print_text(25, 5, "ADVANCED", ST7735_WHITE, ST7735_BLACK);
    st7735_draw_line(0, 15, ST7735_WIDTH, 15, ST7735_WHITE);
    
    const char* adv_items[] = {
        "Focus Peak Mode",
        "Exposure Bracket",
        "Time Lapse",
        "Custom Patterns",
        "Motor Tuning",
        "Statistics",
        "Factory Reset"
    };
    
    for (int i = 0; i < 7; i++) {
        uint16_t color = (i == menu_selection) ? ST7735_YELLOW : ST7735_WHITE;
        uint16_t bg_color = (i == menu_selection) ? ST7735_BLUE : ST7735_BLACK;
        
        char line[32];
        snprintf(line, sizeof(line), "%s%s", 
                (i == menu_selection) ? "> " : "  ", 
                adv_items[i]);
        
        st7735_print_text(5, 25 + i * 12, line, color, bg_color);
    }
    
    st7735_print_text(5, 140, "Under Development", ST7735_CYAN, ST7735_BLACK);
}

// Save settings to non-volatile storage
void save_settings(void) {
    // In a real implementation, this would save to EEPROM or flash
    st7735_fill_rect(20, 60, 88, 25, ST7735_GREEN);
    st7735_print_text(30, 68, "SETTINGS", ST7735_BLACK, ST7735_GREEN);
    st7735_print_text(35, 78, "SAVED!", ST7735_BLACK, ST7735_GREEN);
    
    vTaskDelay(1500 / portTICK_PERIOD_MS);
    menu_changed = true;
}

// Load settings from non-volatile storage
void load_settings(void) {
    // In a real implementation, this would load from EEPROM or flash
    // For now, we use default values already set in the structures
}

// Example main function to tie everything together
void app_main(void) {
    display_init();  // Use Adafruit driver

    // Optional: show splash screen
    tft.setCursor(10, 40);
    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(2);
    tft.print("Macro Rail");

    vTaskDelay(pdMS_TO_TICKS(2000));
    menu_init();
}