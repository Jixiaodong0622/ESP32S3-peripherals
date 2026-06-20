#include "uart.h"

/**
 * @brief       初始化串口
 * @param       baudrate : 波特率, 根据自己需要设置波特率值(如 9600、115200 等)
 * @retval      无
 */
void usart_init(uint32_t baudrate)
{
    /* 串口参数配置结构体, 初始化为 0 */
    uart_config_t uart0_config = {0};
    uart0_config.baud_rate = baudrate;                 /* 设置波特率, 由入参传入 */
    uart0_config.data_bits = UART_DATA_8_BITS;         /* 数据位: 8 位 */
    uart0_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE; /* 硬件流控: 禁用 */
    uart0_config.parity = UART_PARITY_DISABLE;         /* 校验位: 无校验 */
    uart0_config.source_clk = UART_SCLK_APB;           /* 时钟源: APB 时钟 */
    uart0_config.stop_bits = UART_STOP_BITS_1;         /* 停止位: 1 位 */

    /* 应用上面的参数配置到指定串口 */
    uart_param_config(USART_UX, &uart0_config);

    /* 设置串口使用的引脚: 串口号, TX 引脚, RX 引脚, RTS/CTS 不使用 */
    uart_set_pin(USART_UX, USART_TX_GPIO_PIN, USART_RX_GPIO_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* 安装串口驱动并分配收发缓冲区
     * 参数: 串口号, 接收缓冲区大小, 发送缓冲区大小, 事件队列大小, 队列句柄(不使用), 中断分配标志
     */
    uart_driver_install(USART_UX, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 20, NULL, 0);
}