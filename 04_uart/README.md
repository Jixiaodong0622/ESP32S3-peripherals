# 实验四　串口通信实验(UART)

本实验通过 ESP32-S3 的 UART0 与电脑进行串口通信:开发板把收到的数据原样回显(echo),空闲时定时打印提示信息并翻转 LED 指示程序运行。是学习串口收发的入门例程。

- 实验平台:正点原子 ESP32-S3 开发板
- 使用串口:`UART0`(见 `components/BSP/UART/uart.h` 中的 `USART_UX`)
- TX 发送引脚:`GPIO43`(`USART_TX_GPIO_PIN`)
- RX 接收引脚:`GPIO44`(`USART_RX_GPIO_PIN`)
- 波特率:`115200`(在 `main.c` 中调用 `usart_init(115200)` 设置)

> ESP32-S3 的 UART0 默认通过板载 USB 转串口芯片连到电脑,既用于程序下载/日志打印,也用于本实验的数据收发。打开任意串口助手,波特率设为 115200 即可与开发板对话。

---

## 一、串口通信原理

### 1. 什么是串口通信

串口(Serial Port)是一种**串行、异步、全双工**的通信方式。它的几个关键概念:

- **串行 vs 并行**:串行通信一次只在一根数据线上传送 1 个 bit,按位依次发送;并行通信用多根线同时传送多个 bit。串行线少、成本低、适合长距离,是 MCU 最常用的通信方式。
- **异步 vs 同步**:异步通信**没有单独的时钟线**,收发双方靠事先约定好的**波特率**各自计时来对齐数据,所以叫 UART(Universal Asynchronous Receiver/Transmitter,通用异步收发器)。
- **全双工**:有独立的发送线(TXD)和接收线(RXD),收和发可以同时进行。

最基本的连线只需 3 根:**TX、RX、GND**。注意必须**交叉连接**:A 的 TX 接 B 的 RX,A 的 RX 接 B 的 TX,GND 共地。

### 2. 波特率(Baud Rate)

波特率表示每秒传输的**码元(符号)个数**,单位 Baud。在 UART 这种二进制电平传输里,1 个码元就是 1 个 bit,所以波特率数值上等于每秒传输的位数(bit/s)。

常见波特率:`4800`、`9600`、`115200` 等。**收发双方的波特率必须一致**,否则采样时刻错位,会收到乱码。本实验用 `115200`。

### 3. 数据帧格式

UART 以"帧"为单位收发,空闲时数据线保持高电平。一个完整的数据帧由以下几部分组成:

```
空闲(高) │ 起始位 │ D0 D1 D2 D3 D4 D5 D6 D7 │ 校验位 │ 停止位 │ 空闲(高)
          └─ 0 ──┘└──────── 数据位(LSB先发) ────────┘└可选┘└── 1 ──┘
```

| 组成 | 说明 |
| --- | --- |
| **起始位** | 1 位低电平(0),标志一帧开始,接收方据此启动采样 |
| **数据位** | 5/6/7/8 位,通常为 8 位,**低位(LSB)先发** |
| **校验位** | 可选,用于简单检错。无校验/奇校验(ODD)/偶校验(EVEN) |
| **停止位** | 1 / 1.5 / 2 位高电平(1),标志一帧结束 |

> **奇偶校验**:把数据位中"1"的个数加上校验位,使总数为奇数(奇校验)或偶数(偶校验)。例如数据 `1100 1010` 有 4 个"1",偶校验时校验位填 0(保持偶数),奇校验时填 1(凑成奇数)。它只能发现一位翻转错误,不能纠错。

本实验采用最常见的格式:**8 位数据 + 无校验 + 1 位停止位**(简称 8-N-1)。

### 4. 电平标准

UART 协议只规定时序,不规定电平,常见有几种物理层标准:

- **TTL 电平**:MCU 引脚直接输出的电平,ESP32-S3 为 3.3V(高电平≈3.3V,低电平≈0V)。
- **RS-232**:用正负电压表示逻辑,电平高、抗干扰强,是电脑老式 COM 口标准。
- **RS-422 / RS-485**:差分信号,抗干扰强、传输距离远,常用于工业现场。

电脑现在多用 USB,因此开发板上用 **USB 转串口芯片**(如 CH340)把 TTL 串口转换成 USB,插上数据线即可在电脑端虚拟出一个串口。

### 5. ESP32-S3 的 UART 外设

ESP32-S3 内部有 **3 个 UART 控制器:UART0、UART1、UART2**,功能基本相同,都支持异步通信、IrDA、RS485 等模式,并支持硬件流控。简单了解其内部结构:

- **FIFO 缓冲**:每个 UART 各有 1024×8-bit 的 RAM 作为收发 FIFO,硬件自动缓存收发的数据,减轻 CPU 负担。
- **时钟源**:UART 内核时钟可选 `APB_CLK`、`RC_FAST_CLK`、`XTAL_CLK`,经分频器分频后得到所需波特率。本实验用 `UART_SCLK_APB`。
- **收发器**:发送器(Transmitter)把 FIFO 里的数据按帧格式从 TXD 移位发出;接收器(Receiver)检测 RXD 上的起始位后采样,把数据存入 RX FIFO。
- **硬件流控**:通过 RTS/CTS 两根额外的握手线控制收发节奏,防止接收方来不及处理而丢数据。本实验不使用(`UART_HW_FLOWCTRL_DISABLE`)。

---

## 二、UART API 介绍

ESP-IDF 的 UART 驱动位于头文件 `driver/uart.h`,本实验用到以下接口。

### 1. `uart_param_config()` —— 配置串口参数

```c
esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t *uart_config);
```

| 参数 | 说明 |
| --- | --- |
| `uart_num` | 串口号,如 `UART_NUM_0`、`UART_NUM_1`、`UART_NUM_2` |
| `uart_config` | 指向 `uart_config_t` 结构体的指针,描述波特率、数据位等参数 |

返回值:成功 `ESP_OK`,失败 `ESP_FAIL`。

其中 `uart_config_t` 结构体的成员含义:

| 成员 | 说明 | 可选值 / 本实验取值 |
| --- | --- | --- |
| `baud_rate` | 波特率 | `9600`、`115200` 等 / 本实验 `115200` |
| `data_bits` | 数据位长度 | `UART_DATA_5/6/7/8_BITS` / 本实验 `UART_DATA_8_BITS` |
| `parity` | 校验方式 | `UART_PARITY_DISABLE`(无)/`UART_PARITY_EVEN`(偶)/`UART_PARITY_ODD`(奇) / 本实验 `DISABLE` |
| `stop_bits` | 停止位 | `UART_STOP_BITS_1` / `_1_5` / `_2` / 本实验 `UART_STOP_BITS_1` |
| `flow_ctrl` | 硬件流控 | `UART_HW_FLOWCTRL_DISABLE` / `_RTS` / `_CTS` / `_CTS_RTS` / 本实验 `DISABLE` |
| `source_clk` | 时钟源 | `UART_SCLK_APB` / `UART_SCLK_XTAL` / `UART_SCLK_RTC` / `UART_SCLK_DEFAULT` / 本实验 `UART_SCLK_APB` |
| `rx_flow_ctrl_thresh` | 接收流控阈值,仅当开启 RTS 流控时有效 | 通常填 122,本实验因结构体清零为 0(未用流控,无影响) |

### 2. `uart_set_pin()` —— 指定串口引脚

```c
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_io_num, int rx_io_num,
                       int rts_io_num, int cts_io_num);
```

| 参数 | 说明 |
| --- | --- |
| `uart_num` | 串口号 |
| `tx_io_num` | 发送 TX 使用的 GPIO 编号,填 `-1` 表示不改变 |
| `rx_io_num` | 接收 RX 使用的 GPIO 编号,填 `-1` 表示不改变 |
| `rts_io_num` | 流控 RTS 的 GPIO,不用填 `UART_PIN_NO_CHANGE`(即 -1) |
| `cts_io_num` | 流控 CTS 的 GPIO,不用填 `UART_PIN_NO_CHANGE`(即 -1) |

本实验把 TX 设为 `GPIO43`、RX 设为 `GPIO44`,RTS/CTS 不使用。

### 3. `uart_driver_install()` —— 安装串口驱动

```c
esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size, int tx_buffer_size,
                              int event_queue_size, QueueHandle_t *uart_queue, int intr_alloc_flags);
```

调用它才会真正分配收发缓冲区、注册中断,串口才能工作。

| 参数 | 说明 | 本实验取值 |
| --- | --- | --- |
| `uart_num` | 串口号 | `UART_NUM_0` |
| `rx_buffer_size` | 接收环形缓冲区大小(字节),需大于硬件 FIFO(128) | `RX_BUF_SIZE*2` = 2048 |
| `tx_buffer_size` | 发送环形缓冲区大小,填 0 则发送不经缓冲、阻塞发送 | `RX_BUF_SIZE*2` = 2048 |
| `event_queue_size` | 事件队列长度,配合 `uart_queue` 使用 | `20` |
| `uart_queue` | 返回事件队列句柄的指针,不用事件机制填 `NULL` | `NULL` |
| `intr_alloc_flags` | 中断分配标志,一般填 `0` | `0` |

返回值:成功 `ESP_OK`,失败 `ESP_FAIL`。

### 4. `uart_get_buffered_data_len()` —— 查询接收缓冲区数据长度

```c
esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t *size);
```

把接收缓冲区中**已缓存、待读取的字节数**写入 `size`。本实验在主循环里用它判断是否收到了数据。

### 5. `uart_read_bytes()` —— 读取数据

```c
int uart_read_bytes(uart_port_t uart_num, void *buf, uint32_t length, TickType_t ticks_to_wait);
```

| 参数 | 说明 |
| --- | --- |
| `uart_num` | 串口号 |
| `buf` | 存放读出数据的缓冲区指针 |
| `length` | 期望读取的字节数 |
| `ticks_to_wait` | 最长等待时间(RTOS tick),数据不足时最多等这么久 |

返回值:实际读到的字节数(失败为 -1)。

### 6. `uart_write_bytes()` —— 发送数据

```c
int uart_write_bytes(uart_port_t uart_num, const void *src, size_t size);
```

| 参数 | 说明 |
| --- | --- |
| `uart_num` | 串口号 |
| `src` | 待发送数据的缓冲区指针 |
| `size` | 要发送的字节数 |

返回值:写入发送缓冲区的字节数。该函数把数据放入发送 FIFO/缓冲后即返回,**不保证数据已全部从线上发完**。若需确认发送完成,可再调用 `uart_wait_tx_done()`。

---

## 三、硬件连接

本实验使用 UART0,它通过开发板上的 USB 转串口电路连到电脑:

```
   ESP32-S3                USB转串口芯片            电脑
  ┌────────┐              ┌──────────┐
  │ GPIO43 │── TXD ──────►│ RXD      │
  │  (TX)  │              │      USB │═══════════►  USB 口
  │ GPIO44 │◄── RXD ──────│ TXD      │           (虚拟串口/COM)
  │  (RX)  │              │          │
  │  GND   │──────────────│ GND      │
  └────────┘              └──────────┘
```

> 注意 TX/RX **交叉连接**:开发板 TX(GPIO43)接对方 RX,开发板 RX(GPIO44)接对方 TX。使用板载 USB 串口时这些走线已在 PCB 上连好,直接插 USB 线即可。

LED 接在 `GPIO1`,本实验用它在串口空闲时闪烁,指示程序正在运行。

---

## 四、程序流程

```
app_main()
  ├── nvs_flash_init()           // 初始化 NVS(非易失存储)
  ├── led_init()                 // 初始化 LED(GPIO1)
  ├── usart_init(115200)         // 初始化 UART0:配置参数→设引脚→装驱动
  └── while(1)
        ├── uart_get_buffered_data_len()   // 查询是否收到数据
        ├── 若收到 (len>0):
        │     ├── uart_read_bytes()        // 读出数据到 data
        │     └── uart_write_bytes()       // 原样回显发回电脑
        └── 若空闲:
              ├── 每 5000 次打印实验标题
              ├── 每 200 次提示"请输入数据"
              ├── 每 30 次翻转一次 LED
              └── vTaskDelay(10)           // 延时释放 CPU
```

`usart_init()` 的三步固定套路(见 `uart.c`):

1. 填好 `uart_config_t` 参数 → `uart_param_config()` 应用配置;
2. `uart_set_pin()` 把 TX/RX 绑定到 GPIO43/GPIO44;
3. `uart_driver_install()` 安装驱动、分配收发缓冲区,串口开始工作。

---

## 五、实验现象

下载程序后,打开串口助手(波特率 115200,8-N-1):

- 开发板会周期性打印实验标题和"请输入数据,以回车键结束"的提示;
- 在串口助手里发送任意字符,开发板会把它**原样回显**回来;
- LED(GPIO1)持续闪烁,表示程序正常运行。

---

## 六、相关文件

| 文件 | 说明 |
| --- | --- |
| `main/main.c` | 程序入口,初始化后循环收发(回显)串口数据 |
| `components/BSP/UART/uart.h` | 串口号、收发引脚、缓冲区大小定义 |
| `components/BSP/UART/uart.c` | 串口初始化函数 `usart_init()` |
| `components/BSP/LED/led.c` | LED 初始化,用于运行指示 |
</content>
</invoke>
