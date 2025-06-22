#include "stepper.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "STEPPER";

// Global variables
int focus_position = 0;
bool motor_enabled = false;

void stepper_init(void) {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << STEP_PIN) | (1ULL << DIR_PIN) | (1ULL << ENABLE_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Initially disable motor
    gpio_set_level(ENABLE_PIN, 1);  // Active low enable
    gpio_set_level(STEP_PIN, 0);
    gpio_set_level(DIR_PIN, 0);
    
    motor_enabled = false;
    focus_position = 0;
    
    ESP_LOGI(TAG, "Stepper motor initialized");
}

void stepper_enable(bool enable) {
    gpio_set_level(ENABLE_PIN, enable ? 0 : 1);  // Active low
    motor_enabled = enable;
    
    ESP_LOGI(TAG, "Stepper motor %s", enable ? "enabled" : "disabled");
}

void stepper_move(int steps) {
    if (!motor_enabled) {
        ESP_LOGW(TAG, "Motor is disabled, cannot move");
        return;
    }
    
    if (steps == 0) {
        return;
    }
    
    gpio_set_level(DIR_PIN, steps > 0 ? 1 : 0);
    int abs_steps = abs(steps);
    
    ESP_LOGI(TAG, "Moving %d steps %s", abs_steps, steps > 0 ? "forward" : "backward");
    
    for (int i = 0; i < abs_steps; i++) {
        gpio_set_level(STEP_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(2));  // 2ms pulse
        gpio_set_level(STEP_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(2));  // 2ms delay
    }
    
    focus_position += steps;
    
    ESP_LOGD(TAG, "Move complete. New position: %d", focus_position);
}

int stepper_get_position(void) {
    return focus_position;
}

void stepper_reset_position(void) {
    focus_position = 0;
    ESP_LOGI(TAG, "Position reset to 0");
}

bool stepper_is_enabled(void) {
    return motor_enabled;
}