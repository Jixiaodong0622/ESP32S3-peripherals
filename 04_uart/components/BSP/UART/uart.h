#ifndef __UART_H__
#define __UART_H__

#include "driver/uart.h"
#include "driver/uart_select.h"
#include "driver/gpio.h"

/* 引脚和串口定义 */
#define USART_UX UART_NUM_0
#define USART_TX_GPIO_PIN GPIO_NUM_43 /* 串口发送引脚 */
#define USART_RX_GPIO_PIN GPIO_NUM_44 /* 串口接收引脚 */

/* 串口接收相关定义 */
#define RX_BUF_SIZE 1024 /* 接收缓冲区大小 */

void usart_init(uint32_t baudrate);
#endif
