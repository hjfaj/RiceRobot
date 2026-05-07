#ifndef __ESP8266_H
#define __ESP8266_H

#include "stm32f10x.h"
#include <string.h>

// ===================== 帧协议常量 =====================
#define FRAME_HEADER_1 0xAA // 帧头第1字节
#define FRAME_HEADER_2 0xBB // 帧头第2字节
#define FRAME_TAIL_1 0xCC   // 帧尾第1字节
#define FRAME_TAIL_2 0xDD   // 帧尾第2字节

// ===================== 串口3接收缓冲 =====================
#define USART3_RX_BUF_SIZE 256 // 接收缓冲区大小（用于接收服务器下发指令）

extern volatile uint8_t USART3_RX_BUF[USART3_RX_BUF_SIZE];
extern volatile uint16_t USART3_RX_Count; // 已接收字节数
extern volatile uint8_t USART3_RX_Flag;   // 接收完成标志

// ===================== 函数声明 =====================

// 初始化 USART3 硬件（用于与 ESP8266 Arduino 固件通信）
void ESP8266_Init(void);

// 通过 USART3 发送单字节
void ESP8266_SendByte(uint8_t byte);

// 通过 USART3 发送字符串
void ESP8266_SendString(const char *str);

// 封装并发送一帧数据：帧头 + 长度 + JSON + 帧尾
void ESP8266_SendFrame(const char *json);

// 获取服务器下发的数据（如果有），返回已接收字节数，0 表示无数据
uint16_t ESP8266_GetReceivedData(uint8_t *buf, uint16_t maxLen);

#endif
