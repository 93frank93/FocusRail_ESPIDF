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

// Function prototypes
static void IRAM_ATTR encoder_isr_handler(void *arg);

void encoder_init(void) {
    // Configure encoder pins
    gpio_config_t io_conf = {};
    
    // Out A and Out B pins as input with pull-up
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << ENCODER_A_PIN) | (1ULL << ENCODER_B_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    
    // SW pin as input with pull-up
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = (1ULL << ENCODER_SW_PIN);
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
    
    // Install ISR service
    gpio_install_isr_service(0);
    gpio_isr_handler_add(ENCODER_A_PIN, encoder_isr_handler, (void*)ENCODER_A_PIN);
    gpio_isr_handler_add(ENCODER_B_PIN, encoder_isr_handler, (void*)ENCODER_B_PIN);
    gpio_isr_handler_add(ENCODER_SW_PIN, encoder_isr_handler, (void*)ENCODER_SW_PIN);
    
    // Create queue for encoder events
    encoder_queue = xQueueCreate(10, sizeof(encoder_event_t));
    
    ESP_LOGI(TAG, "Encoder initialized");
}

static void IRAM_ATTR encoder_isr_handler(void *arg) {
    static uint32_t last_interrupt_time = 0;
    uint32_t interrupt_time = esp_timer_get_time() / 1000; // Convert to ms
    
    // Debounce
    if (interrupt_time - last_interrupt_time < 5) {
        return;
    }
    last_interrupt_time = interrupt_time;
    
    encoder_event_t event = {0};
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    uint32_t gpio_num = (uint32_t)arg;
    
    if (gpio_num == ENCODER_SW_PIN) {
        // Detect button press (falling edge if pull-up, rising edge if pull-down)
        int sw_state = gpio_get_level(ENCODER_SW_PIN);
        if (sw_state == 0) {  // Button pressed (assuming active low)
            event.button_pressed = true;
            xQueueSendFromISR(encoder_queue, &event, &xHigherPriorityTaskWoken);
        }
    } else if (gpio_num == ENCODER_A_PIN) {
        int a_state = gpio_get_level(ENCODER_A_PIN);
        int b_state = gpio_get_level(ENCODER_B_PIN);
        
        if (a_state != b_state) {
            event.direction = 1;  // Clockwise
        } else {
            event.direction = -1; // Counter-clockwise
        }
        xQueueSendFromISR(encoder_queue, &event, &xHigherPriorityTaskWoken);
    }
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void encoder_task(void *pvParameters) {
    encoder_event_t event;
    
    ESP_LOGI(TAG, "Encoder task started");
    
    while (1) {
        if (xQueueReceive(encoder_queue, &event, portMAX_DELAY)) {
            menu_handle_input(&event);
        }
    }
}