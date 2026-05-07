#ifndef __WINDOW_H
#define __WINDOW_H

#include "OLED.h"
#include "stm32f10x.h"
#include <stdint.h>

void Window_Show1(void);
void Window_Show2(void);
void Window_Show3(void);
void Window_Show4(void);
void Window_Show(uint8_t window_num);

#endif // __WINDOW_H
