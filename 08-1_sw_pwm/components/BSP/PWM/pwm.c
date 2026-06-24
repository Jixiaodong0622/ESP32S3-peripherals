#include "pwm.h"

/**
 * @brief  PWM 初始化（基于 LEDC 外设）
 * @note   先配置一个 LEDC 定时器，再将通道绑定到该定时器并映射到 GPIO 输出 PWM。
 *         定时器与通道的 speed_mode 必须保持一致。
 * @param  resolution  占空比分辨率（位数，ledc_timer_bit_t），占空比取值范围 0 ~ (2^resolution - 1)
 * @param  freq        PWM 输出频率，单位 Hz
 * @retval 无
 */
void pwm_init(uint8_t resolution, uint16_t freq)
{
    /* 1.配置 LEDC 定时器 */
    ledc_timer_config_t ledc_timer = {0};
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;          /* 时钟源自动选择 */
    ledc_timer.duty_resolution = resolution;     /* 占空比分辨率（位数） */
    ledc_timer.freq_hz = freq;                   /* PWM 频率，单位 Hz */
    ledc_timer.speed_mode = LEDC_LOW_SPEED_MODE; /* 速度模式：低速模式 */
    ledc_timer.timer_num = LEDC_PWM_TIMER;       /* 使用的定时器编号 */
    ledc_timer_config(&ledc_timer);              /* 应用定时器配置 */

    /* 2.配置通道 */
    ledc_channel_config_t ledc_channel = {0};
    ledc_channel.channel = LEDC_PWM_CH0_CHANNEL;   /* 通道编号 */
    ledc_channel.duty = 0;                         /* 初始占空比为 0 */
    ledc_channel.intr_type = LEDC_INTR_DISABLE;    /* 关闭通道中断 */
    ledc_channel.gpio_num = LEDC_PWM_CH0_GPIO;     /* PWM 输出的 GPIO 引脚 */
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE; /* 速度模式：低速模式（需与定时器一致） */
    ledc_channel.timer_sel = LEDC_PWM_TIMER;       /* 绑定到上面配置的定时器 */
    ledc_channel_config(&ledc_channel);            /* 应用通道配置 */
}

/**
 * @brief  设置 PWM 占空比
 * @note   需调用 ledc_update_duty 后新占空比才会真正输出生效。
 * @param  duty  新的占空比值，取值范围 0 ~ (2^resolution - 1)，resolution 为 pwm_init 中设置的分辨率
 * @retval 无
 */
void pwm_set_duty(uint16_t duty)
{
    /* 1.设置新的占空比 */
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_PWM_CH0_CHANNEL, duty);
    /* 2. 新的占空比生效 */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_PWM_CH0_CHANNEL);
}