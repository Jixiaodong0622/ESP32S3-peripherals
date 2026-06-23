#include "wdt.h"

/* 定时器句柄：保存所创建的 esp_timer 实例，喂狗(restart_timer)时需要用到 */
esp_timer_handle_t esp_tim_handle;

/**
 * @brief       初始化“模拟看门狗”（用 esp_timer 周期定时器模拟）
 * @note        创建一个周期定时器，每隔 tps 微秒触发一次回调 wdt_isr_handler，
 *              回调内调用 esp_restart() 复位芯片。若在一个周期内没有喂狗
 *              (调用 restart_timer 重新计时)，时间一到芯片就会复位，
 *              以此模拟看门狗“不喂狗就复位”的行为。
 * @param       arr:   预留参数（本实现未使用，仅为与硬件看门狗接口形式保持一致）
 * @param       tps:   定时周期，单位微秒(us)，即看门狗的超时时间
 * @retval      无
 */
void wd_init(uint16_t arr, uint64_t tps)
{
    /* 定时器创建参数 */
    esp_timer_create_args_t tim_periodic_arg = {
        .arg = NULL,                  /* 传递给回调的参数，这里不需要 */
        .callback = &wdt_isr_handler, /* 超时回调函数：到点执行芯片复位 */
    };

    esp_timer_create(&tim_periodic_arg, &esp_tim_handle); /* 创建定时器实例 */
    esp_timer_start_periodic(esp_tim_handle, tps);        /* 以 tps(us) 为周期启动 */
}

/**
 * @brief       看门狗超时回调（模拟看门狗复位）
 * @note        IRAM_ATTR 将该函数放入 IRAM，保证执行时不受 Flash 访问影响。
 *              定时器超时(未及时喂狗)时被调用，直接复位整个芯片。
 * @param       arg:   创建定时器时传入的参数（此处未使用）
 * @retval      无
 */
void IRAM_ATTR wdt_isr_handler(void *arg)
{
    esp_restart(); /* 软件复位芯片，相当于看门狗触发复位 */
}

/**
 * @brief       喂狗：重新启动定时器计时
 * @note        重新计时即“喂狗”，只要在超时前调用，芯片就不会复位。
 * @param       timeout:   重新计时的周期，单位微秒(us)
 * @retval      无
 */
void restart_timer(uint64_t timeout)
{
    esp_timer_restart(esp_tim_handle, timeout); /* 重置定时器，重新开始倒计时 */
}