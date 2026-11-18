#ifndef __MPU6050_H
#define __MPU6050_H

#include "main.h"

typedef struct {
    int16_t ax, ay, az;              // 加速度原始值
    int16_t gx, gy, gz;              // 角速度原始值
    int16_t temp;                    // 温度原始值
    float   ax_g, ay_g, az_g;        // 换算成 g
    float   gx_dps, gy_dps, gz_dps;  // 换算成 °/s
    float   temp_c;                  // 换算成 °C
    uint8_t ok;                      // 1 = 本次读取成功
} MPU6050_Data_t;

extern MPU6050_Data_t g_mpu;         // 全局数据：main.c 写、uart.c 读

uint8_t MPU6050_Init(void);                     // 返回 1 成功，0 失败
uint8_t MPU6050_ReadAll(MPU6050_Data_t *out);   // 返回 1 成功

#endif