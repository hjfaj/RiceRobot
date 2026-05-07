#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f10x.h"

/**
 * ============================================================
 * MPU6050 六轴姿态传感器驱动
 * ============================================================
 * 通信接口: 软件I2C (SCL-PB12, SDA-PB13)
 * I2C地址: 0x68 (AD0接低电平)
 * 功能: 三轴加速度计 + 三轴陀螺仪
 * ============================================================
 */

#define MPU6050_I2C_ADDR 0x68

#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_PWR_MGMT_2   0x6C
#define MPU6050_SMPLRT_DIV   0x19
#define MPU6050_CONFIG       0x1A
#define MPU6050_GYRO_CONFIG  0x1B
#define MPU6050_ACCEL_CONFIG 0x1C
#define MPU6050_INT_ENABLE   0x38
#define MPU6050_ACCEL_XOUT_H 0x3B
#define MPU6050_ACCEL_XOUT_L 0x3C
#define MPU6050_ACCEL_YOUT_H 0x3D
#define MPU6050_ACCEL_YOUT_L 0x3E
#define MPU6050_ACCEL_ZOUT_H 0x3F
#define MPU6050_ACCEL_ZOUT_L 0x40
#define MPU6050_GYRO_XOUT_H  0x43
#define MPU6050_GYRO_XOUT_L  0x44
#define MPU6050_GYRO_YOUT_H  0x45
#define MPU6050_GYRO_YOUT_L  0x46
#define MPU6050_GYRO_ZOUT_H  0x47
#define MPU6050_GYRO_ZOUT_L  0x48
#define MPU6050_WHO_AM_I     0x75

#define MPU6050_GYRO_FS_250  0x00
#define MPU6050_GYRO_FS_500  0x08
#define MPU6050_GYRO_FS_1000 0x10
#define MPU6050_GYRO_FS_2000 0x18

#define MPU6050_ACCEL_FS_2   0x00
#define MPU6050_ACCEL_FS_4   0x08
#define MPU6050_ACCEL_FS_8   0x10
#define MPU6050_ACCEL_FS_16  0x18

#define MPU6050_TILT_THRESHOLD_1   15.0f
#define MPU6050_TILT_COMPENSATION   15

#define MPU6050_TILT_THRESHOLD_2   35.0f
#define MPU6050_TILT_DANGER        40.0f

#define MPU6050_KALMAN_Q    0.001f
#define MPU6050_KALMAN_R    0.1f

typedef struct {
  int16_t Accel_X;
  int16_t Accel_Y;
  int16_t Accel_Z;
  int16_t Gyro_X;
  int16_t Gyro_Y;
  int16_t Gyro_Z;
} MPU6050_RawData;

typedef struct {
  float Roll;
  float Pitch;
  float Yaw;
} MPU6050_Angle;

typedef struct {
  float Q;
  float R;
  float P;
  float K;
  float x;
  float meas;
} KalmanFilter;

typedef enum {
  MPU6050_OK = 0,
  MPU6050_ERROR = 1,
  MPU6050_NOT_FOUND = 2
} MPU6050_Status;

typedef enum {
  STABILITY_NORMAL = 0,
  STABILITY_WARNING = 1,
  STABILITY_DANGER = 2
} Stability_State;

MPU6050_Status MPU6050_Init(void);
void MPU6050_ReadRawData(MPU6050_RawData *data);
void MPU6050_GetAngle(MPU6050_Angle *angle);
Stability_State MPU6050_GetStabilityState(void);
uint8_t MPU6050_IsOnline(void);
float MPU6050_GetRoll(void);
float MPU6050_GetPitch(void);
uint8_t MPU6050_GetWhoAmI(void);

#endif
