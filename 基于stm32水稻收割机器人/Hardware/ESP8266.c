#include "ESP8266.h"
#include "Delay.h"
#include "stm32f10x_iwdg.h"

/*
 * ================================================================
 *  ESP8266 透传模式驱动 (STM32端)
 * ================================================================
 *  说明：
 *    ESP8266 已烧录 Arduino 固件，WiFi/TCP 连接由 Arduino 端自行管理。
 *    STM32 只需通过 USART3 发送帧数据，Arduino 固件会解析帧协议后
 *    转发到 TCP 服务器。服务器下发的数据也会通过串口透传回 STM32。
 *
 *  硬件连接：
 *    STM32 PB10 (USART3_TX) → ESP8266 RX
 *    STM32 PB11 (USART3_RX) ← ESP8266 TX
 *    波特率: 115200
 * ================================================================
 */

// ===================== 串口3接收缓冲区 =====================
volatile uint8_t USART3_RX_BUF[USART3_RX_BUF_SIZE]; // 接收缓冲区 (ISR共享)
volatile uint16_t USART3_RX_Count = 0;              // 已接收字节计数 (ISR共享)
volatile uint8_t USART3_RX_Flag = 0;                // 接收完成标志 (ISR共享)

// ===================== 初始化 =====================
/**
 * @brief  初始化 USART3 硬件，用于与 ESP8266 Arduino 固件通信
 * @note   上电后 ESP8266 Arduino 固件会自动连接 WiFi 和 TCP，
 *         STM32 无需发送任何 AT 指令，等待约 5 秒即可开始发数据。
 */
void ESP8266_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  // 使能 USART3 和 GPIOB 时钟
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

  // 配置 USART3 TX 引脚 (PB10) —— 推挽复用输出
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  // 配置 USART3 RX 引脚 (PB11) —— 浮空输入
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  // 配置 USART3 参数: 115200-8-N-1
  USART_InitStructure.USART_BaudRate = 115200;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl =
      USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
  USART_Init(USART3, &USART_InitStructure);

  // 配置 USART3 接收中断
  NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

  // 启用 USART3 接收中断
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

  // 启用 USART3
  USART_Cmd(USART3, ENABLE);

  // 等待 ESP8266 Arduino 固件启动并完成 WiFi/TCP 连接（约 5 秒）
  // 分段延时并喂狗，避免 IWDG（4秒超时）复位
  {
    uint8_t i;
    for (i = 0; i < 5; i++) {
      Delay_ms(1000);
      IWDG_ReloadCounter();
    }
  }
}

// ===================== USART3 中断服务函数 =====================
/**
 * @brief  USART3 接收中断处理
 * @note   接收服务器通过 ESP8266 透传下发的数据
 */
void USART3_IRQHandler(void) {
  if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) {
    uint8_t rxByte = USART_ReceiveData(USART3);

    // 缓冲区未满时存储数据
    if (USART3_RX_Count < USART3_RX_BUF_SIZE) {
      USART3_RX_BUF[USART3_RX_Count++] = rxByte;
      USART3_RX_Flag = 1; // 标记有数据到达
    }

    USART_ClearITPendingBit(USART3, USART_IT_RXNE);
  }
}

// ===================== 发送函数 =====================
/**
 * @brief  通过 USART3 发送单字节数据
 * @param  byte: 要发送的字节
 */
void ESP8266_SendByte(uint8_t byte) {
  while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
    ;
  USART_SendData(USART3, byte);
  while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET)
    ;
}

/**
 * @brief  通过 USART3 发送字符串
 * @param  str: 要发送的字符串指针
 */
void ESP8266_SendString(const char *str) {
  while (*str) {
    ESP8266_SendByte((uint8_t)*str++);
  }
}

/**
 * @brief  封装并发送一帧数据到 ESP8266（由 Arduino 固件转发到 TCP 服务器）
 * @param  json: JSON 格式的载荷字符串
 * @note   帧格式: [0xAA 0xBB] [长度高字节] [长度低字节] [JSON数据] [0xCC 0xDD]
 *         与 paddy-server 的 protocol.h 完全兼容
 */
void ESP8266_SendFrame(const char *json) {
  uint16_t len = strlen(json);

  // 发送帧头
  ESP8266_SendByte(FRAME_HEADER_1); // 0xAA
  ESP8266_SendByte(FRAME_HEADER_2); // 0xBB

  // 发送载荷长度（大端序）
  ESP8266_SendByte((uint8_t)(len >> 8));
  ESP8266_SendByte((uint8_t)(len & 0xFF));

  // 发送 JSON 载荷
  while (*json) {
    ESP8266_SendByte((uint8_t)*json++);
  }

  // 发送帧尾
  ESP8266_SendByte(FRAME_TAIL_1); // 0xCC
  ESP8266_SendByte(FRAME_TAIL_2); // 0xDD
}

// ===================== 接收函数 =====================
/**
 * @brief  获取服务器下发的数据
 * @param  buf: 输出缓冲区
 * @param  maxLen: 缓冲区最大长度
 * @retval 实际拷贝的字节数，0 表示无新数据
 */
uint16_t ESP8266_GetReceivedData(uint8_t *buf, uint16_t maxLen) {
  uint16_t copyLen = 0;

  // 临界区：关 USART3 中断，防止 ISR 并发修改缓冲区
  USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
  if (USART3_RX_Flag && USART3_RX_Count > 0) {
    copyLen = (USART3_RX_Count < maxLen) ? USART3_RX_Count : maxLen;
    memcpy(buf, (const void *)USART3_RX_BUF, copyLen);

    // 清除接收状态
    USART3_RX_Count = 0;
    USART3_RX_Flag = 0;
  }
  USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

  return copyLen;
}
