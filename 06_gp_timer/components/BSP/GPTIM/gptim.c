#include "gptim.h"
QueueHandle_t queue;
/**
 * @brief       初始化通用定时器（GPTimer）并启动周期性报警中断
 * @note        创建定时器 -> 创建事件队列 -> 设置/读取计数值 -> 注册中断回调
 *              -> 配置报警动作 -> 使能并启动定时器。
 *              定时器每达到 alarm_count 个计数（tick）就触发一次报警中断，
 *              中断里调用 gptimer_callback() 回调函数。
 * @param       counts:      定时器初始计数值（启动时写入的起始 tick 值）
 * @param       resolution:  定时器分辨率（单位 Hz），即每秒计数多少次。
 *                           例如 1000000 表示 1MHz，此时 1 个 tick = 1 微秒
 * @retval      无
 */
void gptim_int_init(uint16_t counts, uint32_t resolution)
{
    /* 配置通用定时器 */
    gptimer_config_t g_tim_handle = {0}; /* 定时器配置结构体，必须清零：避免 intr_priority/flags 等未赋值字段为脏数据导致创建失败 */
    gptimer_handle_t g_tim = NULL;       /* 定时器句柄，gptimer_new_timer 创建成功后指向定时器实例 */
    ESP_LOGI("GPTIMER_ALARM", "配置通用定时器");
    g_tim_handle.clk_src = GPTIMER_CLK_SRC_APB; /* 时钟源选择 APB 总线时钟 */
    g_tim_handle.direction = GPTIMER_COUNT_UP;  /* 计数方向：向上计数（从初值递增） */
    g_tim_handle.resolution_hz = resolution;    /* 分辨率：每秒计数次数，决定 1 个 tick 的时间长度 */
    /* 按配置创建定时器实例，返回句柄到 g_tim；检查返回值，失败则记录并退出 */
    ESP_ERROR_CHECK(gptimer_new_timer(&g_tim_handle, &g_tim));

    /* 创建一个长度为 10 的事件队列，每个元素是一个 gptimer_event_t，
       用于中断回调把计数值传递给主任务（中断与任务之间的数据通道） */
    queue = xQueueCreate(10, sizeof(gptimer_event_t));
    if (!queue)
    {
        ESP_LOGE("GPTIMER_ALARM", "创建队列失败"); /* 队列创建失败（内存不足等），直接返回 */
        return;
    }

    /* 设置和获取计数值 */
    ESP_LOGI("GPTIMER_ALARM", "设置计数值");
    gptimer_set_raw_count(g_tim, counts); /* 把定时器当前计数值（原始寄存器值）设为 counts */
    ESP_LOGI("GPTIMER_ALARM", "获取计数值");
    uint64_t count;
    gptimer_get_raw_count(g_tim, &count); /* 读回当前计数值，存入全局变量 count，用于验证 */
    ESP_LOGI("GPTIMER_ALARM", "定时器计数值： %llu", count);

    /* 注册用户回调函数 */
    gptimer_event_callbacks_t g_tim_callbacks = {0}; /* 回调函数集合结构体，清零未用字段 */
    g_tim_callbacks.on_alarm = gptimer_callback;     /* 指定“报警事件”发生时调用的回调函数 */
    /* 注册回调，并把 queue 作为 user_data 传入，回调里可通过 user_data 拿到队列句柄 */
    gptimer_register_event_callbacks(g_tim, &g_tim_callbacks, queue);

    /* 设置报警动作 */
    gptimer_alarm_config_t alarm_config = {0}; /* 报警配置结构体，清零 reload_count/flags 等未用字段 */
    alarm_config.alarm_count = 1000000;        /* 报警阈值：计数到 1000000 个 tick 触发一次中断 */
    ESP_LOGI("GPTIMER_ALARM", "使能通用定时器");
    gptimer_enable(g_tim);                          /* 使能定时器（从 init 状态进入 enable 状态） */
    gptimer_set_alarm_action(g_tim, &alarm_config); /* 应用报警配置，启用报警事件 */
    gptimer_start(g_tim);                           /* 启动定时器，开始计数 */
}

/**
 * @brief       定时器报警中断回调函数（在 ISR 中断上下文中执行）
 * @note        IRAM_ATTR 把函数放到内部 RAM，保证 Flash 操作时中断仍可执行、响应更快。
 *              每次定时器计数到达 alarm_count 时被自动调用：
 *              取出本次计数值发送到队列，并重新设置下一次报警阈值，实现周期性中断。
 * @param       timer:      触发本次中断的定时器句柄
 * @param       edata:      报警事件数据（含本次报警时的计数值 count_value、报警阈值 alarm_value 等）
 * @param       user_data:  注册回调时传入的参数，这里是队列句柄 queue
 * @retval      true:  需要在 ISR 退出时进行任务切换（有更高优先级任务被唤醒）
 *              false: 不需要任务切换
 */
bool IRAM_ATTR gptimer_callback(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE; /* 标志位：是否有更高优先级任务因发送队列而被唤醒 */

    queue = (QueueHandle_t)user_data; /* 把 user_data 还原为队列句柄 */
    gptimer_event_t ele = {           /* 构造要发送的事件，记录本次报警时的计数值 */
                           .event_count = edata->count_value};

    /* 可选：通过操作系统队列将事件数据从中断发送到其他任务（ISR 专用 API） */
    xQueueSendFromISR(queue, &ele, &high_task_awoken);

    /* 重新配置下一次报警阈值 = 本次报警值 + 1000000，使中断周期性触发 */
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = edata->alarm_value + 1000000};
    gptimer_set_alarm_action(timer, &alarm_config); /* BUG: 此处应为 (timer, &alarm_config)，详见下方说明 */
    /* 返回是否需要在 ISR 结束时让步（进行上下文切换） */
    return high_task_awoken == pdTRUE;
}