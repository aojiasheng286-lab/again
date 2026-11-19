#ifndef __TEST_H
#define __TEST_H

#include "main.h"

// 上电自检：把 MPU6050 的 WHO_AM_I 打到串口，一行
// 只在初始化阶段调用一次，不要放进 while(1)
void test_mpu_id(void);

#endif