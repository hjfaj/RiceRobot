#include "HX711.h"
#include "Delay.h"

long HX711_Offset = 0;
float HX711_GapValue = 405.0; // 这个系数需要你放一个已知重量的砝码进行计算调整
                              // (GapValue = 原始差值 / 砝码重量)

/**
 * @brief  初始化 HX711 所需的 GPIO 引脚
 */
void HX711_Init(void) {
  GPIO_InitTypeDef GPIO_InitStructure;

  // 开启 GPIOB 时钟
  RCC_APB2PeriphClockCmd(HX711_GPIO_CLK, ENABLE);

  // HX711_SCK(PB5) 推挽输出
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
  GPIO_InitStructure.GPIO_Pin = HX711_SCK_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(HX711_GPIO_PORT, &GPIO_InitStructure);

  // HX711_DOUT(PB6) 浮空输入或上拉输入均可，HX711 默认会拉高
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_InitStructure.GPIO_Pin = HX711_DOUT_PIN;
  GPIO_Init(HX711_GPIO_PORT, &GPIO_InitStructure);

  // 初始化时将 SCK 拉低
  HX711_SCK_LOW();

  // 初始化完成后自动进行一次去皮运算
  Delay_ms(200); // 延时等待模块稳定
  HX711_Tare();
}

/**
 * @brief  从 HX711 读取 24 位原始 ADC 值
 * @return 返回读取到的 24位有符号整数(处理为 uint32_t)
 */
/**
 * @brief 从HX711读取24位数据
 * @return uint32_t 返回读取到的24位数据（转换为32位无符号整数）
 */
uint32_t HX711_Read(void) {
  uint32_t count = 0;    // 用于存储读取到的数据
  uint8_t i;             // 循环计数器
  uint32_t timeout = 100000; // 超时计数

  // 等待HX711数据准备就绪
  // 数据线拉高时说明还没准备好，等待拉低
  HX711_SCK_LOW();       // 将SCK线拉低
  count = 0;             // 清空计数器
  
  // 添加超时机制
  while (HX711_DOUT_READ() == 1)  // 检测DOUT引脚，直到变为低电平（数据准备好）
  {
    if (timeout-- == 0) {
      return 0; // 超时返回0
    }
  }

  // 读取 24 位数据
  for (i = 0; i < 24; i++) {  // 循环24次，读取24位数据
    HX711_SCK_HIGH();         // SCK产生上升沿
    count = count << 1;      // 将count左移一位，为下一位数据腾出位置
    Delay_us(1); // 保证 SCK 高电平时间 (>0.2us)
    HX711_SCK_LOW();
    if (HX711_DOUT_READ()) {
      count++;
    }
    Delay_us(1); // 保证 SCK 低电平时间 (>0.2us)
  }

  // 第 25 个脉冲，配置下一次为 128 增益
  HX711_SCK_HIGH();
  Delay_us(1);
  // 处理负数，HX711输出的是补码，如果是负数，最高位为1
  // (对于24位数据，最高位即第24位)
  if (count & 0x800000) count |= 0xFF000000;
  HX711_SCK_LOW();
  Delay_us(1);

  return (count);
}

/**
 * @brief  多次读取求平均值，用于滤波
 * @param  times 采样的次数
 * @return 采样的平均值
 */
long HX711_Get_Average(uint8_t times) {
  long sum = 0;
  uint8_t i;
  if (times == 0) return 0; // 防止除零
  for (i = 0; i < times; i++) {
    sum += HX711_Read();
  }
  return sum / times;
}

/**
 * @brief  去皮重：将当前多次平均值设为零点偏移量
 */
void HX711_Tare(void) { HX711_Offset = HX711_Get_Average(10); }

/**
 * @brief  获取换算之后的实际重量（默认单位假设是克 g 或 公斤 Kg）
 *         这取决于 GapValue 的比例校准。
 * @return 浮点类型的重量值
 */
float HX711_Get_Weight(void) {
  long current_value = HX711_Get_Average(3); // 取 3 次平均加快反应速度
  long diff;

  // 防止因为轻微震动导致值为负数
  if (current_value > HX711_Offset) {
    diff = current_value - HX711_Offset;
  } else {
    diff = 0;
  }

  // 将原始差值换算为实际重量
  float weight = (float)diff / HX711_GapValue;
  return weight;
}
