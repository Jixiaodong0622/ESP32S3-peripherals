#ifndef __PWM_H__
#define __PWM_H__

#include "driver/ledc.h"

#define LEDC_PWM_TIMER LEDC_TIMER_1         /* 使用定时器1 */
#define LEDC_PWM_CH0_GPIO GPIO_NUM_1        /* LED 控制器对应的 GPIO（需与 LED 实际连接的引脚一致） */
#define LEDC_PWM_CH0_CHANNEL LEDC_CHANNEL_1 /* LED 控制器通道号 */

void pwm_init(uint8_t resolution, uint16_t freq);
void pwm_set_duty(uint16_t duty);
#endif
