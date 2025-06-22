#ifndef STEPPER_H
#define STEPPER_H

#include <stdbool.h>

// Stepper motor pins
#define STEP_PIN            GPIO_NUM_25
#define DIR_PIN             GPIO_NUM_26
#define ENABLE_PIN          GPIO_NUM_27

// Global variables (declared in stepper.c)
extern int focus_position;
extern bool motor_enabled;

// Function prototypes
void stepper_init(void);
void stepper_enable(bool enable);
void stepper_move(int steps);
int stepper_get_position(void);
void stepper_reset_position(void);
bool stepper_is_enabled(void);

#endif // STEPPER_H