#include "L298N_Cutter.h"

/**
 * @brief  L298N锯片+传送带驱动初始化
 * @note   锯片:   IN1-PA1, IN2-PA5, ENA-PA9  (TIM1_CH2)
 *         传送带: IN3-PC0, IN4-PC1, ENB-PA10 (TIM1_CH3)
 */
void L298N_Cutter_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  TIM_OCInitTypeDef TIM_OCInitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC |
                             RCC_APB2Periph_TIM1,
                         ENABLE);

  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  GPIO_ResetBits(GPIOA, GPIO_Pin_1 | GPIO_Pin_5);
  GPIO_ResetBits(GPIOC, GPIO_Pin_0 | GPIO_Pin_1);

  TIM_TimeBaseStructure.TIM_Period = 999;
  TIM_TimeBaseStructure.TIM_Prescaler = 143;
  TIM_TimeBaseStructure.TIM_ClockDivision = 0;
  TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
  TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
  TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

  TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
  TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
  TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
  TIM_OCInitStructure.TIM_Pulse = 0;
  TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
  TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCPolarity_High;
  TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
  TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

  TIM_OC2Init(TIM1, &TIM_OCInitStructure);
  TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);

  TIM_OC3Init(TIM1, &TIM_OCInitStructure);
  TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

  TIM_CtrlPWMOutputs(TIM1, ENABLE);

  TIM_Cmd(TIM1, ENABLE);
}

/**
 * @brief  锯片电机控制
 * @param  direction: CUTTER_STOP/CUTTER_FORWARD/CUTTER_REVERSE
 * @param  speed: PWM占空比 0-100
 */
void SawMotor_Control(uint8_t direction, uint8_t speed) {
  if (speed > 100)
    speed = 100;
  TIM_SetCompare2(TIM1, speed * 10);

  switch (direction) {
  case CUTTER_FORWARD:
    GPIO_ResetBits(GPIOA, GPIO_Pin_1 | GPIO_Pin_5); // 先刹车
    GPIO_SetBits(GPIOA, GPIO_Pin_1);
    break;

  case CUTTER_REVERSE:
    GPIO_ResetBits(GPIOA, GPIO_Pin_1 | GPIO_Pin_5); // 先刹车
    GPIO_SetBits(GPIOA, GPIO_Pin_5);
    break;

  case CUTTER_STOP:
  default:
    TIM_SetCompare2(TIM1, 0);
    GPIO_ResetBits(GPIOA, GPIO_Pin_1);
    GPIO_ResetBits(GPIOA, GPIO_Pin_5);
    break;
  }
}

/**
 * @brief  传送带电机控制
 * @param  direction: CUTTER_STOP/CUTTER_FORWARD/CUTTER_REVERSE
 * @param  speed: PWM占空比 0-100
 */
void ConveyorMotor_Control(uint8_t direction, uint8_t speed) {
  if (speed > 100)
    speed = 100;
  TIM_SetCompare3(TIM1, speed * 10);

  switch (direction) {
  case CUTTER_FORWARD:
    GPIO_ResetBits(GPIOC, GPIO_Pin_0 | GPIO_Pin_1); // 先刹车
    GPIO_SetBits(GPIOC, GPIO_Pin_0);
    break;

  case CUTTER_REVERSE:
    GPIO_ResetBits(GPIOC, GPIO_Pin_0 | GPIO_Pin_1); // 先刹车
    GPIO_SetBits(GPIOC, GPIO_Pin_1);
    break;

  case CUTTER_STOP:
  default:
    TIM_SetCompare3(TIM1, 0);
    GPIO_ResetBits(GPIOC, GPIO_Pin_0);
    GPIO_ResetBits(GPIOC, GPIO_Pin_1);
    break;
  }
}

/**
 * @brief  启动锯片 (正转)
 * @param  speed: PWM占空比 0-100
 */
void SawMotor_Start(uint8_t speed) {
  SawMotor_Control(CUTTER_FORWARD, speed);
}

/**
 * @brief  停止锯片
 */
void SawMotor_Stop(void) {
  SawMotor_Control(CUTTER_STOP, 0);
}

/**
 * @brief  启动传送带 (正转)
 * @param  speed: PWM占空比 0-100
 */
void ConveyorMotor_Start(uint8_t speed) {
  ConveyorMotor_Control(CUTTER_FORWARD, speed);
}

/**
 * @brief  停止传送带
 */
void ConveyorMotor_Stop(void) {
  ConveyorMotor_Control(CUTTER_STOP, 0);
}
