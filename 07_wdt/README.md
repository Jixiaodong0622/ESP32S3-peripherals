# 实验七　看门狗实验(WatchDog,基于定时器软件模拟)

本实验用 ESP32-S3 的**高分辨率定时器(esp_timer)** 来**软件模拟看门狗**:定时器在超时时调用回调函数复位芯片,只要在超时前"喂狗"(重启计时)芯片就不会复位。它对应正点原子《DNESP32S3 使用指南-IDF 版》第十六章 WATCH_DOG 实验。

- 实验平台:正点原子 ESP32-S3 开发板
- LED 连接引脚:`GPIO1`(见 `components/BSP/LED/led.h`)
- 喂狗按键:`GPIO0`(BOOT 键,见 `components/BSP/KEY/key.h`)
- 本实验参数(见 `main/main.c`):看门狗超时时间 `1000000us = 1s`,即 1 秒内不喂狗就复位

> 因为是**软件模拟**看门狗,所以用到的 API 全部是**定时器相关函数**(`esp_timer_*`),并没有用真正的硬件看门狗外设。理解了上一章 ESPTIMER 实验,本章就很容易上手。

---

## 一、看门狗简介

### 1. 看门狗是什么

MCU 可能工作在复杂环境中,受电磁干扰等影响会出现**程序跑飞**(陷入死循环、无法继续正常执行)。看门狗(WatchDog)就是为应对这种情况而生的。

**看门狗的本质也是一个定时器**:程序启动后,必须在规定时间内周期性地给它一个信号,俗称**"喂狗"**。

- 如果按时喂狗 → 看门狗一直被重置,不会触发 → 系统正常运行;
- 如果**没有按时喂狗**(说明程序卡死或跑飞)→ 看门狗超时 → 向系统发出**复位信号**,让整个系统重启,重新回到正常工作状态。

因此看门狗能够帮助检测并处理系统/软件的异常行为。

### 2. ESP32-S3 的看门狗资源

ESP32-S3 本身提供了多种**硬件**看门狗:

- 3 个数字看门狗定时器(Task WDT / Interrupt WDT 等);
- 1 个模拟看门狗定时器;
- 1 个 XTAL32K 看门狗定时器。

它们各自在特定条件下运行。**而本实验不直接用这些硬件外设,而是用一个普通定时器(esp_timer)来"模拟"看门狗的行为**,目的是帮助理解看门狗"不喂狗就复位"的核心机制。

### 3. 用定时器模拟看门狗的思路

| 看门狗概念 | 本实验用定时器的对应实现 |
| --- | --- |
| 看门狗倒计时 | 一个周期为 `Tout` 的 `esp_timer` 定时器 |
| 超时溢出(未喂狗)| 定时器到期,触发回调 `wdt_isr_handler()` |
| 复位系统 | 回调里调用 `esp_restart()` 复位芯片 |
| 喂狗(重置倒计时)| 调用 `esp_timer_restart()` 重新计时 |
| 喂狗超时时间 `Tout` | 定时器周期 `tps`(本实验 1s)|

**实验现象**:程序启动后若一直不按 BOOT,定时器每隔约 1 秒超时一次 → `esp_restart()` 复位 → LED 因不断复位而闪烁;若以**小于 1 秒**的间隔频繁按下 BOOT 喂狗,则每次都在超时前重置了倒计时,芯片不复位,LED 保持常亮。

---

## 二、定时器(看门狗)API 介绍

本实验所有功能都基于 `esp_timer`,头文件:

```c
#include "esp_timer.h"
```

### 1. `esp_timer_create()` —— 创建定时器实例

```c
esp_err_t esp_timer_create(const esp_timer_create_args_t *create_args,
                           esp_timer_handle_t *out_handle);
```

| 形参 | 说明 |
| --- | --- |
| `create_args` | 指向创建参数结构体的指针,描述回调函数等 |
| `out_handle` | **输出参数**:创建成功后写入定时器句柄,后续操作都用它 |

`esp_timer_create_args_t` 本实验设置了两个成员:

| 成员 | 说明 | 本实验取值 |
| --- | --- | --- |
| `callback` | 定时器到期时调用的回调函数 | `&wdt_isr_handler`(超时即复位) |
| `arg` | 传给回调的参数 | `NULL`(不带参数) |

### 2. `esp_timer_start_periodic()` —— 启动周期定时器(开启看门狗)

```c
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period_us);
```

| 形参 | 说明 |
| --- | --- |
| `timer` | `esp_timer_create()` 返回的句柄 |
| `period_us` | 定时周期,**单位微秒(µs)**,即看门狗的超时时间 `Tout` |

本实验传入 `1000000`(= 1s):每 1 秒触发一次回调。**只要这 1 秒内没人喂狗,就会复位。**

### 3. `esp_timer_restart()` —— 重启计时(喂狗)⭐

```c
esp_err_t esp_timer_restart(esp_timer_handle_t timer, uint64_t timeout_us);
```

| 形参 | 说明 |
| --- | --- |
| `timer` | 要重启的定时器句柄 |
| `timeout_us` | 重新计时的超时时间,**单位微秒(µs)** |

这是**喂狗**的核心:调用后定时器**立即重新开始倒计时**,之前已经走过的时间清零。

- 若该定时器是**一次性(one-shot)** 定时器:立即重启,`timeout_us` 微秒后超时一次;
- 若是**周期性(periodic)** 定时器(本实验):立即以新的 `timeout_us` 微秒为周期重新开始。

> 喂狗的本质就是"在超时前把倒计时重新拨回起点",让超时点永远到不了。

### 4. `esp_restart()` —— 复位芯片(看门狗触发)

```c
void esp_restart(void);
```

- 无参数,**软件复位整个芯片**,用以模拟"没喂狗 → 系统重启";
- 可从 PRO_CPU 或 APP_CPU 调用;
- 复位后 CPU 的重启原因为 `SW_CPU_reset`;部分外设(Wi-Fi、BT、UART0、SPI1 及传统定时器等)不会被复位;
- 此函数**不会返回**(执行后程序从头开始)。

---

## 三、驱动解析与调用流程

看门狗驱动位于 `components/BSP/WDT/`,由 `wdt.h`(声明)和 `wdt.c`(实现)组成。

### 1. `wdt.h` —— 函数声明

```c
void wd_init(uint16_t arr, uint64_t tps);        /* 初始化模拟看门狗 */
void IRAM_ATTR wdt_isr_handler(void *arg);       /* 超时回调:复位芯片 */
void restart_timer(uint64_t timeout);            /* 喂狗:重启计时 */
```

### 2. `wdt.c` —— 初始化、超时回调、喂狗

```c
esp_timer_handle_t esp_tim_handle;     /* 全局句柄,喂狗时需要用到 */

void wd_init(uint16_t arr, uint64_t tps)
{
    esp_timer_create_args_t tim_periodic_arg = {
        .arg = NULL,
        .callback = &wdt_isr_handler,           /* 超时即执行复位 */
    };
    esp_timer_create(&tim_periodic_arg, &esp_tim_handle); /* 创建定时器 */
    esp_timer_start_periodic(esp_tim_handle, tps);        /* 以 tps(us) 为超时周期启动 */
}

void IRAM_ATTR wdt_isr_handler(void *arg)
{
    esp_restart();                              /* 未及时喂狗 → 复位芯片 */
}

void restart_timer(uint64_t timeout)
{
    esp_timer_restart(esp_tim_handle, timeout); /* 喂狗:重新倒计时 */
}
```

> **关于 `wd_init` 的第一个参数 `arr`**:本实现并未使用它(仅为与传统硬件看门狗"重装载值(autoreload)"接口形式保持一致而保留)。真正决定超时时间的是第二个参数 `tps`。`main.c` 调用 `wd_init(5000, 1000000)`,其中 `5000` 被忽略,`1000000us = 1s` 才是看门狗超时时间。
>
> `IRAM_ATTR` 把回调放进内部 RAM,保证执行时不受 Flash 访问影响、响应更快。

### 3. 主程序逻辑(`main.c`)

```
app_main()
  ├── led_init() / key_init()           // 初始化 LED 与 BOOT 按键
  ├── wd_init(5000, 1000000)            // 开启看门狗:超时 1s
  ├── LED(0)                            // 点亮 LED(常亮表示未复位)
  └── while(1)
        ├── if (key_scan(0)==BOOT_PRES) // 检测到 BOOT 按下
        │       restart_timer(1000000)  //   → 喂狗:重置 1s 倒计时
        └── vTaskDelay(10ms)            // 每 10ms 扫描一次按键
```

- **不按 BOOT**:每过 1 秒 `wdt_isr_handler()` 触发 `esp_restart()` 复位,程序重头执行,LED 随之"闪一下" → 看到 LED 周期性闪烁;
- **频繁按 BOOT(间隔 < 1s)**:每次都在超时前 `restart_timer()` 喂狗,倒计时永远到不了头,芯片不复位 → LED 常亮。

### 4. `CMakeLists.txt` 依赖

BSP 组件在 `REQUIRES` 中需加入 `driver`(GPIO/按键用)和 `esp_timer`(看门狗模拟用):

```cmake
set(requires
    driver
    esp_timer)
```

---

## 四、硬件设计

本实验使用的定时器是 **ESP32-S3 的片上资源**,不占用外部引脚,**没有连接原理图**。涉及的外部器件为:

- LED(`GPIO1`):指示是否发生复位——常亮=未复位,闪烁=不断复位;
- BOOT 按键(`GPIO0`):喂狗输入,低电平有效(详见按键实验)。

实验现象:LED 大约每 1 秒闪烁一次(WDT 不断复位芯片);若以小于 1 秒的间隔频繁按下 BOOT 及时喂狗,则 LED 保持常亮。

---

## 五、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口:初始化后循环扫描 BOOT,按下则喂狗 |
| `components/BSP/WDT/wdt.h` | 看门狗(定时器)函数声明 |
| `components/BSP/WDT/wdt.c` | `wd_init()` 开启看门狗、`wdt_isr_handler()` 超时复位、`restart_timer()` 喂狗 |
| `components/BSP/KEY/key.*` | BOOT 按键驱动(详见按键实验) |
| `components/BSP/LED/led.*` | LED 驱动(详见 LED 实验) |
| `components/BSP/CMakeLists.txt` | BSP 构建配置,`REQUIRES` 含 `driver`、`esp_timer` |
