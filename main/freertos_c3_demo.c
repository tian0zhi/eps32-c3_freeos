#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "RTOS_C3";

void task1(void *arg)
{
    gpio_config_t io_conf;
    io_conf.pin_bit_mask =  (1ULL << GPIO_NUM_1);
    io_conf.mode = GPIO_MODE_OUTPUT;
    esp_err_t res = gpio_config(&io_conf);
    uint32_t staute = 0;
    if(res == ESP_OK)
    {
        ESP_LOGI(TAG, "GPIO configured");
        gpio_set_level(GPIO_NUM_1, staute);
    }
    while (1) {
        ESP_LOGI(TAG, "Task1 running on C3");
        if(res == ESP_OK)
        {
            staute = 1UL - staute;
            gpio_set_level(GPIO_NUM_1, staute);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    xTaskCreate(
        task1,
        "task1",
        4096,
        NULL,
        5,
        NULL
    );
}
