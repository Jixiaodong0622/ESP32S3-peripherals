# 实验六　通用定时器实验(GPTimer)

本实验使用 ESP32-S3 的**通用定时器**(General Purpose Timer,GPTimer)产生周期性的**报警(alarm)中断**,在中断回调里把当前计数值通过 FreeRTOS 队列发送给主任务并打印。它对应正点原子《DNESP32S3 使用指南-IDF 版》第十五章 GPTIMER 实验。

- 实验平台:正点原子 ESP32-S3 开发板
- LED 连接引脚:`GPIO1`(见 `components/BSP/LED/led.h`)
- 本实验参数(见 `main/main.c`):初始计数值 `counts = 100`,分辨率 `resolution = 1000000`(1MHz,1 个 tick = 1µs),报警阈值 `alarm_count = 1000000`,即**约每 1 秒报警一次**

> 与上一章 ESPTIMER(高分辨率定时器,基于系统定时器)不同:GPTimer 是 ESP32-S3 **定时器组(Timer Group)** 中的硬件外设,可独立设置时钟源、预分频、计数方向、报警值与自动重载,更接近传统 MCU 里的"通用定时器"。

---

## 一、通用定时器简介

### 1. 基本参数

ESP32-S3 内置 **2 个通用定时器组**,每组含 **2 个通用定时器**(Timer0、Timer1)。通过指定定时器号与通道号即可选定要用的定时器。每个定时器都:

- 支持独立编程;
- 具备微秒级精确定时中断能力;
- 可配置**时钟源、预分频系数、计数方向、自动重载值、报警值、中断使能**等参数。

### 2. 通用定时器架构

一个通用定时器的信号链路如下:

```
 时钟源选择          16 位预分频器         54 位时基计数器          比较器
┌──────────┐      ┌────────────┐       ┌──────────────┐      ┌──────────┐
│ APB_CLK  │      │  ÷ DIVIDER │       │  递增/递减计数 │      │ 当前值 == │
│   或     │ ───► │ (2~65536)  │ ────► │  每个 TB_CLK   │ ───► │  报警值? │ ──► 报警中断
│ XTAL_CLK │      │            │ TB_CLK│  周期 ±1       │      │          │     (可自动重载)
└──────────┘      └────────────┘       └──────────────┘      └──────────┘
```

#### (1)时钟选择器

每个定时器可通过寄存器 `TIMG_TxCONFIG_REG` 的 **`TIMG_Tx_USE_XTAL`** 字段,选择:

- `APB_CLK`(APB 总线时钟),或
- `XTAL_CLK`(外部晶振时钟)

作为时钟源。**本实验代码选择的是 `GPTIMER_CLK_SRC_APB`(APB 时钟)。**

#### (2)16 位预分频器

时钟源先经 16 位预分频器分频,得到**时基计数器时钟 `TB_CLK`**。分频系数由 **`TIMG_Tx_DIVIDER`** 字段配置,可取 `2 ~ 65536`:

- 置 `0` → 分频系数变为 `65536`;
- 置 `1` → 实际分频系数为 `2`。

> ⚠️ 必须先关闭定时器(`TIMG_Tx_EN` 清零)才能修改预分频器,运行中修改结果不可预知。

#### (3)54 位时基计数器(Counter)

- 基于 `TB_CLK` 计数,是定时器的"核心计数寄存器";
- 计数方向由 **`TIMG_Tx_INCREASE`** 配置(递增/递减),且使能后仍可即时改变方向;
- 由 **`TIMG_Tx_EN`** 使能/暂停:使能时每个 `TB_CLK` 周期 ±1,关闭时暂停;
- **读取技巧(54 位 vs 32 位 CPU)**:CPU 是 32 位,无法一次读完 54 位。向 **`TIMG_TxUPDATE_REG`** 写入任意值,会把当前计数锁存到 **`TIMG_TxLO_REG`**(低 32 位)和 **`TIMG_TxHI_REG`**(高 22 位);当 `TIMG_TxUPDATE_REG` 被硬件清零表示锁存完成,即可从这两个寄存器安全读取。

#### (4)比较器(Comparator)/ 报警

- 当**计数器当前值 == 报警值**时触发**报警(alarm)**,报警产生中断,并可(选)自动重新加载计数值;
- 54 位报警值由 **`TIMG_TxALARMLO_REG`**(低 32 位)和 **`TIMG_TxALARMHI_REG`**(高 22 位)配置;
- 必须置位 **`TIMG_Tx_ALARM_EN`** 使能报警后,报警值才生效;
- 为防止"报警使能过晚"(使能时计数值已越过报警值),硬件会在越界后立即触发一次报警。

### 3. 常用寄存器一览

| 寄存器 / 字段 | 作用 |
| --- | --- |
| `TIMG_TxCONFIG_REG` | 定时器总配置寄存器 |
| `TIMG_Tx_USE_XTAL` | 时钟源选择(APB / XTAL) |
| `TIMG_Tx_DIVIDER` | 16 位预分频系数(2~65536) |
| `TIMG_Tx_EN` | 定时器使能 / 暂停 |
| `TIMG_Tx_INCREASE` | 计数方向(递增 / 递减) |
| `TIMG_TxUPDATE_REG` | 触发把 54 位计数值锁存到 LO/HI |
| `TIMG_TxLO_REG` / `TIMG_TxHI_REG` | 锁存的计数值(低 32 位 / 高 22 位) |
| `TIMG_TxALARMLO_REG` / `TIMG_TxALARMHI_REG` | 报警值(低 32 位 / 高 22 位) |
| `TIMG_Tx_ALARM_EN` | 报警功能使能 |

> 在 ESP-IDF 中,这些寄存器都由 `driver/gptimer.h` 提供的高层 API 封装,我们通过结构体配置即可,无需直接读写寄存器。

---

## 二、GPTimer API 介绍

使用前需包含头文件:

```c
#include "driver/gptimer.h"
```

### 1. `gptimer_new_timer()` —— 创建并配置定时器

```c
esp_err_t gptimer_new_timer(const gptimer_config_t *config, gptimer_handle_t *ret_timer);
```

| 形参 | 说明 |
| --- | --- |
| `config` | 指向定时器配置结构体的指针 |
| `ret_timer` | **输出参数**:创建成功后返回定时器句柄,后续操作都用它 |

**返回值**:`ESP_OK` 成功,其他失败。

`gptimer_config_t` 结构体成员:

| 成员 | 说明 | 本实验取值 |
| --- | --- | --- |
| `clk_src` | 时钟源:`GPTIMER_CLK_SRC_APB` / `GPTIMER_CLK_SRC_XTAL` / `GPTIMER_CLK_SRC_DEFAULT` | `GPTIMER_CLK_SRC_APB` |
| `direction` | 计数方向:`GPTIMER_COUNT_UP`(递增)/ `GPTIMER_COUNT_DOWN`(递减) | `GPTIMER_COUNT_UP` |
| `resolution_hz` | 计数器分辨率(Hz),**1 个 tick = 1 / resolution_hz 秒** | `1000000`(1MHz → 1 tick = 1µs) |
| `intr_priority` | 中断优先级,`0` 表示自动分配默认优先级 | 默认(0) |
| `flags.intr_shared` | 是否与其他定时器共享中断源 | 默认 |

> 结构体务必先清零(`{0}`),否则 `intr_priority`、`flags` 等未赋值字段是脏数据,可能导致创建失败。

### 2. `gptimer_set_raw_count()` / `gptimer_get_raw_count()` —— 设置 / 读取计数值

```c
esp_err_t gptimer_set_raw_count(gptimer_handle_t timer, unsigned long long value);
esp_err_t gptimer_get_raw_count(gptimer_handle_t timer, uint64_t *value);
```

| 形参 | 说明 |
| --- | --- |
| `timer` | 由 `gptimer_new_timer()` 创建的句柄 |
| `value` | 设置:要写入的计数值;读取:接收当前计数值的指针 |

本实验把初始计数值设为 `counts`(=100),再读回打印验证。

### 3. `gptimer_register_event_callbacks()` —— 注册事件回调

```c
esp_err_t gptimer_register_event_callbacks(gptimer_handle_t timer,
                                           const gptimer_event_callbacks_t *cbs,
                                           void *user_data);
```

| 形参 | 说明 |
| --- | --- |
| `timer` | 定时器句柄 |
| `cbs` | 要绑定的回调函数集合(`gptimer_event_callbacks_t`,本实验用其 `on_alarm` 成员注册报警回调) |
| `user_data` | 用户数据,会原样传给回调(本实验传入消息队列句柄 `queue`) |

**返回值**:`ESP_OK` 成功。

> 此函数只是"延迟安装"中断服务,并不立即使能。因此**必须在 `gptimer_enable()` 之前调用**,否则返回 `ESP_ERR_INVALID_STATE`。
> 回调运行在 ISR 上下文,内部只能调用带 `FromISR` 后缀的 FreeRTOS API,不能阻塞。

### 4. `gptimer_set_alarm_action()` —— 设置报警动作

```c
esp_err_t gptimer_set_alarm_action(gptimer_handle_t timer, const gptimer_alarm_config_t *config);
```

| 形参 | 说明 |
| --- | --- |
| `timer` | 定时器句柄 |
| `config` | 指向报警配置结构体的指针;**传 `NULL` 表示禁用报警** |

`gptimer_alarm_config_t` 结构体:

```c
typedef struct {
    uint64_t alarm_count;   /* 报警目标计数值:计到这个值就触发报警中断 */
    uint64_t reload_count;  /* 自动重载值:报警后重新装入计数器(仅 auto_reload_on_alarm=true 时生效) */
    struct {
        uint32_t auto_reload_on_alarm: 1; /* 是否在报警时由硬件自动重载 reload_count */
    } flags;
} gptimer_alarm_config_t;
```

> 注意:`auto_reload_on_alarm` 为 `true` 时,`alarm_count` 与 `reload_count` 不能相同(否则报警值=重载值没有意义)。

### 5. `gptimer_enable()` / `gptimer_start()` —— 使能与启动

```c
esp_err_t gptimer_enable(gptimer_handle_t timer);  /* init 状态 → enable 状态,并使能已注册的回调 */
esp_err_t gptimer_start(gptimer_handle_t timer);   /* 让内部计数器真正开始计数 */
```

> 两者职责不同:`gptimer_enable()` 只是把驱动切到"使能态"并激活回调;**还要再调用 `gptimer_start()` 计数器才会真正运行**。

---

## 三、驱动解析与调用流程

驱动位于 `components/BSP/GPTIM/`,由 `gptim.h`(声明)和 `gptim.c`(实现)组成。

### 1. 中断与任务的数据通道:消息队列

GPTimer 的回调运行在 ISR 中,不能在里面做耗时/阻塞操作。本实验用一个 **FreeRTOS 队列** 把"本次报警的计数值"从中断发到主任务,主任务再慢慢打印,这是中断与任务解耦的典型做法:

```c
typedef struct { uint64_t event_count; } gptimer_event_t; /* 队列里传递的事件数据 */
extern QueueHandle_t queue;                               /* 全局队列句柄 */
```

### 2. 初始化流程 `gptim_int_init(counts, resolution)`

```
gptim_int_init(100, 1000000)
  ├── 填写 gptimer_config_t(APB 时钟 / 递增 / 1MHz 分辨率)
  ├── gptimer_new_timer()              // 创建定时器,得到句柄 g_tim
  ├── xQueueCreate(10, ...)            // 创建长度 10 的事件队列
  ├── gptimer_set_raw_count(100)       // 设初始计数值
  ├── gptimer_get_raw_count()          // 读回验证并打印
  ├── gptimer_register_event_callbacks()// 注册 on_alarm 回调,user_data=queue
  ├── gptimer_enable()                 // 使能(必须在注册回调之后)
  ├── gptimer_set_alarm_action()       // alarm_count=1000000 → 约 1s 报警
  └── gptimer_start()                  // 启动计数
```

### 3. 报警回调 `gptimer_callback()`

回调每次报警时被自动调用,它:

1. 从 `edata->count_value` 取出当前计数值,封装成 `gptimer_event_t`;
2. 用 `xQueueSendFromISR()` 发送到队列(ISR 专用 API);
3. **重新设置下一次报警值** = `edata->alarm_value + 1000000`,从而实现"每隔约 1 秒周期性报警";
4. 返回 `high_task_awoken`,告知是否需要在 ISR 退出时做任务切换。

> 本实验通过"回调里手动重设下一次 alarm_count"实现周期触发;另一种等效做法是设置 `flags.auto_reload_on_alarm = true` + `reload_count`,让硬件自动重载,无需在回调里重设。

### 4. 主任务消费事件(`main.c`)

`app_main` 初始化后进入 `while(1)`,用 `xQueueReceive(queue, &event, 2000)` 阻塞等待事件:收到则打印计数值,超时(2000 个节拍)则打印"错过一次计数事件"。

### 5. `CMakeLists.txt` 依赖

BSP 组件需在 `REQUIRES` 中加入 `driver`(GPTimer 接口在 `driver/gptimer.h`)和 `esp_timer`:

```cmake
set(requires
    esp_timer
    driver)
```

---

## 四、硬件设计

本实验使用的通用定时器是 **ESP32-S3 的片上资源**,不占用外部引脚,**没有对应连接原理图**。LED 接 `GPIO1`(接法与点亮逻辑见 LED 实验)。

实验现象:程序运行后配置通用定时器并周期性触发报警,串口在每个周期打印输出定时器的报警事件及计数值。

---

## 五、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口:初始化 LED、定时器,循环从队列接收并打印计数事件 |
| `components/BSP/GPTIM/gptim.h` | 事件结构体、队列句柄与函数声明 |
| `components/BSP/GPTIM/gptim.c` | `gptim_int_init()` 配置/启动定时器,`gptimer_callback()` 报警回调 |
| `components/BSP/LED/led.h` / `led.c` | LED 引脚定义、操作宏与初始化 |
| `components/BSP/CMakeLists.txt` | BSP 构建配置,`REQUIRES` 含 `driver`、`esp_timer` |
