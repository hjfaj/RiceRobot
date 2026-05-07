#include "HCSR04.h"
#include "Delay.h"

/**
 * @brief  HC-SR04 超声波传感器初始化
 * @note   引脚分配:
 *           左侧: Trig-PC13 (输出), Echo-PC14 (输入)
 *           右侧: Trig-PB2  (输出), Echo-PC14 (输入)
 */
void HCSR04_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOB, ENABLE);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
  GPIO_Init(GPIOC, &GPIO_InitStructure);

  GPIO_ResetBits(GPIOC, GPIO_Pin_13);
  GPIO_ResetBits(GPIOB, GPIO_Pin_2);
}

/**
 * @brief  获取指定侧超声波传感器的距离
 * @param  side: HCSR04_LEFT 或 HCSR04_RIGHT
 * @retval 距离值 (cm)；若超时（无回波）返回 0
 */
uint16_t HCSR04_GetDistance(uint8_t side) {
  uint16_t trig_pin;
  uint16_t echo_pin;
  uint32_t timeout;
  uint32_t echo_us = 0;

  GPIO_TypeDef* trig_gpio;
  if (side == HCSR04_LEFT) {
    trig_pin = GPIO_Pin_13;
    trig_gpio = GPIOC;
  } else {
    trig_pin = GPIO_Pin_2;
    trig_gpio = GPIOB;
  }
  echo_pin = GPIO_Pin_14;

  GPIO_SetBits(trig_gpio, trig_pin);
  Delay_us(10);
  GPIO_ResetBits(trig_gpio, trig_pin);

  timeout = HCSR04_ECHO_TIMEOUT_US;
  while (GPIO_ReadInputDataBit(GPIOC, echo_pin) == 0) {
    if (timeout-- == 0) {
      return 0;
    }
    Delay_us(1);
  }

  timeout = HCSR04_ECHO_TIMEOUT_US;
  while (GPIO_ReadInputDataBit(GPIOC, echo_pin) == 1) {
    if (timeout-- == 0) {
      return 0;
    }
    Delay_us(1);
    echo_us++;
  }

  return (uint16_t)(echo_us / 58u);
}
