#include "stm32f10x.h" // Device header

#define KEY1 GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1)
#define KEY2 GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_0)

// 按键事件标志（ISR 置位，主循环查询并清除）
volatile uint8_t key1_event = 0;
volatile uint8_t key2_event = 0;

void Key_Init(void) {
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

  GPIO_InitTypeDef GPIO_InitStructure;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_0;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  // 配置中断线
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource1);
  GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource0);

  EXTI_InitTypeDef EXTI_InitStructure;
  EXTI_InitStructure.EXTI_Line = EXTI_Line1 | EXTI_Line0;
  EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt; // 中断模式
  EXTI_InitStructure.EXTI_Trigger =
      EXTI_Trigger_Falling; // 下降沿触发（按下时）
  EXTI_InitStructure.EXTI_LineCmd = ENABLE;
  EXTI_Init(&EXTI_InitStructure);

  NVIC_InitTypeDef NVIC_InitStructure;
  // 配置中断通道 1 (对应 PB1)
  NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  // 配置中断通道 0 (对应 PB0)
  NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
}

// PB1 中断服务函数 —— 只置标志位，不做耗时操作
void EXTI1_IRQHandler(void) {
  if (EXTI_GetITStatus(EXTI_Line1) != RESET) {
    key1_event = 1; // 通知主循环处理
    EXTI_ClearITPendingBit(EXTI_Line1);
  }
}

// PB0 中断服务函数 —— 只置标志位，不做耗时操作
void EXTI0_IRQHandler(void) {
  if (EXTI_GetITStatus(EXTI_Line0) != RESET) {
    key2_event = 1; // 通知主循环处理
    EXTI_ClearITPendingBit(EXTI_Line0);
  }
}
