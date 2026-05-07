#include "SU03T.h"

// 接收状态机状态，用于解析 AA XX 的协议 (ISR共享)
static volatile uint8_t SU03T_RxState = 0;
// 最新接收到的命令 (ISR共享)
static volatile uint8_t SU03T_RxCommand = SU03T_CMD_NONE;
// 接收标志位 (ISR共享)
static volatile uint8_t SU03T_RxFlag = 0;

/**
 * @brief  初始化 SU03T 使用的 USART2 和 GPIO
 * @param  无
 * @retval 无
 */
void SU03T_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;
  USART_InitTypeDef USART_InitStructure;
  NVIC_InitTypeDef NVIC_InitStructure;

  // 1. 开启时钟
  RCC_APB2PeriphClockCmd(SU03T_GPIO_CLK, ENABLE);
  RCC_APB1PeriphClockCmd(SU03T_USART_CLK, ENABLE);

  // 2. 配置 PA2 作为 TX (推挽复用输出)
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Pin = SU03T_GPIO_TX_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(SU03T_GPIO_TX_PORT, &GPIO_InitStructure);

  // 3. 配置 PA3 作为 RX (浮空或上拉输入)
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin = SU03T_GPIO_RX_PIN;
  GPIO_Init(SU03T_GPIO_RX_PORT, &GPIO_InitStructure);

  // 4. 配置 USART 参数 (115200波特率，8数据位，1停止位，无校验)
  USART_InitStructure.USART_BaudRate = SU03T_BAUDRATE;
  USART_InitStructure.USART_HardwareFlowControl =
      USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_Init(SU03T_USART, &USART_InitStructure);

  // 5. 配置 USART 接收中断
  USART_ITConfig(SU03T_USART, USART_IT_RXNE, ENABLE);

  // 6. 配置 NVIC 中断优先级
  NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
  NVIC_Init(&NVIC_InitStructure);

  // 7. 使能 USART
  USART_Cmd(SU03T_USART, ENABLE);
}

/**
 * @brief  USART2 中断服务函数
 * @param  无
 * @retval 无
 */
void USART2_IRQHandler(void) {
  // 如果接收寄存器非空
  if (USART_GetITStatus(SU03T_USART, USART_IT_RXNE) != RESET) {
    uint8_t rx_byte = USART_ReceiveData(SU03T_USART);

    // 状态机解析协议: 帧头 0xAA，接着 1 字节命令
    if (SU03T_RxState == 0) {
      if (rx_byte == 0xAA) {
        SU03T_RxState = 1; // 接收到帧头
      }
    } else if (SU03T_RxState == 1) {
      // 接收到真正的命令字节
      SU03T_RxCommand = rx_byte;
      SU03T_RxFlag = 1;  // 置位接收成功标志
      SU03T_RxState = 0; // 状态机复位，等待下一个 AA
    }

    USART_ClearITPendingBit(SU03T_USART, USART_IT_RXNE);
  }
}

/**
 * @brief  获取 SU03T 发送来的识别命令
 * @param  无
 * @retval 接收到的命令码。如果没有新命令，则返回 SU03T_CMD_NONE (0xFE)
 */
uint8_t SU03T_GetCommand(void) {
  uint8_t cmd_ret = SU03T_CMD_NONE;
  // 临界区：关 USART2 中断，防止 ISR 并发修改
  USART_ITConfig(SU03T_USART, USART_IT_RXNE, DISABLE);
  if (SU03T_RxFlag == 1) {
    cmd_ret = SU03T_RxCommand;
    SU03T_RxFlag = 0; // 清除标志位
  }
  USART_ITConfig(SU03T_USART, USART_IT_RXNE, ENABLE);
  return cmd_ret;
}

/**
 * @brief  手工清除当前的命令标志与状态
 * @param  无
 * @retval 无
 */
/**
 * @brief 清除SU03T模块的命令状态
 * @details 该函数用于重置SU03T模块的接收标志、命令和状态变量
 *          将它们恢复到初始值，以便接收新的命令
 */
void SU03T_ClearCommand(void) {
  USART_ITConfig(SU03T_USART, USART_IT_RXNE, DISABLE);
  SU03T_RxFlag = 0;                 // 清除接收标志
  SU03T_RxCommand = SU03T_CMD_NONE; // 将命令重置为无命令状态
  SU03T_RxState = 0;                // 重置接收状态机状态
  USART_ITConfig(SU03T_USART, USART_IT_RXNE, ENABLE);
}

/**
 * @brief  向 SU03T 发送单字节数据
 * @param  data 要发送的数据
 * @retval 无
 */
void SU03T_SendData(uint8_t data) {
  USART_SendData(SU03T_USART, data);
  // 等待发送完成标志位空
  while (USART_GetFlagStatus(SU03T_USART, USART_FLAG_TXE) == RESET)
    ;
}

/**
 * @brief  驱动 SU03T 播放指定语音
 * @param  voice_cmd 在定制平台上配置的触发语音指令代码
 * @retval 无
 */
void SU03T_PlayVoice(uint8_t voice_cmd) {
  // 根据需要可扩展为发送多字节。这里假设发一字节指令
  SU03T_SendData(voice_cmd);
}
