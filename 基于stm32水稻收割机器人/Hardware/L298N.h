
#ifndef __L298N_H
#define __L298N_H

#include "stm32f10x.h"

// 电机方向定义
#define FORWARD 1  // 前进
#define BACKWARD 2 // 后退
#define STOP 0     // 停止

// TT减速电机速度参数 (0-100, 对应PWM占空比)
// 注意：TT电机通常为3-6V供电。若系统为12V供电，请务必调低占空比！
// 12V输入 → L298N压降约2V → 最大输出约10V
// 输出电压 = 占空比 × 10V，例如：50%占空比 ≈ 5V输出
#define MOTOR_DEFAULT_SPEED 30 // 默认行驶速度 (30% ≈ 3V输出)
#define MOTOR_TURN_SPEED 20    // 转弯时内侧轮速度

extern volatile uint8_t g_motor_speed;

// 机器人运动方向定义
#define GO_FORWARD 1  // 前进
#define GO_BACKWARD 2 // 后退
#define TURN_LEFT 3   // 左转
#define TURN_RIGHT 4  // 右转
#define TURN_180 5    // 原地180°转弯（用于自动收割换行）
#define ROBOT_STOP 0  // 停止

// 函数声明
void L298N_Init(void);
void MotorA_Control(uint8_t direction, uint8_t speed);
void MotorB_Control(uint8_t direction, uint8_t speed);
void Robot_Move(uint8_t direction);
void Robot_SetSpeed(uint8_t speed);

#endif
