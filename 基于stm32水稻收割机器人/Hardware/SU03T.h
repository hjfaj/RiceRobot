#ifndef __SU03T_H
#define __SU03T_H

#include "stm32f10x.h" // Device header

/* SU-03T 串口通信定义 */
#define SU03T_USART USART2
#define SU03T_USART_CLK RCC_APB1Periph_USART2
#define SU03T_GPIO_TX_PORT GPIOA
#define SU03T_GPIO_TX_PIN GPIO_Pin_2
#define SU03T_GPIO_RX_PORT GPIOA
#define SU03T_GPIO_RX_PIN GPIO_Pin_3
#define SU03T_GPIO_CLK RCC_APB2Periph_GPIOA
#define SU03T_BAUDRATE 115200

/* 语音命令定义 */
#define SU03T_CMD_NONE 0xFE           // 无新命令
#define SU03T_CMD_WAKEUP 0x00         // 唤醒时
#define SU03T_CMD_MOVE_FORWARD 0x01   // 向前开
#define SU03T_CMD_MOVE_BACKWARD 0x02  // 向后倒
#define SU03T_CMD_TURN_LEFT 0x03      // 向左转
#define SU03T_CMD_TURN_RIGHT 0x04     // 向右转
#define SU03T_CMD_STOP_MOVE 0x05      // 停车
#define SU03T_CMD_START_CUT 0x11      // 开始收割
#define SU03T_CMD_STOP_CUT 0x12       // 停止收割
#define SU03T_CMD_START_THRESH 0x13   // 开始脱粒
#define SU03T_CMD_REPORT_STATUS 0x22  // 汇报状态
#define SU03T_CMD_EMERPENCY_STOP 0x23 // 紧急停止
#define SU03T_CMD_ALL_START 0xFF      // 开启全自动无人收割|执行一键收割

/* 函数声明 */
void SU03T_Init(void);
uint8_t SU03T_GetCommand(void);
void SU03T_ClearCommand(void);
void SU03T_SendData(uint8_t data);
void SU03T_PlayVoice(uint8_t voice_cmd);

#endif
