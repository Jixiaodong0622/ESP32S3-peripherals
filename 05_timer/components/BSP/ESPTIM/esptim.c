#include "esptim.h"
#include "led.h"

/**
 * @brief       初始化并启动 ESP 高精度定时器（周期性触发）
 * @note        定时器到达设定周期后会自动调用 esptim_callback 回调函数，
 *              并按该周期循环触发，无需手动重载
 * @param       tps : 定时器触发周期，单位为微秒（us）
 * @retval      无（void）
 */
void esptim_int_init(uint64_t tps)
{
    esp_timer_handle_t esp_tim_handle; /* 定时器句柄，用于标识/操作该定时器 */

    /* 定时器创建参数：指定回调函数及传入回调的参数 */
    esp_timer_create_args_t tim_periodic_arg = {
        .arg = NULL,                  /* 传递给回调函数的参数，此处不使用，置为 NULL */
        .callback = &esptim_callback, /* 定时器到期时执行的回调函数 */
    };

    esp_timer_create(&tim_periodic_arg, &esp_tim_handle); /* 根据参数创建定时器，得到句柄 */
    esp_timer_start_periodic(esp_tim_handle, tps);        /* 以 tps 微秒为周期，启动周期性定时器 */
}

/**
 * @brief       ESP 定时器周期性触发的回调函数
 * @note        每个定时周期到达时由系统自动调用，本例中用于翻转 LED 状态
 * @param       arg : 创建定时器时传入的参数（对应 tim_periodic_arg.arg），此处未使用
 * @retval      无（void）
 */
void esptim_callback(void *arg)
{
    LED_TOGGLE(); /* 翻转 LED 电平，实现定时闪烁效果 */
}