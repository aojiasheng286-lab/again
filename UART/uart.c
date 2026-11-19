#define MSG_LEN_STR 32      // 和 page.c 里的 MSG_LEN 保持一致
#include "mpu6050.h"
#include "uart.h"
#include "usart.h"
#include "key.h"    // 需要用到 key_event 和 mode
#include "my_adc.h"
#include <stdio.h>
#include "page.h"  
#include <string.h>

// 私有变量（加上 static，别的文件看不见）
static uint8_t rx_data;      // 接收上位机发来的字符
static char tx_buff[160];     // 发送缓冲区
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
    static char    line[MSG_LEN_STR];     // 见下面的定义
    static uint8_t len = 0;

    if (huart->Instance != USART1) return;

    if (rx_data == '\n' || rx_data == '\r')
    {
        // 一行的结束：长度 ≥ 2 就当消息存起来
        if (len >= 2)
        {
            line[len] = '\0';
            msg_push(line);
        }
        len = 0;
    }
    else
    {
        // 先攒进本行缓冲
        if (len < 31) line[len++] = rx_data;
        else { line[31] = '\0'; msg_push(line); len = 0; }   // 超长自动提交

        // 原来的单字符命令，保持立即生效
        uint8_t changed = 0;
        if (rx_data == '1')      { mode = 0; changed = 1; }
        else if (rx_data == '2') { mode = 1; changed = 1; }
        else if (rx_data == '3') { mode = 2; changed = 1; }
        else if (rx_data == '4') { mode = 3; changed = 1; }
        else if (rx_data == '5') { mode = 4; changed = 1; }
        else if (rx_data == 'p' || rx_data == 'P') page_request_next();

        if (changed) led_mode_reset();
    }

    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
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
    // 读一份按键事件副本（只读不清，清零由 led_mode_task 负责）
    if (key_event != 0) {
        uart_event = key_event;
    }

    static uint32_t last_uart_time = 0;
    if (HAL_GetTick() - last_uart_time >= 500)
    {
        last_uart_time = HAL_GetTick();

        float v = adc_get_voltage();

        char prefix[16] = "";

        // 这一拍如果有按键事件，就做成前缀和本拍数据一起发
        // 注意：不能连发两次 DMA —— 第二次会因为 busy 直接失败
        if (uart_event != 0)
        {
            sprintf(prefix, "[KEY%d] ", uart_event);
            uart_event = 0;
        }

        // 上位机显示的数据跟着当前页面走 —— 这就是 3.2 的"切换"
        switch (page_current())
        {
            case PAGE_ADC:
                sprintf(tx_buff, "%s[P1 ADC] V=%.2f M=%d\r\n",
                        prefix, v, mode);
                break;

            case PAGE_MPU:
                sprintf(tx_buff, "%s[P2 MPU] A=%.2f,%.2f,%.2f G=%.0f,%.0f,%.0f T=%.1f\r\n",
                        prefix,
                        g_mpu.ax_g, g_mpu.ay_g, g_mpu.az_g,
                        g_mpu.gx_dps, g_mpu.gy_dps, g_mpu.gz_dps, g_mpu.temp_c);
                break;

            case PAGE_LED:
                sprintf(tx_buff, "%s[P3 LED] mode=%d C=%d,%d,%d%%\r\n",
                        prefix, mode,
                        page_led_duty(1), page_led_duty(2), page_led_duty(3));
                break;

            case PAGE_MSG:
                sprintf(tx_buff, "%s[P4 MSG] %s\r\n", prefix, page_msg_status());
                break;

            default:
                sprintf(tx_buff, "%s[P?]\r\n", prefix);
                break;
        }

        // DMA 发送
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buff, strlen(tx_buff));
    }
}
