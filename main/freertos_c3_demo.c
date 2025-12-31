#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "RTOS_C3";

void task1(void *arg)
{
    gpio_config_t io_conf;
    esp_err_t res;
    uint32_t staute = 0;
    TickType_t last = xTaskGetTickCount();      // 绝对计时定时器

    io_conf.pin_bit_mask =  (1ULL << GPIO_NUM_1);
    io_conf.mode = GPIO_MODE_OUTPUT;
    res = gpio_config(&io_conf);

    if(res == ESP_OK)
    {
        ESP_LOGI(TAG, "GPIO configured successfully!");
        gpio_set_level(GPIO_NUM_1, staute);
    }
    else
    {
        ESP_LOGE(TAG, "GPIO configuration failed!");
    }
    while (1) 
    {
        ESP_LOGI(TAG, "Task1 running on C3");
        if(res == ESP_OK)
        {
            staute = 1UL - staute;
            gpio_set_level(GPIO_NUM_1, staute);
        }
        // | API             | 用途                 |
        // | --------------- | ----------           |
        // | vTaskDelay      | 相对延时             |
        // | vTaskDelayUntil | 绝对周期（强烈推荐） |
        // vTaskDelay(pdMS_TO_TICKS(5000));
        vTaskDelayUntil(&last, pdMS_TO_TICKS(4000));
    }
}

QueueHandle_t q;

void producer(void *arg)
{
    int cnt = 0;
    while (1) {
        xQueueSend(q, &cnt, portMAX_DELAY);
        cnt++;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void consumer(void *arg)
{
    int val;
    while (1) {
        if (xQueueReceive(q, &val, portMAX_DELAY)) {
            ESP_LOGI("CONS", "val=%d", val);
        }
    }
}


void app_main(void)
{
    q = xQueueCreate(10, sizeof(int));
    configASSERT(q);
    xTaskCreate(task1, "task1", 4096, NULL, 4, NULL);
    xTaskCreate(producer, "producer", 4096, NULL, 5, NULL);
    xTaskCreate(consumer, "consumer", 4096, NULL, 6, NULL);

}
