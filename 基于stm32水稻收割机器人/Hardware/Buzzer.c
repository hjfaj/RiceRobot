#include "Buzzer.h"
#include "Delay.h"

#define BUZZER_PORT  GPIOB
#define BUZZER_PIN   GPIO_Pin_15

/* ---- PWM 控制 ---- */
static volatile uint8_t buzzer_enabled = 0; /* ISR共享 */
static volatile uint8_t buzzer_duty    = 25; /* ISR共享，默认 25% */

/* ---- Beep 序列状态机 ---- */
static uint8_t  b_times  = 0;
static uint16_t b_on     = 0;
static uint16_t b_off    = 0;
static uint8_t  b_count  = 0;
static uint8_t  b_state  = 0;
static uint32_t b_tick   = 0;

/* ---- 持续报警状态机 ---- */
static uint8_t  a_level  = 0;
static uint8_t  a_state  = 0;
static uint32_t a_tick   = 0;
static uint16_t a_on_ms  = 0;
static uint16_t a_off_ms = 0;

/* ---- TIM4 PWM 初始化 (10kHz, 100 级占空比) ---- */
static void TIM4_PWM_Init(void) {
  TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

  TIM_TimeBaseStructure.TIM_Period        = 99;   /* 0~99 = 100 级 */
  TIM_TimeBaseStructure.TIM_Prescaler     = 71;   /* 72MHz / 72 = 1MHz */
  TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
  TIM_TimeBaseStructure.TIM_CounterMode   = TIM_CounterMode_Up;
  TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

  TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

  NVIC_InitStructure.NVIC_IRQChannel    = TIM4_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority       = 3;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  TIM_Cmd(TIM4, ENABLE);
}

void Buzzer_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

  GPIO_InitStructure.GPIO_Pin   = BUZZER_PIN;
  GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);

  GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);

  TIM4_PWM_Init();
}

void Buzzer_SetVolume(uint8_t vol) {
  if (vol > 100) vol = 100;
  buzzer_duty = vol;
}

/* ---- TIM4 ISR：软件 PWM ---- */
void TIM4_IRQHandler(void) {
  static uint8_t pwm_cnt = 0;
  if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET) {
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    pwm_cnt++;
    if (pwm_cnt >= 100) pwm_cnt = 0;
    if (buzzer_enabled && pwm_cnt < buzzer_duty) {
      GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
    } else {
      GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
    }
  }
}

/* ---- 逻辑开关（控制 PWM 使能） ---- */
void Buzzer_On(void) {
  buzzer_enabled = 1;
}

void Buzzer_Off(void) {
  buzzer_enabled = 0;
}

void Buzzer_Beep(uint8_t times, uint16_t on_ms, uint16_t off_ms) {
  if (times == 0) return;
  b_times = times;
  b_on    = on_ms;
  b_off   = off_ms;
  b_count = 0;
  b_state = 1;
  Buzzer_On();
  b_tick  = GetTick();
}

void Buzzer_AlarmTick(uint8_t level) {
  uint32_t now = GetTick();

  if (level != a_level) {
    a_level = level;
    if (level == 0) {
      Buzzer_Off();
      a_state = 0;
      return;
    }
    if (level == 1) {
      a_on_ms  = 500;
      a_off_ms = 500;
    } else {
      a_on_ms  = 150;
      a_off_ms = 150;
    }
    a_state = 1;
    Buzzer_On();
    a_tick  = now;
    return;
  }

  if (a_level == 0) {
    if (b_times != 0) {
      /* 正在执行 Beep 序列 */
      if (b_state == 1 && now - b_tick >= b_on) {
        b_count++;
        if (b_count >= b_times) {
          Buzzer_Off();
          b_times = 0;
          b_state = 0;
        } else {
          Buzzer_Off();
          b_state = 2;
          b_tick  = now;
        }
      } else if (b_state == 2 && now - b_tick >= b_off) {
        Buzzer_On();
        b_state = 1;
        b_tick  = now;
      }
    }
    /* 没有报警也没有 Beep 序列时，强制关蜂鸣器 */
    if (b_times == 0) {
      Buzzer_Off();
    }
    return;
  }

  if (a_state == 1 && now - a_tick >= a_on_ms) {
    Buzzer_Off();
    a_state = 2;
    a_tick  = now;
  } else if (a_state == 2 && now - a_tick >= a_off_ms) {
    Buzzer_On();
    a_state = 1;
    a_tick  = now;
  }
}
