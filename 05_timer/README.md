# 实验五　高分辨率定时器实验(ESP Timer)

本实验通过 ESP32-S3 的**高分辨率定时器**(esp_timer)产生周期性中断,在定时器回调函数中翻转 LED,实现精确的定时闪烁。它对应正点原子《DNESP32S3 使用指南-IDF 版》第十四章 ESPTIMER 实验。

- 实验平台:正点原子 ESP32-S3 开发板
- LED 连接引脚:`GPIO1`(见 `components/BSP/LED/led.h` 中的 `LED_GPIO_PIN`)
- 定时周期:本实验取 `2000000us`(2 秒,见 `main/main.c`),即 LED 每 2 秒翻转一次

---

## 一、定时器简介

### 1. 定时器能做什么

定时器是单片机内部集成的功能,其计时依赖**计数器**:每经历一个时钟周期,计数器递增一次;当计数值达到设定值后触发**中断**,在中断(回调)函数里就能执行我们想要的逻辑。常见用途:

1. **执行定时任务**:周期性地、精准地执行某段代码(如每 500ms 采集一次数据);
2. **时间测量**:测量一段代码的执行时间或两个事件之间的间隔;
3. **精确延时**:提供微秒级精度的延时;
4. **PWM 信号生成**:借助精确计时输出 PWM,用于电机调速、LED 调光等;
5. **事件触发与监控**:周期性触发事件、实现看门狗等。

### 2. 硬件定时器 vs 软件定时器

| 类型 | 实现方式 | 优点 | 缺点 |
| --- | --- | --- | --- |
| **硬件定时器** | 依托芯片内置的计时/计数器电路 | 精度高、可靠,独立于 CPU 负载和操作系统调度,即使 CPU 繁忙也能准时触发 | 数量受硬件资源限制 |
| **软件定时器** | 由操作系统/软件库模拟 | 数量灵活,可创建大量定时器 | 精度受系统负载和任务调度影响 |

本实验使用的 **ESP 高分辨率定时器(esp_timer)** 基于 ESP32-S3 片上的**系统定时器(System Timer)** 硬件实现,因此能提供微秒级的可靠定时。

### 3. ESP32-S3 系统定时器结构

ESP32-S3 内置一组 **52 位系统定时器**,它由两类核心部件组成:

```
        XTAL_CLK (40MHz)
              │  分频
              ▼
        CNT_CLK (平均 16MHz)
   ┌──────────┴──────────┐
   │                     │
 ┌─▼──────┐         ┌────▼───┐
 │ 计数器  │  比较   │ 比较器  │ ──► 报警(中断)
 │ UNIT0  │ ──────► │ COMP0  │
 │ UNIT1  │         │ COMP1  │
 └────────┘         │ COMP2  │
   (2 个)            └────────┘
                      (3 个)
```

#### (1)计数器(Counter)

- 系统定时器内置 **2 个 52 位计数器**:`UNIT0`、`UNIT1`;
- 时钟源为 `XTAL_CLK`(外部晶振,40MHz),经分频后得到实际计数时钟 `CNT_CLK`;
- 分频规律:一个计数周期用 `fXTAL_CLK/3`、下一个用 `fXTAL_CLK/2`,平均频率为 `fXTAL_CLK/2.5 = 16MHz`;
- 因此 **每 16 个 CNT_CLK 周期计数值递增 1µs**(每个 CNT_CLK 周期约 1/16µs)。这正是"微秒级"分辨率的由来。

#### (2)比较器(Comparator)

- 系统定时器内置 **3 个 52 位比较器**:`COMP0`、`COMP1`、`COMP2`;
- 比较器同样以 `XTAL_CLK`(40MHz)为时钟源;
- 作用:**实时监控计数器的当前计数值是否达到"报警值(alarm)"**。一个计数器 + 一个比较器配合工作——当计数值达到比较器设定的报警值时,比较器生成**报警中断**,从而触发我们的回调函数。

> 简单理解:**计数器负责"一直数",比较器负责"盯着数到没到点,到点就报警"**。我们用 API 设定的"定时周期",本质上就是设定比较器的报警值。esp_timer 在底层自动管理这些硬件计数器/比较器,我们无需直接操作寄存器。

---

## 二、esp_timer API 介绍

ESP-IDF 的高分辨率定时器接口位于头文件 `esp_timer.h`,使用前需包含:

```c
#include "esp_timer.h"
```

本实验用到下面两个核心函数。

### 1. `esp_timer_create()` —— 创建一个定时器实例

```c
esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args,
                           esp_timer_handle_t *out_handle);
```

| 形参 | 说明 |
| --- | --- |
| `create_args` | 指向**定时器创建参数结构体**的指针,描述回调函数、参数等(见下表) |
| `out_handle` | **输出参数**:创建成功后,定时器句柄写入此处,后续启动/停止/删除都用它 |

**返回值**:`ESP_OK` 表示创建成功,其他值表示失败。

其中 `esp_timer_create_args_t` 结构体的成员如下:

| 成员 | 类型 | 说明 | 本实验取值 |
| --- | --- | --- | --- |
| `callback` | `esp_timer_cb_t` | 定时周期到达时调用的**回调函数**(需自行编写) | `&esptim_callback` |
| `arg` | `void *` | 传递给回调函数的参数(回调里通过 `arg` 拿到) | `NULL`(不带参数) |
| `dispatch_method` | 枚举 | 从 **task** 还是 **ISR(中断)** 上下文调用回调 | 默认(task) |
| `name` | `const char *` | 定时器名称,用于 esp_timer 调试/转储功能 | 默认 |
| `skip_unhandled_events` | `bool` | 是否跳过周期定时器中未及时处理的事件 | 默认 |

> 本实验只显式设置了 `callback` 和 `arg` 两个成员,其余成员保持默认(0/NULL)即可。

### 2. `esp_timer_start_periodic()` —— 启动周期性定时器

```c
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
```

| 形参 | 说明 |
| --- | --- |
| `timer` | 由 `esp_timer_create()` 创建并返回的定时器句柄 |
| `period_us` | **定时周期,单位为微秒(µs)** |

**返回值**:`ESP_OK` 表示启动成功,其他值表示失败。

启动后,定时器每经过 `period_us` 微秒就**自动**触发一次回调,**无需手动重载**,直到调用 `esp_timer_stop()`。

> **单位换算要点**:`period_us` 以**微秒**为单位。本实验 `main.c` 传入 `2000000`,即 `2000000µs = 2s`,所以 LED 每 2 秒翻转一次;若想 1 秒翻转一次,应传 `1000000`。

### 3. 常用扩展 API

| 函数 | 作用 |
| --- | --- |
| `esp_timer_start_once(timer, timeout_us)` | 启动**单次**定时器,`timeout_us` 微秒后只触发一次 |
| `esp_timer_stop(timer)` | 停止正在运行的定时器 |
| `esp_timer_delete(timer)` | 删除定时器,释放资源(需先 stop) |
| `esp_timer_get_time()` | 返回系统启动至今的微秒数(`int64_t`),常用于时间测量 |

---

## 三、驱动解析与调用流程

本实验的定时器驱动位于 `components/BSP/ESPTIM/`,由 `esptim.h`(声明)和 `esptim.c`(实现)两个文件组成。

### 1. `esptim.h` —— 函数声明

```c
void esptim_int_init(uint64_t tps); /* 初始化高分辨率定时器 */
void esptim_callback(void *arg);    /* 定时器回调函数 */
```

### 2. `esptim.c` —— 初始化与回调

```c
void esptim_int_init(uint64_t tps)
{
    esp_timer_handle_t esp_tim_handle;          /* 定时器句柄 */

    esp_timer_create_args_t tim_periodic_arg = {
        .arg = NULL,                            /* 不向回调传参 */
        .callback = &esptim_callback,           /* 周期到达时调用的回调 */
    };

    esp_timer_create(&tim_periodic_arg, &esp_tim_handle); /* ① 创建定时器,拿到句柄 */
    esp_timer_start_periodic(esp_tim_handle, tps);        /* ② 以 tps 微秒为周期启动 */
}

void esptim_callback(void *arg)
{
    LED_TOGGLE();                               /* 每次报警触发都翻转一次 LED */
}
```

**理解要点**:`esp_timer_create_args_t` 通过 `callback` 成员以**函数指针**形式登记回调函数;`esp_timer_create()` 用该结构体的指针完成创建并返回句柄;再由 `esp_timer_start_periodic()` 设定周期。底层计数器每数满一个周期(比较器报警)就调用一次 `esptim_callback`,从而翻转 LED。

### 3. 调用流程

```
app_main()
  └── led_init()                       // 配置 GPIO1 为输出
  └── esptim_int_init(2000000)         // 设定 2s 周期
        ├── esp_timer_create()         // 创建定时器实例,得到句柄
        └── esp_timer_start_periodic() // 启动周期定时器
              ⤵ (每 2s 由硬件自动触发)
        └── esptim_callback()          // 回调中 LED_TOGGLE()
```

### 4. `CMakeLists.txt` 依赖

使用 esp_timer 需要在 `components/BSP/CMakeLists.txt` 的依赖库 `REQUIRES` 中加入 `esp_timer`(以及 GPIO 所需的 `driver`),否则编译时找不到相关接口:

```cmake
set(requires
    driver
    esp_timer)
```

---

## 四、硬件设计

本实验使用的高分辨率定时器(ESP 定时器)是 **ESP32-S3 的片上资源**,不占用外部引脚,**没有对应的连接原理图**。唯一的外部器件是 LED(接 `GPIO1`),其点亮逻辑与限流接法见 LED 实验说明。

实验现象:程序运行后配置高分辨率定时器并开启中断,在中断回调函数中翻转 LED 状态,可观察到 LED 每 2 秒改变一次亮灭状态。

---

## 五、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口,初始化 LED 与定时器(周期 `2000000us`) |
| `components/BSP/ESPTIM/esptim.h` | 定时器初始化与回调函数声明 |
| `components/BSP/ESPTIM/esptim.c` | `esptim_int_init()` 创建并启动定时器,`esptim_callback()` 翻转 LED |
| `components/BSP/LED/led.h` / `led.c` | LED 引脚定义、操作宏与初始化 |
| `components/BSP/CMakeLists.txt` | BSP 组件构建配置,需在 `REQUIRES` 中加入 `esp_timer` |
