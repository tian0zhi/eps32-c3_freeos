#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/ledc.h"

#define TAG "GPIO_NOTIFY_DEMO"

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

    /* 安装 GPIO ISR 服务 */
    gpio_install_isr_service(0);

    /* 挂载 GPIO 中断 */
    gpio_isr_handler_add(GPIO_INPUT, gpio_isr_handler, NULL);

    ESP_LOGI(TAG, "System initialized. Waiting for GPIO interrupt...");
}
