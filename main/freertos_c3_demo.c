#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define TAG "GPIO_NOTIFY_DEMO"

#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include <stdatomic.h>

#include <string.h>

#include "lwip/sockets.h"
#include "lwip/inet.h"

// 网络管理部分
static const char *WIFI_TAG = "WIFI";

typedef enum {
    WIFI_DISCONNECTED = 0,
    WIFI_CONNECTING,
    WIFI_CONNECTED
} wifi_state_t;

volatile wifi_state_t g_wifi_state = WIFI_DISCONNECTED;
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {

        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(WIFI_TAG, "WiFi started");
            g_wifi_state = WIFI_CONNECTING;
            esp_wifi_connect();
        }

        else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(WIFI_TAG, "WiFi disconnected");
            g_wifi_state = WIFI_DISCONNECTED;
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_wifi_state = WIFI_CONNECTED;
    }
}

void wifi_init_sta(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "CU_aW7P",
            .password = "rqnumgrn",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

void wifi_monitor_task(void *arg)
{
    while (1) {
        if (g_wifi_state == WIFI_DISCONNECTED) {
            ESP_LOGW("WIFI_MON", "WiFi lost, reconnecting...");
            g_wifi_state = WIFI_CONNECTING;
            esp_wifi_connect();
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // 每5秒检查一次
    }
}

// GPIO 灯控部分
/* GPIO 定义 */
#define GPIO_INPUT      GPIO_NUM_4
#define GPIO_LED        GPIO_NUM_1

/* 任务句柄 */
static TaskHandle_t led_task_handle = NULL;

/* ==================== LED 任务 ==================== */
static void led_task(void *arg)
{
    uint32_t notify_value;
    uint32_t level;

    /* 初始化 LED GPIO */
    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << GPIO_LED,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);

    level = 0;

    gpio_set_level(GPIO_LED, level);  // 初始熄灭

    ESP_LOGI(TAG, "LED task started, waiting for notification...");

    while (1)
    {
        level = 1 - level;       // 切换 LED 状态
        /* 阻塞等待 ISR 通知 */
        xTaskNotifyWait(
            0,                  // 不清除进入时的通知
            0xFFFFFFFF,         // 退出时清除所有通知
            &notify_value,      // 接收通知值
            portMAX_DELAY       // 一直等
        );

        ESP_LOGI(TAG, "Notification received, turn LED change");

        gpio_set_level(GPIO_LED, level);   // 点亮 LED
    }
}

/* ==================== GPIO 中断处理函数 ==================== */
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* 向 LED 任务发送通知 */
    xTaskNotifyFromISR(
        led_task_handle,           // 要通知的任务
        1,                          // 通知值（这里值本身无所谓）
        eSetValueWithOverwrite,     // 覆盖方式
        &xHigherPriorityTaskWoken
    );

    /* 如果唤醒了更高优先级任务，立刻切换 */
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

// GPIO PWM 部分
void pwm_ctrl_task(void *arg)
{
    TickType_t lastWake = xTaskGetTickCount();

    while (1) {
        // 开启 PWM
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        printf("PWM ON\n");

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1000));

        // 停止 PWM
        ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        printf("PWM OFF\n");

        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10000));
    }
}



void pwm_hw_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 2000,        // 2 kHz
        .duty_resolution = LEDC_TIMER_10_BIT,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = 2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = 512, // 50%
    };
    ledc_channel_config(&ch);
    ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    printf("PWM OFF\n");
}

// 周期性任务，模拟中断触发
void PeriodTask_isr(void *param)
{
    TickType_t lastWake = xTaskGetTickCount();

    while (1)
    {
        gpio_isr_handler(NULL);
        // 周期性执行
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(10000));
    }
}

// 网络任务部分

#define UDP_PORT 3358
#define BUF_SIZE 512

void udp_server_task(void *arg)
{
    char rx_buffer[BUF_SIZE];
    struct sockaddr_in addr, source_addr;
    socklen_t socklen = sizeof(source_addr);

    while (1) {

        // 等待 WiFi 连接
        while (g_wifi_state != WIFI_CONNECTED) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
        if (sock < 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(UDP_PORT);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);

        bind(sock, (struct sockaddr *)&addr, sizeof(addr));

        ESP_LOGI("UDP", "UDP server started");

        while (g_wifi_state == WIFI_CONNECTED) {

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(sock, &readfds);

            struct timeval tv;
            tv.tv_sec = 2;   // 2秒超时
            tv.tv_usec = 0;

            int ret = select(sock + 1, &readfds, NULL, NULL, &tv);

            if (ret < 0) {
                ESP_LOGE("UDP", "select error");
                break;
            } 
            else if (ret == 0) {
                // 超时
                ESP_LOGD("UDP", "select timeout");
                continue;
            }

            if (FD_ISSET(sock, &readfds)) {
                int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0,
                                   (struct sockaddr *)&source_addr, &socklen);
                if (len < 0) {
                    ESP_LOGW("UDP", "recv error");
                    break;
                }

                rx_buffer[len] = 0;
                ESP_LOGI("UDP", "Data: %s", rx_buffer);
            }
        }

        close(sock);
        ESP_LOGW("UDP", "Socket closed, wait reconnect");
    }
}
/* ==================== 主函数 ==================== */
void app_main(void)
{
    /* 配置输入 GPIO */
    gpio_config_t input_cfg = {
        .pin_bit_mask = 1ULL << GPIO_INPUT,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,   // 默认拉低
        .intr_type = GPIO_INTR_POSEDGE,         // 上升沿中断
    };
    gpio_config(&input_cfg);
    pwm_hw_init();
    wifi_init_sta();

    /* 创建 LED 任务 */
    xTaskCreate(
        led_task,
        "led_task",
        2048,
        NULL,
        5,
        &led_task_handle
    );

    /* 创建周期性任务, 周期性执行中断触发函数  */
    xTaskCreate(
        PeriodTask_isr,
        "PeriodTask_isr",
        2048,
        NULL,
        6,
        NULL
    );

    /* 创建 WiFi 监控任务 */
    xTaskCreate(
        wifi_monitor_task, 
        "wifi_monitor", 
        4096, 
        NULL, 
        7, 
        NULL);

    /* 创建 UDP 服务器任务 */
    xTaskCreate(
        udp_server_task, 
        "udp_server_task", 
        4096, 
        NULL, 
        6, 
        NULL);

    /* 安装 GPIO ISR 服务 */
    gpio_install_isr_service(0);

    /* 挂载 GPIO 中断 */
    gpio_isr_handler_add(GPIO_INPUT, gpio_isr_handler, NULL);

    ESP_LOGI(TAG, "System initialized. Waiting for GPIO interrupt...");
}
