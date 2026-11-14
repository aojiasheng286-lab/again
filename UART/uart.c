#include "uart.h"
#include "usart.h"
#include "key.h"    // 需要用到 key_event 和 mode
#include <stdio.h>
#include <string.h>

// 私有变量（加上 static，别的文件看不见）
static uint8_t rx_data;      // 接收上位机发来的字符
static char tx_buff[64];     // 发送缓冲区
static uint8_t uart_event = 0; // 串口专属按键事件缓存

// =======================================================
// 1. 启动接收 (在 main.c 的初始化里调用)
// =======================================================
void uart_start_receive(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}

// =======================================================
// 2. 串口中断回调 (收到数据时自动触发)
// =======================================================
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 根据电脑发来的字符改变模式 (mode 是从 key.h 借来的)
        if (rx_data == '1')      mode = 0;
        else if (rx_data == '2') mode = 1;
        else if (rx_data == '3') mode = 2;
        else if (rx_data == '4') mode = 3;
        else if (rx_data == '5') mode = 4;

        // 重新开启接收
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

// =======================================================
// 3. 串口定时发送任务 (在 while(1) 里调用)
// =======================================================
void uart_process_task(void)
{
    // 截胡按键事件
    if (key_event != 0) {
        uart_event = key_event;
    }

    // 每 200ms 发送一次
    static uint32_t last_uart_time = 0;
    if (HAL_GetTick() - last_uart_time >= 200)
    {
        last_uart_time = HAL_GetTick();
        
        if (uart_event != 0) 
        {
            sprintf(tx_buff, "Key Event: %d\r\n", uart_event);
            uart_event = 0; // 发完清零
        }
        else 
        {
            sprintf(tx_buff, "Mode: %d, LED Running...\r\n", mode);
        }
        
        // DMA 发送
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buff, strlen(tx_buff));
    }
}