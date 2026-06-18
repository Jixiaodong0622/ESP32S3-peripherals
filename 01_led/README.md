# 实验一　LED 实验(GPIO 输出)

本实验通过控制 GPIO 输出高低电平,实现 LED 的点亮、熄灭与定时翻转,是学习 ESP32 外设的入门例程。

- 实验平台:正点原子 ESP32-S3 开发板
- LED 连接引脚:`GPIO1`(见 `components/BSP/LED/led.h` 中的 `LED_GPIO_PIN`)

---

## 一、GPIO 输出原理

### 1. 什么是 GPIO

GPIO(General Purpose Input/Output,通用输入输出)是 MCU 上可由软件灵活配置的数字引脚。每个引脚都可以被配置为:

- **输入模式**:读取外部电平(按键、传感器等);
- **输出模式**:由 MCU 主动输出高/低电平,驱动 LED、继电器等;
- **复用功能**:连接到内部外设(UART、SPI、I2C、PWM 等)。

本实验只用到**输出模式**。

### 2. 电平与逻辑

ESP32 是 3.3V 系统,数字引脚只有两种稳定状态:

| 逻辑值 | 电平 | 宏定义 |
| --- | --- | --- |
| 1(高) | ≈3.3V | `PIN_SET` |
| 0(低) | ≈0V | `PIN_RESET` |

软件写入 `1` 或 `0`,GPIO 输出寄存器对应位被置位/清零,引脚电压随之改变,从而控制外部器件。

### 3. LED 的点亮逻辑

LED 是有极性的器件,需满足"阳极电位 > 阴极电位 + 导通压降"才会发光,并且必须串联**限流电阻**防止烧毁。根据接法不同,点亮逻辑相反:

- **灌电流(低电平点亮)**:`3.3V → 电阻 → LED → GPIO`,引脚输出 **低电平** 时 LED 导通发光;
- **拉电流(高电平点亮)**:`GPIO → 电阻 → LED → GND`,引脚输出 **高电平** 时 LED 导通发光。

因此驱动代码里"写 1 是亮还是灭"完全取决于硬件接法,这也是 `led.c` 中注释"具体亮灭取决于硬件接法"的原因。

### 4. 上下拉电阻

为避免引脚悬空(电平不确定),可使能内部上拉/下拉电阻,使引脚在未被驱动时保持一个确定的默认电平。本实验在初始化时使能了内部上拉(`GPIO_PULLUP_ENABLE`)。

---

## 二、GPIO API 介绍

ESP-IDF 的 GPIO 驱动位于头文件 `driver/gpio.h`,本实验用到以下几个核心接口。

### 1. `gpio_config()` —— 批量配置引脚

```c
esp_err_t gpio_config(const gpio_config_t *pGPIOConfig);
```

通过一个配置结构体 `gpio_config_t` 一次性完成模式、上下拉、中断等设置:

| 成员 | 说明 | 本实验取值 |
| --- | --- | --- |
| `pin_bit_mask` | 要配置的引脚位掩码,第 n 位为 1 表示配置 GPIOn | `1ull << LED_GPIO_PIN` |
| `mode` | 引脚模式 | `GPIO_MODE_INPUT_OUTPUT`(可输出也可读回) |
| `pull_up_en` | 内部上拉使能 | `GPIO_PULLUP_ENABLE` |
| `pull_down_en` | 内部下拉使能 | `GPIO_PULLDOWN_DISABLE` |
| `intr_type` | 中断触发类型 | `GPIO_INTR_DISABLE`(不用中断) |

> `pin_bit_mask` 用位掩码,可同时配置多个引脚,例如 `(1ull<<1) | (1ull<<2)`。
>
> 这里选 `GPIO_MODE_INPUT_OUTPUT` 而不是纯输出 `GPIO_MODE_OUTPUT`,是因为 `LED_TOGGLE()` 需要用 `gpio_get_level()` 读回当前电平再取反。

### 2. `gpio_set_level()` —— 设置输出电平

```c
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
```

`level` 为 1 输出高电平,为 0 输出低电平。本实验用宏封装:

```c
#define LED(x) \
    do { x ? gpio_set_level(LED_GPIO_PIN, PIN_SET) \
           : gpio_set_level(LED_GPIO_PIN, PIN_RESET); } while (0)
```

### 3. `gpio_get_level()` —— 读取引脚电平

```c
int gpio_get_level(gpio_num_t gpio_num);
```

返回当前引脚电平(0 或 1)。配合取反即可实现翻转:

```c
#define LED_TOGGLE() \
    do { gpio_set_level(LED_GPIO_PIN, !gpio_get_level(LED_GPIO_PIN)); } while (0)
```

### 4. 常用辅助 API(扩展)

| 函数 | 作用 |
| --- | --- |
| `gpio_reset_pin(gpio_num_t)` | 复位引脚到默认状态 |
| `gpio_set_direction(gpio_num_t, mode)` | 单独设置引脚方向 |
| `gpio_set_pull_mode(gpio_num_t, pull)` | 单独设置上下拉 |

### 5. 调用流程

```
app_main()
  └── led_init()                 // gpio_config() 配置 GPIO1 为输出
        └── LED(1)               // 设置初始电平
  └── while(1)
        └── LED_TOGGLE()         // 每 1s 翻转一次,实现 LED 闪烁
        └── vTaskDelay(1000ms)
```

---

## 三、硬件原理图

LED 接在 `GPIO1` 上,典型连接如下(限流电阻防止电流过大烧毁 LED):

```
         3.3V
          │
         ┌┴┐
         │ │  限流电阻 R(约 470Ω~1kΩ)
         └┬┘
          │
        ──┴──   LED(发光二极管)
         ╲ ╱
        ──┬──
          │
       GPIO1 ────► ESP32-S3
```

上图为**低电平点亮(灌电流)**接法:GPIO1 输出低电平时,电流由 3.3V 经电阻、LED 流入引脚,LED 点亮;输出高电平时两端无压差,LED 熄灭。

> 部分开发板采用相反的**高电平点亮(拉电流)**接法(`GPIO → 电阻 → LED → GND`)。请以自己手中开发板的原理图为准,接法决定了 `LED(1)` 是点亮还是熄灭。

### 关键设计要点

1. **必须串联限流电阻**:LED 正向压降约 1.8~3.0V,直接接电源会因电流过大损坏 LED 或引脚;
2. **注意引脚驱动能力**:ESP32 单个 GPIO 输出电流有限(数十 mA 量级),驱动大电流负载需用三极管/MOS 管隔离;
3. **复用引脚的注意事项**:部分引脚有特殊用途(如启动选择、烧录串口),选用前需查阅芯片手册确认可自由使用。

---

## 四、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口,初始化后循环翻转 LED |
| `components/BSP/LED/led.h` | LED 引脚定义与操作宏 |
| `components/BSP/LED/led.c` | LED 初始化函数 `led_init()` |
