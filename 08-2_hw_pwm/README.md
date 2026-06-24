# 实验八-2　硬件 PWM 实验(LEDC 硬件渐变呼吸灯)

本实验同样使用 ESP32-S3 的 **LED PWM 控制器(LEDC)** 做呼吸灯,但与上一章(软件 PWM)不同:这里**用 LEDC 的硬件渐变(fade)功能**让占空比自动、平滑地过渡,**无需 CPU 在循环里一步步改写占空比**。它对应正点原子《DNESP32S3 使用指南-IDF 版》第十八章 HW_PWM 实验。

- 实验平台:正点原子 ESP32-S3 开发板
- PWM 输出引脚:`GPIO1`(接 LED,见 `components/BSP/PWM/pwm.h` 中的 `LEDC_PWM_CH0_GPIO`)
- 本实验参数(见 `main/main.c` 的 `pwm_init(13, 5000)`):占空比分辨率 **13 位**(duty 范围 `0 ~ 8191`),PWM 频率 **5000Hz**;渐变目标占空比 `LEDC_PWM_DUTY = 8000`,渐变时长 `LEDC_PWM_FADE_TIME = 3000ms`

> **软件 PWM vs 硬件 PWM**:两者都用 LEDC 产生波形,区别在于**占空比怎么变化**。
> - 软件方式(上一章):CPU 在 `while(1)` 里每隔几毫秒手动 `+5/-5` 改一次占空比,过渡的平滑度靠循环频率;
> - 硬件方式(本章):只需告诉 LEDC "目标占空比 + 渐变时长",**硬件自动在这段时间内平滑改变占空比**,CPU 设置完即可去做别的事。

---

## 一、PWM 与硬件渐变

### 1. PWM 基础(回顾)

PWM(脉宽调制)通过调节方波中高电平占整个周期的比例(**占空比**)来等效控制平均输出。三个核心参数:**频率**(每秒周期数,Hz)、**周期**(`T = 1/f`)、**占空比**(`0% ~ 100%`)。占空比越大,LED 平均通电越久 → 越亮。详细原理见上一章(软件 PWM)说明。

> 占空比写的是整数计数值:分辨率 `N` 位 → duty 取值 `0 ~ (2^N - 1)`。本实验 13 位 → `0 ~ 8191`,`duty=8000` 约对应 8000/8191 ≈ 97.7% 的亮度。

### 2. 什么是硬件渐变(fade)

LEDC 控制器硬件支持**自动逐渐改变占空比**:你设定一个"目标占空比"和一个"渐变时长",硬件就会在这段时间内,把占空比从**当前值**一步步平滑地推到目标值,整个过程不占用 CPU。这正是做呼吸灯/颜色渐变最省心的方式。

使用硬件渐变的标准步骤:

```
①  ledc_fade_func_install()      // 安装/使能渐变功能(只需一次)
②  ledc_set_fade_with_time()     // 配置:目标占空比 + 渐变时长
③  ledc_fade_start()             // 启动渐变
```

> 第②步除了 `ledc_set_fade_with_time()`(按时间渐变),还可选 `ledc_set_fade_with_step()`(按步进渐变)或 `ledc_set_fade()`。本实验用最常用的"按时间渐变"。

---

## 二、LEDC 渐变 API 介绍

LEDC 驱动位于头文件:

```c
#include "driver/ledc.h"
```

> 定时器与通道的配置函数(`ledc_timer_config()` / `ledc_channel_config()` 及其结构体成员)与上一章完全相同,这里不再重复,重点介绍**渐变相关的三个函数**。

### 1. `ledc_fade_func_install()` —— 安装(使能)渐变功能

```c
esp_err_t ledc_fade_func_install(int intr_alloc_flags);
```

| 形参 | 说明 |
| --- | --- |
| `intr_alloc_flags` | 用于分配中断的标志,一般传 `0`(默认) |

**返回值**:`ESP_OK` 成功。这一步**不可省略**——渐变功能依赖一个中断服务,必须先安装。本实验在 `pwm_init()` 末尾调用 `ledc_fade_func_install(0)`。

### 2. `ledc_set_fade_with_time()` —— 配置渐变(目标占空比 + 时长)

```c
esp_err_t ledc_set_fade_with_time(ledc_mode_t speed_mode,
                                  ledc_channel_t channel,
                                  uint32_t target_duty,
                                  int max_fade_time_ms);
```

| 形参 | 说明 |
| --- | --- |
| `speed_mode` | 速度模式(S3 用 `LEDC_LOW_SPEED_MODE`) |
| `channel` | 要操作的 LEDC 通道 |
| `target_duty` | **目标占空比**(渐变终点),范围 `0 ~ (2^duty_resolution - 1)` |
| `max_fade_time_ms` | **最大渐变时长**(毫秒),即从当前占空比过渡到目标占空比要用多久 |

> 此函数只是"配置"渐变参数,**并不会立即开始**,需要再调 `ledc_fade_start()`。

### 3. `ledc_fade_start()` —— 启动渐变

```c
esp_err_t ledc_fade_start(ledc_mode_t speed_mode,
                          ledc_channel_t channel,
                          ledc_fade_mode_t fade_mode);
```

| 形参 | 说明 |
| --- | --- |
| `speed_mode` | 速度模式 |
| `channel` | 要操作的 LEDC 通道 |
| `fade_mode` | 渐变模式(见下) |

`fade_mode` 可选:

| 取值 | 含义 |
| --- | --- |
| `LEDC_FADE_NO_WAIT` | **非阻塞**:函数立即返回,渐变由硬件在后台进行(本实验使用) |
| `LEDC_FADE_WAIT_DONE` | **阻塞**:等渐变完全结束后函数才返回 |

---

## 三、驱动解析与调用流程

驱动位于 `components/BSP/PWM/`,由 `pwm.h`(宏与声明)和 `pwm.c`(实现)组成。

### 1. `pwm.h` —— 引脚与渐变参数定义

```c
#define LEDC_PWM_TIMER       LEDC_TIMER_0         /* 使用定时器 0 */
#define LEDC_PWM_MODE        LEDC_LOW_SPEED_MODE  /* S3 必须用低速模式 */
#define LEDC_PWM_CH0_GPIO    GPIO_NUM_1           /* PWM 输出引脚(接 LED) */
#define LEDC_PWM_CH0_CHANNEL LEDC_CHANNEL_1       /* 使用通道 1 */
#define LEDC_PWM_DUTY        8000                 /* 渐变目标占空比 */
#define LEDC_PWM_FADE_TIME   3000                 /* 渐变时长(ms) */

void pwm_init(uint8_t resolution, uint16_t freq);
void pwm_set_duty(uint16_t duty);
```

### 2. `pwm.c` —— 初始化(含安装渐变)与渐变设置

```c
void pwm_init(uint8_t resolution, uint16_t freq)
{
    /* 1. 配置 LEDC 定时器(频率、分辨率、时钟源)——同软件 PWM 实验 */
    ledc_timer_config_t ledc_timer = {0};
    ledc_timer.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer.duty_resolution = resolution;          /* 13 位 → duty 0~8191 */
    ledc_timer.freq_hz         = freq;                /* 5000 Hz */
    ledc_timer.speed_mode      = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num       = LEDC_PWM_TIMER;
    ledc_timer_config(&ledc_timer);

    /* 2. 配置通道(绑定定时器与 GPIO) */
    ledc_channel_config_t ledc_channel = {0};
    ledc_channel.channel    = LEDC_PWM_CH0_CHANNEL;
    ledc_channel.duty       = 0;
    ledc_channel.intr_type  = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num   = LEDC_PWM_CH0_GPIO;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledc_channel.timer_sel  = LEDC_PWM_TIMER;
    ledc_channel_config(&ledc_channel);

    /* 3. 安装渐变功能(硬件 PWM 比软件 PWM 多出的关键一步) */
    ledc_fade_func_install(0);
}

void pwm_set_duty(uint16_t duty)
{
    /* 渐亮:在 FADE_TIME 毫秒内由当前占空比渐变到 duty,非阻塞启动 */
    ledc_set_fade_with_time(LEDC_PWM_MODE, LEDC_PWM_CH0_CHANNEL, duty, LEDC_PWM_FADE_TIME);
    ledc_fade_start(LEDC_PWM_MODE, LEDC_PWM_CH0_CHANNEL, LEDC_FADE_NO_WAIT);

    /* 渐暗:再在 FADE_TIME 毫秒内由 duty 渐变回 0,非阻塞启动 */
    ledc_set_fade_with_time(LEDC_PWM_MODE, LEDC_PWM_CH0_CHANNEL, 0, LEDC_PWM_FADE_TIME);
    ledc_fade_start(LEDC_PWM_MODE, LEDC_PWM_CH0_CHANNEL, LEDC_FADE_NO_WAIT);
}
```

> 与软件 PWM 的 `pwm_set_duty()` 对比:那里是 `ledc_set_duty()` + `ledc_update_duty()` 设一个固定值;这里换成 `ledc_set_fade_with_time()` + `ledc_fade_start()`,把"渐亮→渐暗"这一整段过渡交给硬件完成。

### 3. 主程序(`main.c`)

```
app_main()
  ├── led_init()
  ├── pwm_init(13, 5000)              // 13 位分辨率(0~8191), 5kHz, 并安装渐变
  └── while(1)
        ├── vTaskDelay(10ms)
        └── pwm_set_duty(LEDC_PWM_DUTY)// 触发"渐亮到 8000 → 渐暗到 0"
```

相比软件 PWM 在循环里频繁 `±5` 地手动改占空比,这里主循环只需反复调用 `pwm_set_duty()` 触发硬件渐变,占空比的平滑变化由 LEDC 硬件自动完成,实现同样的呼吸灯效果。

> 提示:`LEDC_FADE_NO_WAIT` 为非阻塞模式,`ledc_fade_start()` 调用后立即返回。若希望严格地"先完整渐亮、再完整渐暗"地串行执行,可改用 `LEDC_FADE_WAIT_DONE`(阻塞,等本段渐变结束才返回)。

### 4. `CMakeLists.txt` 依赖

LEDC 接口在 `driver` 组件中,BSP 的 `REQUIRES` 需包含 `driver`:

```cmake
set(requires
    driver)
```

---

## 四、硬件设计

本实验使用的 LEDC 与定时器都是 **ESP32-S3 的片上资源**,不占用外部引脚,**没有连接原理图**。PWM 从 `GPIO1` 输出,`GPIO1` 接 LED,故 LED 亮度随占空比渐变而变化。

实验现象:LED 由暗逐渐变亮、再由亮逐渐变暗,如此循环,呈现呼吸灯效果(与软件 PWM 实验现象一致,但占空比由硬件自动渐变)。

---

## 五、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口:初始化 PWM 后循环触发硬件渐变 |
| `components/BSP/PWM/pwm.h` | LEDC 定时器/通道/引脚及渐变参数宏定义与函数声明 |
| `components/BSP/PWM/pwm.c` | `pwm_init()` 配置并安装渐变,`pwm_set_duty()` 设置硬件渐变 |
| `components/BSP/LED/led.*` | LED 驱动(详见 LED 实验) |
| `components/BSP/CMakeLists.txt` | BSP 构建配置,`REQUIRES` 含 `driver` |
