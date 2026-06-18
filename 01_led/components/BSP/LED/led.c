#include "led.h"

/**
 * @brief       初始化 LED 对应的 GPIO 引脚
 * @note        配置 LED_GPIO_PIN 为输入输出模式并使能上拉，
 *              配置完成后默认熄灭 LED
 * @param       无
 * @retval      无
 */
void led_init(void)
{
    /* GPIO 配置结构体，先清零，避免成员残留随机值 */
    gpio_config_t gpio_init_struct = {0};

    /* 关闭引脚的中断功能（LED 只做输出控制，不需要中断） */
    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;

    /* 设置为输入输出模式：既能输出电平驱动 LED，
       又能用 gpio_get_level() 读回当前电平（LED_TOGGLE 取反时需要） */
    gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT;

    /* 指定要配置的引脚：用位掩码表示，将第 LED_GPIO_PIN 位置 1 */
    gpio_init_struct.pin_bit_mask = 1ull << LED_GPIO_PIN;

    /* 关闭内部下拉电阻 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;

    /* 使能内部上拉电阻，保证引脚有确定的默认电平 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;

    /* 将以上配置写入 GPIO 硬件寄存器，使配置生效 */
    gpio_config(&gpio_init_struct);

    /* 初始化完成后点亮/熄灭 LED（具体亮灭取决于硬件接法与 LED 宏定义） */
    LED(1);
}