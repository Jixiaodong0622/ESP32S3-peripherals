#ifndef __ESPTIM_H__
#define __ESPTIM_H__

#include "esp_timer.h"

void esptim_int_init(uint64_t tps); /* 初始化初始化高分辨率定时器 */
void esptim_callback(void *arg);    /* 定时器回调函数 */

#endif
