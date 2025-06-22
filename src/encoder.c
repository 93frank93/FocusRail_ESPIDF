#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "encoder.h"
#include "menu.h"

static const char *TAG = "ENCODER";

// Global variables
QueueHandle_t encoder_queue;

// Rotary encoder ISR handler (handles both A and B pins)
static void IRAM_ATTR encoder_rotary_isr_handler(void *arg) {
    static uint8_t last_state = 0;
    static const int8_t transition_table[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0
    };

    int a = gpio_get_level(ENCODER_A_PIN);
    int b = gpio_get_level(ENCODER_B_PIN);

    uint8_t current_state = (a << 1) | b;
    uint8_t state_transition = (last_state << 2) | current_state;

    int8_t direction = transition_table[state_transition & 0x0F];
    last_state = current_state;

    if (direction != 0) {
        encoder_event_t event = {0};
        event.direction = direction;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xQueueSendFromISR(encoder_queue, &event, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// Button switch ISR handler with debounce
static void IRAM_ATTR encoder_switch_isr_handler(void *arg) {
    static uint32_t last_interrupt_time = 0;
    uint32_t interrupt_time = esp_timer_get_time() / 1000;  // Convert to ms

    if (interrupt_time - last_interrupt_time > 50) {  // 50 ms debounce
        int sw_state = gpio_get_level(ENCODER_SW_PIN);
        if (sw_state == 0) {  // Button pressed (active low)
            encoder_event_t event = {0};
            event.button_pressed = true;
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(encoder_queue, &event, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR();
            }
        }
        last_interrupt_time = interrupt_time;
    }
}

void encoder_init(void) {
    // Configure encoder pins
    gpio_config_t io_conf = {};

    // Rotary encoder pins A and B as input with pull-up and any edge interrupt
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ENCODER_A_PIN) | (1ULL << ENCODER_B_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // Switch pin as input with pull-up and any edge interrupt
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << ENCODER_SW_PIN);
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    // Install ISR service
    gpio_install_isr_service(0);

    // Attach rotary encoder ISR handler to A and B pins
    gpio_isr_handler_add(ENCODER_A_PIN, encoder_rotary_isr_handler, (void*)ENCODER_A_PIN);
    gpio_isr_handler_add(ENCODER_B_PIN, encoder_rotary_isr_handler, (void*)ENCODER_B_PIN);

    // Attach switch ISR handler to switch pin
    gpio_isr_handler_add(ENCODER_SW_PIN, encoder_switch_isr_handler, (void*)ENCODER_SW_PIN);

    // Create the queue to hold encoder events
    encoder_queue = xQueueCreate(10, sizeof(encoder_event_t));

    ESP_LOGI(TAG, "Encoder initialized");
}

void encoder_task(void *pvParameters) {
    encoder_event_t event;

    ESP_LOGI(TAG, "Encoder task started");

    int accumulated_direction = 0;
    int64_t last_event_time = 0;
    const int64_t aggregation_timeout_ms = 150;

    while (1) {
        if (xQueueReceive(encoder_queue, &event, pdMS_TO_TICKS(aggregation_timeout_ms))) {
            if (event.direction != 0) {
                accumulated_direction += event.direction;
                last_event_time = esp_timer_get_time() / 1000;
                ESP_LOGI(TAG, "Accumulated direction: %d", accumulated_direction);
            } else if (event.button_pressed) {
                // Flush any accumulated direction before handling button
                if (accumulated_direction != 0) {
                    encoder_event_t dir_event = {0};
                    dir_event.direction = (accumulated_direction > 0) ? 1 : -1;
                    menu_handle_input(&dir_event);
                    accumulated_direction = 0;
                }
                ESP_LOGI(TAG, "Button pressed");
                menu_handle_input(&event);
            }
        } else {
            // Timeout occurred, no new events: handle accumulated direction if any
            if (accumulated_direction != 0) {
                encoder_event_t dir_event = {0};
                dir_event.direction = (accumulated_direction > 0) ? 1 : -1;
                ESP_LOGI(TAG, "Processing accumulated direction: %d", dir_event.direction);
                menu_handle_input(&dir_event);
                accumulated_direction = 0;
            }
        }
    }
}