#include "uart.h"
#include "usart.h"
#include "key.h"    // 需要用到 key_event 和 mode
#include "my_adc.h"
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
        uint8_t changed = 0;
        if (rx_data == '1')      { mode = 0; changed = 1; }
        else if (rx_data == '2') { mode = 1; changed = 1; }
        else if (rx_data == '3') { mode = 2; changed = 1; }
        else if (rx_data == '4') { mode = 3; changed = 1; }
        else if (rx_data == '5') { mode = 4; changed = 1; }

        // 串口切模式也要复位灯效状态，否则呼吸亮度、流水灯步数会接着上次的继续
        if (changed) led_mode_reset();

        // 重新开启接收
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

// =======================================================
// 2b. 串口错误回调
// 发生溢出(ORE)等错误后 HAL 会终止接收，不重新挂上就永久收不到数据，只能复位
// =======================================================
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        __HAL_UART_CLEAR_OREFLAG(huart);
        huart->RxState = HAL_UART_STATE_READY;
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

// =======================================================
// 3. 串口定时发送任务 (在 while(1) 里调用)
// =======================================================
void uart_process_task(void)
{
    // 读一份按键事件副本：这里只读、不清零 key_event，
    // 清零由 led_mode_task 负责，所以 main.c 里必须先调本函数、再调 led_mode_task
    if (key_event != 0) {
        uart_event = key_event;
    }

    // 每 500ms 发送一次
    static uint32_t last_uart_time = 0;
    if (HAL_GetTick() - last_uart_time >= 500)
    {
        last_uart_time = HAL_GetTick();
			
			float v =adc_get_voltage();//万用表测得电压最大为3.25
        
        if (uart_event != 0) 
        {
            sprintf(tx_buff, "Key Event: %d | ADC : %.2f V\r\n", uart_event, v);
            uart_event = 0; // 发完清零
        }
        else 
        {
            sprintf(tx_buff, "Mode: %d | ADC : %.2f V\r\n", mode, v);
        }
        
        // DMA 发送
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buff, strlen(tx_buff));
    }
}
