#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "freertos/queue.h"
#include "driver/uart.h"

#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"

static const char *TAG = "RTOS_C3";


// #define TAG "UART_ISR_DEMO"

#define UART_PORT      UART_NUM_1
#define UART_TX_PIN    GPIO_NUM_21
#define UART_RX_PIN    GPIO_NUM_20
#define UART_BUF_SIZE  1024

QueueHandle_t uart_rx_queue;
static void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, UART_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

static void uart_tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        sendData(TX_TASK_TAG, "Hello world");
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}


static void uart_rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(UART_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_lbytes(UART_NUM_1, data, UART_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}


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
    // uart_rx_queue = xQueueCreate(64, sizeof(uint8_t));

    uart_init();
    // uart_isr_init();

    // configASSERT(uart_rx_queue);
    configASSERT(q);
    xTaskCreate(task1, "task1", 4096, NULL, 4, NULL);
    xTaskCreate(producer, "producer", 4096, NULL, 5, NULL);
    xTaskCreate(consumer, "consumer", 4096, NULL, 6, NULL);
    xTaskCreate(uart_tx_task, "uart_task_sender", 4096, NULL, 6, NULL);
    xTaskCreate(uart_rx_task, "uart_task_recive", 4096, NULL, 7, NULL);
}
