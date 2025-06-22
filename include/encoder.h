#ifndef ENCODER_H
#define ENCODER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Pin definitions
#define ENCODER_A_PIN       GPIO_NUM_18  // Out A
#define ENCODER_B_PIN       GPIO_NUM_19  // Out B  
#define ENCODER_SW_PIN      GPIO_NUM_21  // SW (switch)

// Encoder event structure
typedef struct {
    int direction;  // 1 for CW, -1 for CCW
    bool button_pressed;
} encoder_event_t;

// Global encoder queue (declared in encoder.c)
extern QueueHandle_t encoder_queue;

// Function prototypes
void encoder_init(void);
void encoder_task(void *pvParameters);

#endif // ENCODER_H