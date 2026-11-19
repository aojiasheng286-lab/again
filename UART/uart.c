#define message_len 32     
#include "mpu6050.h"
#include "uart.h"
#include "usart.h"
#include "key.h"    
#include "my_adc.h"
#include <stdio.h>
#include "page.h"
#include "clock.h"  
#include <string.h>


static uint8_t rx_data;// 接收上位机发来的字符
static char tx_buff[160];// 发送缓冲区
static uint8_t uart_event = 0;// 串口专属按键事件缓存

















//启动回调函数
void uart_start_receive(void)
{
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}
















static uint8_t do_command(char c)
{
    if (c == '1')
    {
        mode = MODE_BREATH;
    }
    else if (c == '2')
    {
        mode = MODE_SLOW;
    }
    else if (c == '3')
    {
        mode = MODE_FAST;
    }
    else if (c == '4')
    {
        mode = MODE_FLOW;
    }
    else if (c == '5')
    {
        mode = MODE_OFF;
    }
    else if (c == 'p' || c == 'P')
    {
        page_request_next();
        return 1;
    }
    else
    {
        return 0;               // 不是命令字符
    }

    led_mode_reset();           // 灯效变了，复位一次
    return 1;
}




























//中断回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    static char    line[message_len];   // 正在收的这一行
    static uint8_t len = 0;             // 这一行已经收了多少个字符

    if (huart->Instance != USART1)
    {
        return;
    }









    //消息
    if (rx_data == '\n' || rx_data == '\r')
    {
        line[len] = '\0';               // 补上字符串的结尾

        if (len >= 2)                   // 2 个字符以上 -> 当成一条消息存起来
        {
            msg_push(line);
        }

        len = 0;                        // 清空，准备收下一行
    }
    




    

    

    //上面函数
    else
    {
        uint8_t is_command = 0;

        /* 如果这一行还是空的，先看看这个字符是不是命令 */
        if (len == 0)
        {
            is_command = do_command(rx_data);
        }

        /* 不是命令，就当消息内容攒进这一行 */
        if (is_command == 0)
        {
            if (len < message_len - 1)
            {
                line[len] = rx_data;
                len++;
            }
        }
    }

    HAL_UART_Receive_IT(&huart1, &rx_data, 1);   // 别忘了重新打开接收，被坑过一次了
}























//AI优化
// =======================================================
// 串口错误回调
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




















//串口定时发送任务 (在 while(1) 里调用)
void uart_process_task(void)
{
    // 读一份按键事件副本（只读不清，清零由 led_mode_task 负责）
    if (key_event != 0)
    {
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

            case PAGE_SW:
                sprintf(tx_buff, "%s[P5 SW] %d ms\r\n", prefix, (int)stopwatch_ms());
                break;

            case PAGE_CD:
                sprintf(tx_buff, "%s[P6 CD] %d ms\r\n", prefix, (int)countdown_ms());
                break;

            default:
                sprintf(tx_buff, "%s[P?]\r\n", prefix);
                break;
        }

        // DMA 发送
        HAL_UART_Transmit_DMA(&huart1, (uint8_t*)tx_buff, strlen(tx_buff));
    }
}
