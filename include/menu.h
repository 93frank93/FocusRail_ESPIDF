#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include <stdbool.h>
#include "encoder.h"

// Menu states
typedef enum {
    MENU_MAIN = 0,
    MENU_MOVE,
    MENU_SETTINGS,
    MENU_AUTO_STACK
} menu_state_t;

// Menu configuration structure
typedef struct {
    int focus_position;
    int step_size;
    bool motor_enabled;
    menu_state_t current_menu;
    int menu_selection;
} menu_config_t;

// Function prototypes
void menu_init(void);
void menu_handle_input(encoder_event_t *event);
void menu_display(void);
void menu_task(void *pvParameters);
void menu_set_motor_enabled(bool enabled);
void menu_set_focus_position(int position);
void menu_update_focus_position(int delta);
int menu_get_focus_position(void);
int menu_get_step_size(void);
bool menu_get_motor_enabled(void);

// Callback function type for stepper motor control
typedef void (*stepper_move_callback_t)(int steps);
typedef void (*stepper_enable_callback_t)(bool enable);

// Set callback functions for hardware control
void menu_set_stepper_callbacks(stepper_move_callback_t move_cb, stepper_enable_callback_t enable_cb);

#endif // MENU_H