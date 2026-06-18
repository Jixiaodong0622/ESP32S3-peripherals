# 实验三　外部中断实验(GPIO Interrupt)

本实验配置 BOOT 按键所在的 GPIO 为**下降沿触发中断**:按键按下时由硬件自动触发中断,在中断服务函数(ISR)里翻转 LED,主循环则完全空闲。是学习 **GPIO 中断机制** 的入门例程。

- 实验平台:正点原子 ESP32-S3 开发板
- 中断引脚:`GPIO0`(BOOT 键,见 `components/BSP/EXIT/exit.h` 中的 `BOOT_INT_GPIO_PIN`)
- 触发方式:下降沿(`GPIO_INTR_NEGEDGE`)
- 实验现象:每按一次 BOOT 键,LED 翻转一次;主函数 `while(1)` 为空

---

## 一、中断原理

### 1. 什么是中断

中断(Interrupt)是一种"事件驱动"机制:当特定事件发生时,CPU 暂停当前正在执行的程序,转去执行预先登记好的处理函数(中断服务函数 ISR),处理完再返回原来被打断的地方继续执行。

与上一实验**按键轮询**对比:

| 方式 | 工作机制 | 优点 | 缺点 |
| --- | --- | --- | --- |
| 轮询(02_key) | 主循环不停地读引脚电平 | 简单直观 | 占用 CPU,有扫描周期延迟 |
| 中断(本实验) | 事件发生时硬件自动打断 CPU | 实时性高、CPU 空闲时可休眠 | 配置稍复杂,ISR 有诸多限制 |

本实验主循环为空,CPU 不做任何按键检测,完全靠中断响应,体现了中断的"按需触发"优势。

### 2. GPIO 中断的触发方式

ESP32 的 GPIO 中断可按引脚电平的"边沿"或"电平"触发:

| 触发类型 | 宏 | 含义 |
| --- | --- | --- |
| 下降沿 | `GPIO_INTR_NEGEDGE` | 电平从高变低瞬间触发(本实验) |
| 上升沿 | `GPIO_INTR_POSEDGE` | 电平从低变高瞬间触发 |
| 双边沿 | `GPIO_INTR_ANYEDGE` | 上升、下降沿都触发 |
| 低电平 | `GPIO_INTR_LOW_LEVEL` | 电平为低时持续触发 |
| 高电平 | `GPIO_INTR_HIGH_LEVEL` | 电平为高时持续触发 |
| 关闭 | `GPIO_INTR_DISABLE` | 不产生中断 |

按键接法为"按下接地"(低电平有效),配上拉时:**按下瞬间 = 高→低 = 下降沿**,因此选用 `GPIO_INTR_NEGEDGE`,在按下那一刻触发一次。

### 3. 中断服务函数(ISR)的编写要点

ISR 是在中断上下文中执行的,必须遵守严格约束:

- **要短、要快**:ISR 应尽快返回,耗时操作(打印、延时、复杂运算)放到主任务里完成,通常用队列/信号量把事件传出去;
- **不要调用阻塞 API**:如 `vTaskDelay()`、普通 `printf` 等不应在 ISR 里使用;FreeRTOS 中需要用带 `FromISR` 后缀的接口;
- **用 `IRAM_ATTR` 修饰**:把 ISR 放进 IRAM 运行,避免 Flash 读写(擦写)期间因代码在 Flash 中而无法响应中断,提高实时性与稳定性。

```c
static void IRAM_ATTR exit_gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;   /* 注册时传入的引脚号 */
    if (gpio_num == BOOT_INT_GPIO_PIN)
        LED_TOGGLE();                    /* 简单翻转,快速返回 */
}
```

> 注意:本实验 ISR 中直接翻转 LED 属于教学简化。真实抖动场景下,机械按键的边沿抖动可能触发多次中断,严谨做法是在 ISR 里只发信号、由任务做消抖处理。

---

## 二、中断相关 API

ESP-IDF 的 GPIO 中断接口位于 `driver/gpio.h`。

### 1. `gpio_config()` —— 配置引脚并指定中断触发方式

与输入实验相同,但 `intr_type` 设为边沿/电平触发类型:

| 成员 | 本实验取值 | 说明 |
| --- | --- | --- |
| `intr_type` | `GPIO_INTR_NEGEDGE` | 下降沿触发 |
| `mode` | `GPIO_MODE_INPUT` | 输入模式 |
| `pin_bit_mask` | `1ull << BOOT_INT_GPIO_PIN` | 目标引脚 |
| `pull_up_en` | `GPIO_PULLUP_ENABLE` | 上拉,默认高电平 |

### 2. `gpio_install_isr_service()` —— 安装 GPIO 中断服务

```c
esp_err_t gpio_install_isr_service(int intr_alloc_flags);
```

为 GPIO 模块安装统一的中断服务(全局只需调用一次)。安装后即可用 `gpio_isr_handler_add()` 为各引脚单独注册回调。`intr_alloc_flags` 用于指定中断分配标志,本实验传 `ESP_INTR_FLAG_EDGE`(边沿触发)。

### 3. `gpio_isr_handler_add()` —— 为引脚注册中断回调

```c
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num,
                               gpio_isr_t isr_handler,
                               void *args);
```

把某个引脚和它的处理函数绑定。`args` 会原样传给回调函数——本实验传入引脚号,使 ISR 内能判断是哪个引脚触发的。

```c
gpio_isr_handler_add(BOOT_INT_GPIO_PIN,
                     exit_gpio_isr_handler,
                     (void *)BOOT_INT_GPIO_PIN);
```

### 4. `gpio_intr_enable()` / `gpio_intr_disable()` —— 使能/关闭引脚中断

```c
esp_err_t gpio_intr_enable(gpio_num_t gpio_num);
esp_err_t gpio_intr_disable(gpio_num_t gpio_num);
```

控制单个引脚中断的开关。注册回调后需要调用 `gpio_intr_enable()` 才会真正开始响应中断。

### 5. 其他相关 API(扩展)

| 函数 | 作用 |
| --- | --- |
| `gpio_isr_handler_remove(gpio_num)` | 移除某引脚的中断回调 |
| `gpio_uninstall_isr_service()` | 卸载 GPIO 中断服务 |
| `gpio_set_intr_type(gpio_num, type)` | 运行时修改触发方式 |

---

## 三、中断的一般使用流程

无论是 GPIO 中断还是其他外设中断,典型步骤一致:

```
① 配置 GPIO          gpio_config()
   (输入模式 + 上拉 + 指定 intr_type 触发方式)
        │
        ▼
② 安装中断服务        gpio_install_isr_service(flags)
   (整个程序只装一次)
        │
        ▼
③ 注册中断回调        gpio_isr_handler_add(pin, isr, args)
   (把引脚和 ISR 绑定,传入参数)
        │
        ▼
④ 使能引脚中断        gpio_intr_enable(pin)
        │
        ▼
⑤ 编写 ISR           static void IRAM_ATTR isr(void *arg){...}
   (短小、不阻塞、必要时只发信号给任务)
```

对应本实验 `exit_init()` 的代码顺序:

```c
void exit_init(void)
{
    gpio_config(&gpio_init_struct);                  // ① 配置(intr_type=下降沿)
    gpio_install_isr_service(ESP_INTR_FLAG_EDGE);    // ② 安装中断服务
    gpio_isr_handler_add(BOOT_INT_GPIO_PIN,          // ③ 注册回调
                         exit_gpio_isr_handler,
                         (void *)BOOT_INT_GPIO_PIN);
    gpio_intr_enable(BOOT_INT_GPIO_PIN);             // ④ 使能中断
}
// ⑤ ISR: exit_gpio_isr_handler() 在按键按下时被自动调用
```

整体运行流程:

```
app_main()
  ├── led_init()      // LED 输出
  ├── exit_init()     // 按上面 5 步配置中断
  └── while(1){}      // 主循环空转,一切交给中断

   ┌───────────────────────────────────────┐
   │ 按下 BOOT → GPIO0 下降沿 → 硬件触发中断 │
   │   → CPU 跳转执行 exit_gpio_isr_handler  │
   │   → LED_TOGGLE() → 返回主循环           │
   └───────────────────────────────────────┘
```

---

## 四、硬件原理图

BOOT 键接在 `GPIO0`,与按键实验接法相同(低电平有效):

```
        3.3V
         │
        ┌┴┐
        │ │  上拉电阻(本实验用内部上拉)
        └┬┘
         │
    GPIO0 ●────────► ESP32-S3(中断输入)
         │
         │  ┌──────┐
         └──┤ 按键 ├──── GND
            └──────┘
```

- 平时:上拉到高电平,无中断;
- 按下瞬间:高→低,产生**下降沿**,触发中断。

> GPIO0 同时是芯片的 **BOOT 启动选择引脚**,接外部电路时注意不要影响开发板上电启动。

---

## 五、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口,初始化后主循环空转 |
| `components/BSP/EXIT/exit.h` | 中断引脚定义与函数声明 |
| `components/BSP/EXIT/exit.c` | 中断初始化 `exit_init()` 与 ISR `exit_gpio_isr_handler()` |
| `components/BSP/LED/led.*` | LED 驱动(详见 01_led 实验) |
| `components/BSP/KEY/key.*` | 按键轮询驱动(详见 02_key 实验,可与中断方式对比) |
