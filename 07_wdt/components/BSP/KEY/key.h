#ifndef __KEY_H__
#define __KEY_H__
#include "driver/gpio.h"
#include "freeRTOS/FreeRTOS.h"
#include "freeRTOS/task.h"
/* 引脚定义 */
#define BOOT_GPIO_PIN GPIO_NUM_0 /* BOOT 连接的 GPIO 端口 */

/*IO 操作*/
#define BOOT gpio_get_level(BOOT_GPIO_PIN)

/* 按键按下定义 */
#define BOOT_PRES 1 /* BOOT 按键按下 */

/* BOOT 函数声明 */
void key_init(void);
uint8_t key_scan(uint8_t mode);

#endif
