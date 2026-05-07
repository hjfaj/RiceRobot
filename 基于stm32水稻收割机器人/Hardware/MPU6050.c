#include "MPU6050.h"
#include "Delay.h"
#include "math.h"

static uint8_t MPU6050_I2C_Addr = MPU6050_I2C_ADDR << 1;

static float roll = 0.0f;
static float pitch = 0.0f;
static uint8_t mpu_whoami = 0x00;

static int16_t accel_x_offset = 0;
static int16_t accel_yOffset = 0;
static int16_t accel_zOffset = 0;
static int16_t gyro_xOffset = 0;
static int16_t gyro_yOffset = 0;
static int16_t gyro_zOffset = 0;

#define MPU6050_SCL_PORT GPIOB
#define MPU6050_SCL_PIN  GPIO_Pin_12
#define MPU6050_SDA_PORT GPIOB
#define MPU6050_SDA_PIN  GPIO_Pin_13

#define MPU6050_SCL_HIGH() GPIO_SetBits(MPU6050_SCL_PORT, MPU6050_SCL_PIN)
#define MPU6050_SCL_LOW()  GPIO_ResetBits(MPU6050_SCL_PORT, MPU6050_SCL_PIN)
#define MPU6050_SDA_HIGH() GPIO_SetBits(MPU6050_SDA_PORT, MPU6050_SDA_PIN)
#define MPU6050_SDA_LOW()  GPIO_ResetBits(MPU6050_SDA_PORT, MPU6050_SDA_PIN)
#define MPU6050_SDA_READ() GPIO_ReadInputDataBit(MPU6050_SDA_PORT, MPU6050_SDA_PIN)

static void MPU6050_I2C_Delay(void) {
  Delay_us(10);
}

/* SDA 始终保持 Out_OD 模式，读取时先释放总线再读 IDR */

static void I2C_Start(void) {
  MPU6050_SDA_HIGH();
  MPU6050_SCL_HIGH();
  MPU6050_I2C_Delay();
  MPU6050_SDA_LOW();
  MPU6050_I2C_Delay();
  MPU6050_SCL_LOW();
}

static void I2C_Stop(void) {
  MPU6050_SDA_LOW();
  MPU6050_SCL_HIGH();
  MPU6050_I2C_Delay();
  MPU6050_SDA_HIGH();
  MPU6050_I2C_Delay();
}

static void I2C_SendByte(uint8_t dat) {
  uint8_t i;
  for (i = 0; i < 8; i++) {
    if (dat & 0x80) {
      MPU6050_SDA_HIGH();
    } else {
      MPU6050_SDA_LOW();
    }
    dat <<= 1;
    MPU6050_I2C_Delay();
    MPU6050_SCL_HIGH();
    MPU6050_I2C_Delay();
    MPU6050_SCL_LOW();
  }
  /* 释放 SDA 读 ACK（不检查，只发时钟） */
  MPU6050_SDA_HIGH();
  MPU6050_I2C_Delay();
  MPU6050_SCL_HIGH();
  MPU6050_I2C_Delay();
  MPU6050_SCL_LOW();
}

static uint8_t I2C_RecvByte(uint8_t ack) {
  uint8_t i;
  uint8_t dat = 0;
  /* 释放 SDA，让从机驱动 */
  MPU6050_SDA_HIGH();
  for (i = 0; i < 8; i++) {
    dat <<= 1;
    MPU6050_SCL_HIGH();
    MPU6050_I2C_Delay();
    if (MPU6050_SDA_READ()) {
      dat |= 0x01;
    }
    MPU6050_SCL_LOW();
    MPU6050_I2C_Delay();
  }
  /* 发送 ACK/NACK */
  if (ack) {
    MPU6050_SDA_LOW();
  } else {
    MPU6050_SDA_HIGH();
  }
  MPU6050_I2C_Delay();
  MPU6050_SCL_HIGH();
  MPU6050_I2C_Delay();
  MPU6050_SCL_LOW();
  MPU6050_SDA_HIGH();
  return dat;
}

static void MPU6050_WriteReg(uint8_t reg, uint8_t data) {
  I2C_Start();
  I2C_SendByte(MPU6050_I2C_Addr);
  I2C_SendByte(reg);
  I2C_SendByte(data);
  I2C_Stop();
}

static uint8_t MPU6050_ReadReg(uint8_t reg) {
  uint8_t data;
  I2C_Start();
  I2C_SendByte(MPU6050_I2C_Addr);
  I2C_SendByte(reg);
  I2C_Start();
  I2C_SendByte(MPU6050_I2C_Addr | 0x01);
  data = I2C_RecvByte(0);
  I2C_Stop();
  return data;
}

static void MPU6050_ReadBytes(uint8_t reg, uint8_t len, uint8_t *buf) {
  I2C_Start();
  I2C_SendByte(MPU6050_I2C_Addr);
  I2C_SendByte(reg);
  I2C_Start();
  I2C_SendByte(MPU6050_I2C_Addr | 0x01);
  while (len) {
    if (len == 1) {
      *buf = I2C_RecvByte(0);
    } else {
      *buf = I2C_RecvByte(1);
    }
    buf++;
    len--;
  }
  I2C_Stop();
}

static void I2C_BusReset(void) {
  uint8_t i;
  MPU6050_SDA_HIGH();
  MPU6050_I2C_Delay();
  for (i = 0; i < 9; i++) {
    MPU6050_SCL_HIGH();
    MPU6050_I2C_Delay();
    MPU6050_SCL_LOW();
    MPU6050_I2C_Delay();
  }
  I2C_Start();
  I2C_Stop();
}

static void SoftI2C_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

  GPIO_InitStructure.GPIO_Pin = MPU6050_SCL_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(MPU6050_SCL_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = MPU6050_SDA_PIN;
  GPIO_Init(MPU6050_SDA_PORT, &GPIO_InitStructure);

  MPU6050_SCL_HIGH();
  MPU6050_SDA_HIGH();
}

MPU6050_Status MPU6050_Init(void) {
  uint8_t who_am_i;
  uint8_t retry;

  SoftI2C_Init();

  Delay_ms(50);
  I2C_BusReset();
  Delay_ms(50);

  for (retry = 0; retry < 5; retry++) {
    who_am_i = MPU6050_ReadReg(MPU6050_WHO_AM_I);
    if (who_am_i == 0x68 || who_am_i == 0x70) {
      break;
    }
    Delay_ms(20);
  }
  mpu_whoami = who_am_i;
  if (who_am_i != 0x68 && who_am_i != 0x70) {
    return MPU6050_NOT_FOUND;
  }

  MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x00);
  Delay_ms(10);

  MPU6050_WriteReg(MPU6050_SMPLRT_DIV, 0x07);
  Delay_ms(10);

  MPU6050_WriteReg(MPU6050_CONFIG, 0x00);
  Delay_ms(10);

  MPU6050_WriteReg(MPU6050_GYRO_CONFIG, MPU6050_GYRO_FS_2000);
  Delay_ms(10);

  MPU6050_WriteReg(MPU6050_ACCEL_CONFIG, MPU6050_ACCEL_FS_4);
  Delay_ms(10);

  MPU6050_WriteReg(MPU6050_PWR_MGMT_1, 0x01);
  Delay_ms(50);

  /* 陀螺仪零偏校准：静置时采样 200 次取平均 */
  {
    int32_t sum_gx = 0, sum_gy = 0, sum_gz = 0;
    uint8_t n;
    for (n = 0; n < 200; n++) {
      MPU6050_RawData cal;
      MPU6050_ReadRawData(&cal);
      sum_gx += cal.Gyro_X;
      sum_gy += cal.Gyro_Y;
      sum_gz += cal.Gyro_Z;
      Delay_ms(2);
    }
    gyro_xOffset = (int16_t)(sum_gx / 200);
    gyro_yOffset = (int16_t)(sum_gy / 200);
    gyro_zOffset = (int16_t)(sum_gz / 200);
  }

  return MPU6050_OK;
}

void MPU6050_ReadRawData(MPU6050_RawData *data) {
  uint8_t buf[14];
  MPU6050_ReadBytes(MPU6050_ACCEL_XOUT_H, 14, buf);

  data->Accel_X = (int16_t)(buf[0] << 8 | buf[1]);
  data->Accel_Y = (int16_t)(buf[2] << 8 | buf[3]);
  data->Accel_Z = (int16_t)(buf[4] << 8 | buf[5]);
  data->Gyro_X = (int16_t)(buf[8] << 8 | buf[9]);
  data->Gyro_Y = (int16_t)(buf[10] << 8 | buf[11]);
  data->Gyro_Z = (int16_t)(buf[12] << 8 | buf[13]);
}

void MPU6050_GetAngle(MPU6050_Angle *angle) {
  MPU6050_RawData raw;
  float accel_roll, accel_pitch;
  float gyro_rate_x, gyro_rate_y;
  float dt;
  static uint32_t last_time = 0;
  static uint8_t  first_run = 1;
  uint32_t now;

  MPU6050_ReadRawData(&raw);

  /* 加速度计直接算角度 */
  accel_roll  = atan2f((float)(raw.Accel_Y - accel_yOffset),
                       (float)(raw.Accel_Z - accel_zOffset)) * 57.29578f;
  accel_pitch = atan2f((float)(-(raw.Accel_X - accel_x_offset)),
                        sqrtf((float)((raw.Accel_Y - accel_yOffset) *
                                      (raw.Accel_Y - accel_yOffset) +
                                      (raw.Accel_Z - accel_zOffset) *
                                      (raw.Accel_Z - accel_zOffset)))) *
                57.29578f;

  /* 首次运行直接从加速度计初始化，无收敛延迟 */
  if (first_run) {
    roll = accel_roll;
    pitch = accel_pitch;
    first_run = 0;
    last_time = GetTick();
    angle->Roll  = roll;
    angle->Pitch = pitch;
    angle->Yaw   = 0;
    return;
  }

  /* 陀螺仪角速度 (°/s)，2000dps 量程: 16.4 LSB/°/s */
  gyro_rate_x = (float)(raw.Gyro_X - gyro_xOffset) / 16.4f;
  gyro_rate_y = (float)(raw.Gyro_Y - gyro_yOffset) / 16.4f;

  /* dt 计算 */
  now = GetTick();
  dt = (float)(now - last_time) / 1000.0f;
  if (dt <= 0.0f || dt > 0.2f) {
    dt = 0.01f;
  }
  last_time = now;

  /* 互补滤波：加速计权重 10%，响应更快 */
  roll  = 0.90f * (roll + gyro_rate_x * dt) + 0.10f * accel_roll;
  pitch = 0.90f * (pitch + gyro_rate_y * dt) + 0.10f * accel_pitch;

  angle->Roll  = roll;
  angle->Pitch = pitch;
  angle->Yaw   = 0;
}

Stability_State MPU6050_GetStabilityState(void) {
  float abs_roll = fabs(roll);
  float abs_pitch = fabs(pitch);

  if (abs_roll >= MPU6050_TILT_THRESHOLD_2 || abs_pitch >= MPU6050_TILT_THRESHOLD_2) {
    return STABILITY_DANGER;
  }

  if (abs_roll >= MPU6050_TILT_THRESHOLD_1 || abs_pitch >= MPU6050_TILT_THRESHOLD_1) {
    return STABILITY_WARNING;
  }

  return STABILITY_NORMAL;
}

uint8_t MPU6050_IsOnline(void) {
  uint8_t who_am_i = MPU6050_ReadReg(MPU6050_WHO_AM_I);
  return (who_am_i == 0x68 || who_am_i == 0x70) ? 1 : 0;
}

float MPU6050_GetRoll(void) {
  return roll;
}

float MPU6050_GetPitch(void) {
  return pitch;
}

uint8_t MPU6050_GetWhoAmI(void) {
  return mpu_whoami;
}
