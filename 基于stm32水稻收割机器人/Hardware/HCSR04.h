#ifndef __HCSR04_H
#define __HCSR04_H

#include "stm32f10x.h"

/**
 * ============================================================
 * HC-SR04 超声波传感器驱动
 * ============================================================
 * 左侧传感器: Trig - PC13, Echo - PC14
 * 右侧传感器: Trig - PB2,  Echo - PC14
 * ============================================================ */

#define HCSR04_LEFT 0
#define HCSR04_RIGHT 1

#define HCSR04_NO_OBJECT_THRESHOLD 10u

#define HCSR04_EDGE_CONFIRM_MS 1500u

#define HCSR04_ECHO_TIMEOUT_US 40000u

void HCSR04_Init(void);
uint16_t HCSR04_GetDistance(uint8_t side);

#endif
