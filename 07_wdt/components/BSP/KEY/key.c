#include "key.h"

/**
 * @brief       初始化 KEY 对应的 GPIO 引脚
 * @note        配置 BOOT_GPIO_PIN 为输入模式并使能上拉，
 * @param       无
 * @retval      无
 */
void key_init(void)
{
    gpio_config_t gpio_init_struct = {0};                  /* 定义并清零 GPIO 配置结构体 */
    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;        /* 禁用中断（采用轮询扫描方式读取按键） */
    gpio_init_struct.mode = GPIO_MODE_INPUT;               /* 设置为输入模式，用于读取按键电平 */
    gpio_init_struct.pin_bit_mask = 1ull << BOOT_GPIO_PIN; /* 用位掩码指定要配置的引脚（第 BOOT_GPIO_PIN 位置 1） */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE; /* 禁用内部下拉电阻 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;      /* 使能内部上拉电阻：按键未按下时为高电平，按下接地为低电平 */
    gpio_config(&gpio_init_struct);                        /* 应用上述配置，完成 GPIO 初始化 */
}

/**
 * @brief           按键扫描函数
 * @param           mode:0 / 1, 具体含义如下:
 *                  0, 不支持连续按(当按键按下不放时, 只有第一次调用会返回键值,
 *                  必须松开以后, 再次按下才会返回其他键值)
 *                  1, 支持连续按(当按键按下不放时, 每次调用该函数都会返回键值)
 * @retval          键值, 定义如下:
 *                  BOOT_PRES, 1, BOOT 按下
 */
uint8_t key_scan(uint8_t mode)
{
    uint8_t keyval = 0;          /* 键值，默认 0 表示无按键按下 */
    static uint8_t key_boot = 1; /* 按键松开标志位（static 使其在多次调用间保持值）：1=已松开可触发，0=已按下未松开 */

    if (mode) /* mode=1：连续按模式，每次进入都强制复位标志，使按住不放也能持续返回键值 */
    {
        key_boot = 1;
    }

    if (key_boot && (BOOT == 0)) /* 上一次为松开状态，且当前检测到低电平，说明按键被按下 */
    {                            /* 此时 key_boot=1 是“只触发一次”的关键：按住期间下面会清零它 */
        vTaskDelay(10);          /* 延时 10ms 软件消抖，避开按键机械抖动 */
        key_boot = 0;            /* 清零标志，标记为“已按下”，防止重复触发 */

        if (BOOT == 0) /* 消抖后再次确认仍为低电平，确属有效按下而非抖动 */
        {
            keyval = BOOT_PRES; /* 记录键值：BOOT 键按下 */
        }
    }
    else if (BOOT == 1) /* 当前为高电平，说明按键已松开 */
    {
        key_boot = 1; /* 复位标志，允许下一次按下重新触发 */
    }

    return keyval; /* 返回键值：BOOT_PRES 表示按下，0 表示无按键 */
}