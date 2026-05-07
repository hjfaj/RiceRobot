#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

void Key_Init(void);

// 按键事件标志：ISR 置 1，主循环处理并清零
extern volatile uint8_t key1_event;
extern volatile uint8_t key2_event;

#endif
