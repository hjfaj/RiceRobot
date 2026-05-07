#include "Sensor.h"

/**
 * @brief  初始化限位开关和TCRT5000红外反射传感器GPIO
 *         限位开关: PB7 (上拉输入)
 *         TCRT5000: PC15 (上拉输入, DO接PC15, VCC接3.3V, GND接GND)
 */
static void LimitSwitch_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;

  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);

  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin = IR_SENSOR_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
  GPIO_Init(IR_SENSOR_PORT, &GPIO_InitStructure);
}

void Sensor_Init(void) {
  LimitSwitch_Init();
}

uint8_t Sensor_GetLimitSwitch(void) {
  if (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7) == LIMIT_SWITCH_TRIGGERED) {
    return 1;
  }
  return 0;
}

uint8_t Sensor_GetIR(void) {
  if (GPIO_ReadInputDataBit(IR_SENSOR_PORT, IR_SENSOR_PIN) == IR_TRIGGERED) {
    return 1;
  }
  return 0;
}
