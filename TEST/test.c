#include "test.h"
#include "i2c.h"      // hi2c1
#include "usart.h"    // huart1
#include <stdio.h>
#include <string.h>

void test_mpu_id(void)
{
    uint8_t id = 0;
    char msg[48];

    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c1, 0xD0, 0x75,
                                            I2C_MEMADD_SIZE_8BIT, &id, 1, 100);

    sprintf(msg, "st=%d id=0x%02X\r\n", (int)st, id);

    // 用阻塞发送：这是初始化阶段，主循环还没跑起来，不存在和 DMA 抢的问题
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
}