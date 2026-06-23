#ifndef __WDT_H__
#define __WDT_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

void wd_init(uint16_t arr, uint64_t tps);
void IRAM_ATTR wdt_isr_handler(void *arg);

void restart_timer(uint64_t timeout);
#endif