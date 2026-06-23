/**
 ******************************************************************************
 * @file        main.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2025-01-01
 * @brief       基础例程
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ******************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 ******************************************************************************
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_psram.h"
#include "esp_flash.h"
#include "esp_log.h"
#include <stdio.h>

#include "led.h"
#include "gptim.h"
const char *TAG = "main";

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret; /* 用于接收各 API 的返回值（错误码） */

    uint8_t record;              /* 标志位：记录本轮循环是否成功收到定时器事件 */
    gptimer_event_t g_tim_event; /* 从队列接收的事件数据（含定时器计数值） */

    ret = nvs_flash_init(); /* 初始化 NVS（非易失性存储），WiFi/蓝牙等组件依赖它 */
    /* 若 NVS 没有空闲页或版本不匹配，则擦除后重新初始化 */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase()); /* 擦除 NVS 分区 */
        ESP_ERROR_CHECK(nvs_flash_init());  /* 再次初始化 */
    }

    led_init();                   /* 初始化 LED（GPIO） */
    gptim_int_init(100, 1000000); /* 初始化通用定时器：初值 100，分辨率 1MHz（1 tick=1µs），约 1 秒报警一次 */

    while (1)
    {
        record = 1; /* 每轮循环先复位标志：默认认为本轮“未收到事件” */
        /* 阻塞等待定时器中断回调发来的事件，最多等待 2000 个 tick（FreeRTOS 节拍）。
           收到返回 pdTRUE，超时返回 pdFALSE */
        if (xQueueReceive(queue, &g_tim_event, 2000))
        {
            /* 成功收到一次报警事件，打印本次的定时器计数值 */
            ESP_LOGI("GPTIMER_ALARM", "定时器报警, 计数值： %llu", g_tim_event.event_count); /* 打印通用定时器发生一次计数事件后获取到的值 */
            record--;                                                                        /* 标志清零，表示本轮成功收到事件 */
        }
        else
        {
            /* 超时仍未收到事件，说明错过了一次计数事件，打印警告 */
            ESP_LOGW("GPTIMER_ALARM", "错过一次计数事件");
        }
    }
    vQueueDelete(queue); /* 删除队列释放资源（while(1) 不退出，此行实际不会执行到） */
}
