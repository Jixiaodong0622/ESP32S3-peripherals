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
#include "uart.h"
const char *TAG = "main";

/**
 * @brief       程序入口
 * @param       无
 * @retval      无
 */
void app_main(void)
{
    esp_err_t ret;                              /* 用于接收函数返回的错误码 */
    uint8_t len = 0;                            /* 串口接收缓冲区中当前的数据长度 */
    uint16_t times = 0;                         /* 空闲计数器, 用于定时打印提示信息和翻转 LED */
    unsigned char data[RX_BUF_SIZE] = {0};      /* 串口接收数据缓存数组 */

    ret = nvs_flash_init();                     /* 初始化 NVS (非易失性存储) */
    /* 若 NVS 没有空闲页或检测到新版本, 先擦除再重新初始化 */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());     /* 擦除 NVS 分区 */
        ESP_ERROR_CHECK(nvs_flash_init());      /* 重新初始化 NVS */
    }

    led_init();                                 /* 初始化 LED */
    usart_init(115200);                         /* 初始化串口, 波特率 115200 */

    while (1)
    {
        /* 获取串口接收缓冲区中已缓存的数据长度, 存入 len */
        uart_get_buffered_data_len(USART_UX, (size_t *)&len);
        if (len > 0)                            /* 收到了数据 */
        {
            memset(data, 0, RX_BUF_SIZE);       /* 清空接收缓存数组 */
            printf("\n 您发送的消息为:\n");
            /* 从串口读取 len 个字节到 data, 最长等待 100 个 tick */
            uart_read_bytes(USART_UX, data, len, 100);
            /* 将收到的数据原样发回 (回显) */
            uart_write_bytes(USART_UX, (const void *)data, strlen((const void *)data));
        }
        else                                    /* 没有收到数据 */
        {
            times++;                            /* 空闲计数自增 */
            if (times % 5000 == 0)              /* 每空闲 5000 次, 打印一次实验标题 */
            {
                printf("\n 正点原子 ATK-DNESP32-S3 开发板 串口实验\n");
                printf("正点原子@ALIENTEK\n\n\n");
            }
            if (times % 200 == 0)               /* 每空闲 200 次, 提示用户输入 */
            {
                printf("请输入数据，以回车键结束\n");
            }
            if (times % 30 == 0)                /* 每空闲 30 次, 翻转一次 LED (指示程序运行) */
            {
                LED_TOGGLE();
            }
            vTaskDelay(10);                     /* 延时 10 个 tick, 释放 CPU 给其他任务 */
        }
    }
}
