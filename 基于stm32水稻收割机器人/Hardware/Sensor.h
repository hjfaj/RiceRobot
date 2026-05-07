#ifndef __SENSOR_H
#define __SENSOR_H

#include "stm32f10x.h"

/**
 * ============================================================
 * 传感器引脚配置
 * ============================================================
 * 限位开关       : PB7 (GPIO输入, 低电平触发)
 * TCRT5000红外反射传感器 : PC15 (GPIO输入, 低电平触发；安装于粮仓顶部，朝下检测谷物堆积高度)
 * ============================================================ */

#define LIMIT_SWITCH_TRIGGERED 0
#define LIMIT_SWITCH_RELEASED 1

#define IR_SENSOR_PORT GPIOC
#define IR_SENSOR_PIN GPIO_Pin_15
#define IR_TRIGGERED 0

#define STORAGE_WEIGHT_THRESHOLD 5.0f

void Sensor_Init(void);
uint8_t Sensor_GetLimitSwitch(void);
uint8_t Sensor_GetIR(void);

#endif
