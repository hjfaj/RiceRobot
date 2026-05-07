#ifndef __HX711_H
#define __HX711_H

#include "stm32f10x.h" // Device header

// 引脚定义配置
#define HX711_GPIO_PORT GPIOB
#define HX711_GPIO_CLK RCC_APB2Periph_GPIOB
#define HX711_SCK_PIN GPIO_Pin_5
#define HX711_DOUT_PIN GPIO_Pin_6

// GPIO 操作宏
#define HX711_SCK_HIGH() GPIO_SetBits(HX711_GPIO_PORT, HX711_SCK_PIN)
#define HX711_SCK_LOW() GPIO_ResetBits(HX711_GPIO_PORT, HX711_SCK_PIN)
#define HX711_DOUT_READ() GPIO_ReadInputDataBit(HX711_GPIO_PORT, HX711_DOUT_PIN)

// 函数声明
void HX711_Init(void);
uint32_t HX711_Read(void);
long HX711_Get_Average(uint8_t times);
void HX711_Tare(void);
float HX711_Get_Weight(void);

// 校准参数声明
extern long HX711_Offset;    // 去皮偏移量
extern float HX711_GapValue; // 比例系数 (需要根据实际力学结构调整)

#endif // __HX711_H
