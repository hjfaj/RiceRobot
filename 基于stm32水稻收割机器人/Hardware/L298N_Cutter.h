#ifndef __L298N_CUTTER_H
#define __L298N_CUTTER_H

#include "stm32f10x.h"

/**
 * ============================================================
 * L298N 锯片+传送带驱动
 * ============================================================
 * 锯片电机 (Saw Motor):
 *   IN1 - PA1  (方向控制)
 *   IN2 - PA5  (方向控制)
 *   ENA - PA9  (TIM1_CH2 PWM调速)
 *
 * 传送带电机 (Conveyor Motor):
 *   IN3 - PC0  (方向控制)
 *   IN4 - PC1  (方向控制)
 *   ENB - PA10 (TIM1_CH3 PWM调速)
 * ============================================================
 */

#define CUTTER_STOP     0
#define CUTTER_FORWARD  1
#define CUTTER_REVERSE  2

#define SAW_DEFAULT_SPEED    80
#define CONVEYOR_DEFAULT_SPEED 50

void L298N_Cutter_Init(void);
void SawMotor_Control(uint8_t direction, uint8_t speed);
void ConveyorMotor_Control(uint8_t direction, uint8_t speed);
void SawMotor_Start(uint8_t speed);
void SawMotor_Stop(void);
void ConveyorMotor_Start(uint8_t speed);
void ConveyorMotor_Stop(void);

#endif
