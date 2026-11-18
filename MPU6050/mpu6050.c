#include "mpu6050.h"
#include "i2c.h"        // 为了拿到 hi2c1

// 7 位地址 0x68（AD0 接地），HAL 要左移一位后的 8 位地址
#define MPU6050_ADDR             0xD0

#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_WHO_AM_I     0x75

MPU6050_Data_t g_mpu;

// ---------------- 底层：读写单个寄存器 ----------------

static uint8_t MPU_ReadReg(uint8_t reg, uint8_t *val)
{
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, reg,
                         I2C_MEMADD_SIZE_8BIT, val, 1, 100) != HAL_OK)
    {
        return 0;
    }
    return 1;
}

static uint8_t MPU_WriteReg(uint8_t reg, uint8_t val)
{
    if (HAL_I2C_Mem_Write(&hi2c1, MPU6050_ADDR, reg,
                          I2C_MEMADD_SIZE_8BIT, &val, 1, 100) != HAL_OK)
    {
        return 0;
    }
    return 1;
}

// ---------------- 初始化 ----------------

uint8_t MPU6050_Init(void)
{
    uint8_t id  = 0;
    uint8_t pwr = 0xFF;

    // 1) 确认芯片在线
    if (HAL_I2C_IsDeviceReady(&hi2c1, MPU6050_ADDR, 3, 100) != HAL_OK) return 0;

    // 2) 确认身份
    //    0x68 = 原厂 MPU6050
    //    0x70 = MPU6500、0x71 = MPU9250、0x98 = 部分国产替代
    //    这几种的寄存器地址和灵敏度系数与 6050 一致，可以照用
    if (!MPU_ReadReg(MPU6050_REG_WHO_AM_I, &id)) return 0;
    if (id != 0x68 && id != 0x70 && id != 0x71 && id != 0x98) return 0;

    // 3) 软复位，等它自己起来
    MPU_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x80);
    HAL_Delay(100);

    // 4) 唤醒：清 SLEEP 位，时钟源用 X 轴陀螺 PLL
    MPU_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x01);
    HAL_Delay(10);

    // 5) 采样率 100Hz：DLPF 开启后陀螺输出 1000Hz，1000/(1+9) = 100Hz
    MPU_WriteReg(MPU6050_REG_CONFIG, 0x03);        // DLPF 带宽约 44Hz
    MPU_WriteReg(MPU6050_REG_SMPLRT_DIV, 9);

    // 6) 量程：加速度 ±2g、陀螺 ±250°/s
    //    这两个值必须和下面 ReadAll 里的换算系数成对修改
    MPU_WriteReg(MPU6050_REG_ACCEL_CONFIG, 0x00);
    MPU_WriteReg(MPU6050_REG_GYRO_CONFIG,  0x00);

    // 7) 回读确认真的醒了（不做这步，忘了唤醒会表现为"数据全 0"）
    if (!MPU_ReadReg(MPU6050_REG_PWR_MGMT_1, &pwr)) return 0;
    if (pwr & 0x40) return 0;      // SLEEP 位还在，说明没叫醒

    return 1;
}

// ---------------- 读一次全量数据 ----------------

uint8_t MPU6050_ReadAll(MPU6050_Data_t *out)
{
    uint8_t buf[14];

    // 从 0x3B 一次读 14 字节：加速度6 + 温度2 + 角速度6
    // 一次读完才能保证这 14 个字节是同一时刻的采样
    if (HAL_I2C_Mem_Read(&hi2c1, MPU6050_ADDR, MPU6050_REG_ACCEL_XOUT_H,
                         I2C_MEMADD_SIZE_8BIT, buf, 14, 100) != HAL_OK)
    {
        out->ok = 0;
        return 0;
    }

    // 大端：高字节在前
    out->ax   = (int16_t)((buf[0]  << 8) | buf[1]);
    out->ay   = (int16_t)((buf[2]  << 8) | buf[3]);
    out->az   = (int16_t)((buf[4]  << 8) | buf[5]);
    out->temp = (int16_t)((buf[6]  << 8) | buf[7]);
    out->gx   = (int16_t)((buf[8]  << 8) | buf[9]);
    out->gy   = (int16_t)((buf[10] << 8) | buf[11]);
    out->gz   = (int16_t)((buf[12] << 8) | buf[13]);

    // 换算系数必须和第 6 步设的量程一致
    out->ax_g   = out->ax / 16384.0f;   // ±2g    → 16384 LSB/g
    out->ay_g   = out->ay / 16384.0f;
    out->az_g   = out->az / 16384.0f;
    out->gx_dps = out->gx / 131.0f;     // ±250°/s → 131 LSB/(°/s)
    out->gy_dps = out->gy / 131.0f;
    out->gz_dps = out->gz / 131.0f;

    // 你的片子是 0x70（MPU6500 系），用 6500 的温度公式
    // 若读数偏低约 15°C，说明其实是 6050 die，换成 temp/340.0f + 36.53f
    out->temp_c = out->temp / 333.87f + 21.0f;

    out->ok = 1;
    return 1;
}