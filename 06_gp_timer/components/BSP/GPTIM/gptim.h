#ifndef __GPTIM__
#define __GPTIM__

#include "driver/gptimer.h"
#include "freeRTOS/FreeRTOS.h"
#include "freeRTOS/task.h"
#include "esp_log.h"

#include "freertos/queue.h"

typedef struct
{
    uint64_t event_count;
} gptimer_event_t;

extern QueueHandle_t queue;

void gptim_int_init(uint16_t counts, uint32_t resolution); /* 初始化通用定时器（resolution 用 uint32_t，避免 1000000 等大值被截断） */

bool IRAM_ATTR gptimer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data); /* 定时器回调函数 */

#endif
