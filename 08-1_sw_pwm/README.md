# 实验八-1　软件 PWM 实验(LEDC 呼吸灯)

本实验使用 ESP32-S3 的 **LED PWM 控制器(LEDC)** 输出 PWM 信号,并在程序中**用软件不断改变占空比**,让 LED 由暗渐亮、再由亮渐暗,实现**呼吸灯**效果。它对应正点原子《DNESP32S3 使用指南-IDF 版》第十七章 SW_PWM 实验。

- 实验平台:正点原子 ESP32-S3 开发板
- PWM 输出引脚:`GPIO1`(接 LED,见 `components/BSP/PWM/pwm.h` 中的 `LEDC_PWM_CH0_GPIO`)
- 本实验参数(见 `main/main.c` 的 `pwm_init(10, 1000)`):占空比分辨率 **10 位**(duty 范围 `0 ~ 1023`),PWM 频率 **1000Hz**

> 「软件 PWM」指的是:PWM 波形本身由 **LEDC 硬件**自动产生,而**占空比的连续变化是由 CPU 在循环里一步步写入的**。下一章(HW_PWM)会改用 LEDC 的硬件渐变功能,占空比由硬件自动平滑过渡,无需 CPU 干预。

---

## 一、PWM 基础知识

### 1. 什么是 PWM

PWM(Pulse Width Modulation,**脉宽调制**)是一种把模拟量用数字脉冲来"等效"表达的技术。通过调节方波中高电平所占的比例,就能等效地控制输出的"平均能量",常用于:

- 调节 LED 亮度;
- 控制直流电机转速、舵机角度;
- 生成各类驱动信号。

### 2. PWM 的三个核心参数

| 参数 | 含义 | 关系 |
| --- | --- | --- |
| **频率(frequency)** | 1 秒内有多少个 PWM 周期,单位 Hz | 本实验 `1000Hz` |
| **周期(period)** | 一个完整 PWM 信号的时长 `T` | `T = 1 / f`(1000Hz → 1ms) |
| **占空比(duty cycle)** | 一个周期内高电平时间占整个周期的比例,`0% ~ 100%` | 决定等效输出大小 |

> 例:周期 10ms、高电平 8ms,则占空比 = 8/10 = **80%**。

```
占空比 25%        占空比 50%        占空比 75%
 ┌─┐             ┌──┐            ┌───┐
 │ │   ┌─┐       │  │   ┌──┐     │   │  ┌───┐
─┘ └───┘ └─    ──┘  └───┘  └─   ─┘   └──┘   └
 ◄T►            ◄ T ►          ◄ T ►   (周期 T 不变,高电平占比改变)
```

### 3. 占空比与亮度的关系

LED 亮 1s 灭 1s,看到的是闪烁;把周期缩到 200ms(亮 100ms 灭 100ms)就是高频闪烁;继续缩短周期,**人眼分辨不出闪烁**时,看到的就是一个介于"全亮"和"全灭"之间的中间亮度。于是:**占空比越大 → 平均通电时间越长 → LED 越亮**。不断改变占空比,就得到呼吸灯。

### 4. ESP32-S3 的 LEDC 控制器

ESP32-S3 的 LED PWM 控制器(简称 **LEDC**)专门用于生成 PWM:

- 有 **8 个独立通道**,可同时输出 8 路独立波形(可驱动 RGB LED 等);
- 内部有 **4 个定时器**,每个 PWM 通道从中**选择一个定时器**,以其计数值为基准生成 PWM;
- 可在**无需 CPU 干预**的情况下自动改变占空比(硬件渐变,见下一章);
- ⚠️ **ESP32-S3 仅支持低速模式 `LEDC_LOW_SPEED_MODE`**。

**配置 PWM 的基本流程**:先配置定时器(频率、分辨率、时钟源)→ 再配置通道(选定时器、绑定 GPIO、初始占空比)→ 然后就能在该 GPIO 上输出 PWM。

#### 关键概念:占空比分辨率(duty_resolution)

占空比不是直接写百分比,而是写一个**整数计数值**。分辨率为 `N` 位时,占空比取值范围是 `0 ~ (2^N - 1)`:

- 本实验 `resolution = 10` 位 → duty 范围 `0 ~ 1023`;
- `duty = 0` 对应 0%,`duty = 1023` 对应 ≈100%,`duty = 512` 对应 ≈50%;
- 分辨率范围一般 **10 ~ 15 位**(也可低至 1 位)。分辨率越高,亮度调节越细腻,但频率上限会受时钟源限制。

---

## 二、LEDC API 介绍

LEDC 驱动位于头文件:

```c
#include "driver/ledc.h"
```

### 1. `ledc_timer_config()` —— 配置 LEDC 定时器

```c
esp_err_t ledc_timer_config(const ledc_timer_config_t *timer_conf);
```

| 形参 | 说明 |
| --- | --- |
| `timer_conf` | 指向 LEDC 定时器配置结构体的指针 |

**返回值**:`ESP_OK` 成功,其他失败。

`ledc_timer_config_t` 结构体成员:

| 成员 | 说明 | 本实验取值 |
| --- | --- | --- |
| `speed_mode` | 速度模式 | `LEDC_LOW_SPEED_MODE`(S3 只支持低速) |
| `timer_num` | 使用哪个定时器:`LEDC_TIMER_0`~`LEDC_TIMER_3` | `LEDC_TIMER_1` |
| `freq_hz` | PWM 信号频率,单位 Hz | `1000` |
| `duty_resolution` | 占空比分辨率(位数),决定 duty 取值范围 | `10`(→ 0~1023) |
| `clk_cfg` | 时钟源:`LEDC_AUTO_CLK`/`LEDC_USE_APB_CLK`/`LEDC_USE_XTAL_CLK`/`LEDC_USE_RC_FAST_CLK` 等 | `LEDC_AUTO_CLK`(自动选择) |

> 时钟源会限制 PWM 频率上限:源时钟频率越高,可设的 PWM 频率上限越高。`LEDC_AUTO_CLK` 让驱动按频率/分辨率自动挑选合适时钟。

### 2. `ledc_channel_config()` —— 配置 LEDC 通道

```c
esp_err_t ledc_channel_config(const ledc_channel_config_t *ledc_conf);
```

| 形参 | 说明 |
| --- | --- |
| `ledc_conf` | 指向 LEDC 通道配置结构体的指针 |

`ledc_channel_config_t` 结构体成员:

| 成员 | 说明 | 本实验取值 |
| --- | --- | --- |
| `gpio_num` | PWM 输出的 GPIO 引脚 | `GPIO_NUM_1` |
| `speed_mode` | 速度模式,**必须与定时器一致** | `LEDC_LOW_SPEED_MODE` |
| `channel` | 输出通道号 `0~7` | `LEDC_CHANNEL_1` |
| `intr_type` | 是否使能通道中断:`LEDC_INTR_DISABLE` / `LEDC_INTR_FADE_END` | `LEDC_INTR_DISABLE` |
| `timer_sel` | 该通道绑定的定时器(要与上面配置的定时器对应) | `LEDC_TIMER_1` |
| `duty` | 初始占空比,范围 `0 ~ (2^duty_resolution - 1)` | `0` |
| `hpoint` | 占空比对应的起始时钟计数值 | 默认(0) |
| `output_invert` | 是否反相输出(1 反相 / 0 不反相) | 默认(0) |

配置完成后,该通道就会在指定 GPIO 上输出"由定时器决定频率、由 `duty` 决定占空比"的 PWM 信号。

### 3. `ledc_set_duty()` —— 设置新的占空比

```c
esp_err_t ledc_set_duty(ledc_mode_t speed_mode, ledc_channel_t channel, uint32_t duty);
```

| 形参 | 说明 |
| --- | --- |
| `speed_mode` | 速度模式(`LEDC_LOW_SPEED_MODE`) |
| `channel` | 要设置的通道 |
| `duty` | 新占空比值,范围 `0 ~ (2^duty_resolution - 1)` |

> ⚠️ 只调用 `ledc_set_duty()` **不会立即生效**,必须再调用 `ledc_update_duty()`。

### 4. `ledc_update_duty()` —— 让新占空比生效

```c
esp_err_t ledc_update_duty(ledc_mode_t speed_mode, ledc_channel_t channel);
```

| 形参 | 说明 |
| --- | --- |
| `speed_mode` | 速度模式 |
| `channel` | 要更新的通道 |

> `set_duty` + `update_duty` 必须**成对使用**:前者把新值写入寄存器,后者把新值真正应用到输出。本实验把这两步封装进了 `pwm_set_duty()`。
> 另有 `ledc_get_duty(speed_mode, channel)` 可读回当前占空比。

---

## 三、驱动解析与调用流程

驱动位于 `components/BSP/PWM/`,由 `pwm.h`(宏与声明)和 `pwm.c`(实现)组成。

### 1. `pwm.h` —— 引脚与通道定义

```c
#define LEDC_PWM_TIMER       LEDC_TIMER_1   /* 使用定时器 1 */
#define LEDC_PWM_CH0_GPIO    GPIO_NUM_1     /* PWM 输出引脚(接 LED) */
#define LEDC_PWM_CH0_CHANNEL LEDC_CHANNEL_1 /* 使用通道 1 */

void pwm_init(uint8_t resolution, uint16_t freq);
void pwm_set_duty(uint16_t duty);
```

### 2. `pwm.c` —— 初始化与占空比设置

```c
void pwm_init(uint8_t resolution, uint16_t freq)
{
    /* 1. 配置 LEDC 定时器:频率、分辨率、时钟源 */
    ledc_timer_config_t ledc_timer = {0};
    ledc_timer.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer.duty_resolution = resolution;     /* 10 位 → duty 0~1023 */
    ledc_timer.freq_hz         = freq;           /* 1000 Hz */
    ledc_timer.speed_mode      = LEDC_LOW_SPEED_MODE;
    ledc_timer.timer_num       = LEDC_PWM_TIMER;
    ledc_timer_config(&ledc_timer);

    /* 2. 配置通道:绑定定时器与 GPIO,初始占空比 0 */
    ledc_channel_config_t ledc_channel = {0};
    ledc_channel.channel    = LEDC_PWM_CH0_CHANNEL;
    ledc_channel.duty       = 0;
    ledc_channel.intr_type  = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num   = LEDC_PWM_CH0_GPIO;
    ledc_channel.speed_mode = LEDC_LOW_SPEED_MODE; /* 必须与定时器一致 */
    ledc_channel.timer_sel  = LEDC_PWM_TIMER;
    ledc_channel_config(&ledc_channel);
}

void pwm_set_duty(uint16_t duty)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_PWM_CH0_CHANNEL, duty); /* 写入新占空比 */
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_PWM_CH0_CHANNEL);    /* 使其生效 */
}
```

> 结构体用 `{0}` 清零很重要:`hpoint`、`output_invert` 等未显式赋值的成员会被置 0,避免脏数据导致异常。

### 3. 主程序:软件实现呼吸灯(`main.c`)

```
app_main()
  ├── led_init()
  ├── pwm_init(10, 1000)              // 10 位分辨率(0~1023), 1kHz
  └── while(1)                        // 每 10ms 调整一次占空比
        ├── vTaskDelay(10ms)
        ├── dir==1 ? val+=5 : val-=5  // 递增/递减
        ├── val>1005 → dir=0          // 到顶,改为递减
        ├── val<5    → dir=1          // 到底,改为递增
        └── pwm_set_duty(val)         // 应用新占空比
```

占空比 `val` 在 `0↔1005` 之间来回缓慢变化(每 10ms 变 5),LED 亮度随之"渐亮→渐暗"循环,形成呼吸灯。**注意这是"软件 PWM":占空比的每一步变化都由 CPU 在循环中主动写入。**

### 4. `CMakeLists.txt` 依赖

LEDC 接口在 `driver` 组件中,BSP 的 `REQUIRES` 需包含 `driver`:

```cmake
set(requires
    driver)
```

---

## 四、硬件设计

本实验使用的 LEDC 与定时器都是 **ESP32-S3 的片上资源**,不占用外部引脚,**没有连接原理图**。PWM 从 `GPIO1` 输出,而 `GPIO1` 接着 LED,所以 LED 亮度随占空比变化。

实验现象:LED 由暗逐渐变亮、再由亮逐渐变暗,如此循环,呈现呼吸灯效果。

---

## 五、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口:初始化 PWM 后循环改变占空比实现呼吸灯 |
| `components/BSP/PWM/pwm.h` | LEDC 定时器/通道/引脚宏定义与函数声明 |
| `components/BSP/PWM/pwm.c` | `pwm_init()` 配置定时器与通道,`pwm_set_duty()` 设置占空比 |
| `components/BSP/LED/led.*` | LED 驱动(详见 LED 实验) |
| `components/BSP/CMakeLists.txt` | BSP 构建配置,`REQUIRES` 含 `driver` |
