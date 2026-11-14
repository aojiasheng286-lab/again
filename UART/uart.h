#ifndef __UART_H
#define __UART_H

#include "main.h"

// 声明对外提供的函数
void uart_start_receive(void); // 启动串口中断接收
void uart_process_task(void);  // 串口任务（放到 while(1) 里调用）

#endif