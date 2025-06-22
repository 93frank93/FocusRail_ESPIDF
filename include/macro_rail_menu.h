// macro_rail_menu.h
#ifndef MACRO_RAIL_MENU_H
#define MACRO_RAIL_MENU_H

#include "st7735_lcd.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Rotary encoder pins
#define ENCODER_CLK_PIN  25 // pin A
#define ENCODER_DT_PIN   26 // pin B
#define ENCODER_SW_PIN   27 // SW
#define ENCODER_VCC_PIN  32  // Optional power control
#define ENCODER_GND_PIN  33  // Optional ground control

// Stepper motor control pins
#define STEPPER_STEP_PIN 12
#define STEPPER_DIR_PIN  13
#define STEPPER_EN_PIN   14

// Camera trigger pin
#define CAMERA_TRIGGER_PIN 15

// Menu states
typedef enum {
    MENU_MAIN = 0,
    MENU_MANUAL_CONTROL,
    MENU_AUTO_STACK,
    MENU_SETTINGS,
    MENU_CALIBRATION,
    MENU_STACK_PROGRESS,
    MENU_ADVANCED
} menu_state_t;

// Focus rail configuration
typedef struct {
    float step_size_microns;      // Microns per step
    float max_travel_mm;          // Maximum travel distance
    float current_position_mm;    // Current position
    int32_t total_steps;          // Total steps moved
    int32_t steps_per_mm;         // Steps per millimeter
    bool homed;                   // Has been homed
} rail_config_t;

// Auto stack settings
typedef struct {
    float start_position_mm;      // Stack start position
    float end_position_mm;        // Stack end position
    float step_size_microns;      // Step size between shots
    int total_shots;              // Calculated total shots
    int shots_taken;              // Current shot count
    int delay_ms;                 // Delay between shots
    bool reverse_direction;       // Stack direction
    bool return_to_start;         // Return to start after stack
} stack_config_t;

// System settings
typedef struct {
    int lcd_brightness;           // LCD brightness (0-100)
    int camera_trigger_duration;  // Trigger pulse duration (ms)
    int settling_time;            // Motor settling time (ms)
    bool beep_enabled;            // Enable beeper
    float backlash_compensation;  // Backlash compensation (microns)
    int encoder_sensitivity;      // Encoder sensitivity multiplier
} system_config_t;

// Function prototypes
void menu_init(void);
void menu_task(void *pvParameters);
void encoder_init(void);
void stepper_init(void);
void camera_init(void);

// Menu functions
void display_main_menu(void);
void display_manual_control(void);
void display_auto_stack_menu(void);
void display_settings_menu(void);
void display_calibration_menu(void);
void display_stack_progress(void);
void display_advanced_menu(void);

// Control functions
void handle_encoder_rotation(int direction);
void handle_encoder_press(void);
void move_rail(float distance_mm);
void move_rail_steps(int32_t steps);
void trigger_camera(void);
void home_rail(void);
void start_auto_stack(void);
void stop_auto_stack(void);

// Utility functions
void update_position_display(void);
void calculate_stack_shots(void);
void save_settings(void);
void load_settings(void);

// Global variables
extern menu_state_t current_menu;
extern int menu_selection;
extern rail_config_t rail_config;
extern stack_config_t stack_config;
extern system_config_t system_config;
extern bool stack_running;

#endif // MACRO_RAIL_MENU_H