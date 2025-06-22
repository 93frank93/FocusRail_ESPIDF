#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "encoder.h"
#include "display.h"
#include "stepper.h"
#include "menu.h"

static const char *TAG = "FOCUS_RAIL";

void app_main(void) {
    ESP_LOGI(TAG, "Starting Focus Rail Controller");
    
    // Initialize components
    encoder_init();
    display_init();
    stepper_init();
    menu_init();
    
    ESP_LOGI(TAG, "Hardware initialized");
    
    // Create tasks
    xTaskCreate(encoder_task, "encoder_task", 4096, NULL, 10, NULL);
    xTaskCreate(menu_task, "menu_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Tasks created, system ready");
    
    // Display welcome message
    display_welcome();
    
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    menu_display();
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}