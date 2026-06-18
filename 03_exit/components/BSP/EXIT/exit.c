#include "exit.h"
#include "led.h"

/**
 * @brief       GPIO 中断服务函数（ISR 回调）
 * @note        使用 IRAM_ATTR 修饰，将函数放入 IRAM 中运行，
 *              避免 Flash 操作（如擦写）期间中断无法响应，提高实时性
 * @param       arg : 注册中断时传入的参数，此处为触发中断的 GPIO 引脚号
 * @retval      无
 */
static void IRAM_ATTR exit_gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;      /* 将传入参数还原为 GPIO 引脚号 */

    if (gpio_num == BOOT_INT_GPIO_PIN)      /* 判断是否为 BOOT 按键对应的引脚触发 */
    {
        LED_TOGGLE();                       /* 翻转 LED 状态 */
    }
}

/**
 * @brief       外部中断初始化程序
 * @note        配置 BOOT 按键所在 GPIO 为下降沿触发中断，并注册中断服务函数
 * @param       无
 * @retval      无
 */
void exit_init(void)
{
    gpio_config_t gpio_init_struct = {0};                       /* GPIO 配置结构体，初始化清零 */

    gpio_init_struct.intr_type = GPIO_INTR_NEGEDGE;             /* 中断触发方式：下降沿触发 */
    gpio_init_struct.mode = GPIO_MODE_INPUT;                    /* GPIO 模式：输入 */
    gpio_init_struct.pin_bit_mask = 1ull << BOOT_INT_GPIO_PIN;  /* 设置要配置的引脚位掩码 */
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;      /* 禁用下拉电阻 */
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;           /* 使能上拉电阻，默认高电平 */

    gpio_config(&gpio_init_struct);                            /* 应用上述配置 */

    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);             /* 安装 GPIO 中断服务（边沿触发） */

    /* 为指定引脚注册中断服务函数，并把引脚号作为参数传入 */
    gpio_isr_handler_add(BOOT_INT_GPIO_PIN, exit_gpio_isr_handler, (void *)BOOT_INT_GPIO_PIN);

    gpio_intr_enable(BOOT_INT_GPIO_PIN);                       /* 使能该引脚的中断 */
}