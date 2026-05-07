#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

void Buzzer_Init(void);
void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint8_t times, uint16_t on_ms, uint16_t off_ms);
void Buzzer_AlarmTick(uint8_t level);

/* 音量 0-100，默认 25 */
void Buzzer_SetVolume(uint8_t vol);

#endif
