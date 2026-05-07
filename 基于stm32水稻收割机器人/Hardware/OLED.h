#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

/**
 * @brief  OLED初始化
 */
void OLED_Init(void);

/**
 * @brief  清屏
 */
void OLED_Clear(void);

/**
 * @brief  显示单个字符
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)，每个字符占1列
 * @param  Char    要显示的字符
 * @example OLED_ShowChar(1, 1, 'A');  // 第1行第1列显示'A'
 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);

/**
 * @brief  显示字符串
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)
 * @param  String  要显示的字符串
 * @example OLED_ShowString(2, 1, "Hello");  // 第2行第1列显示"Hello"
 */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);

/**
 * @brief  显示无符号整数
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)
 * @param  Number  要显示的数字
 * @param  Length  数字位数 (1-10)
 * @example OLED_ShowNum(3, 1, 12345, 5);  // 第3行第1列显示"12345"
 *          OLED_ShowNum(3, 1, 42, 3);     // 第3行第1列显示"042"(补零)
 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number,
                  uint8_t Length);

/**
 * @brief  显示有符号整数
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)
 * @param  Number  要显示的数字 (可为负数)
 * @param  Length  数字位数 (1-10，不含符号位)
 * @example OLED_ShowSignedNum(1, 1, -123, 3);  // 显示"-123"
 */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number,
                        uint8_t Length);

/**
 * @brief  显示十六进制数
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)
 * @param  Number  要显示的数字
 * @param  Length  十六进制位数 (1-8)
 * @example OLED_ShowHexNum(1, 1, 0xAB, 2);  // 显示"AB"
 *          OLED_ShowHexNum(1, 1, 255, 4);   // 显示"00FF"
 */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number,
                     uint8_t Length);

/**
 * @brief  显示二进制数
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)
 * @param  Number  要显示的数字
 * @param  Length  二进制位数 (1-32)
 * @example OLED_ShowBinNum(1, 1, 5, 8);  // 显示"00000101"
 */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number,
                     uint8_t Length);

/**
 * @brief  画点
 * @param  x      X坐标 (0-127)
 * @param  y      Y坐标 (0-63)
 * @param  mode    0-清除点, 1-画点
 * @example OLED_DrawPoint(64, 32, 1);  // 在屏幕中心画一个点
 */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t mode);

/**
 * @brief  显示图片
 * @param  x       起始X坐标 (0-127)
 * @param  y       起始Y坐标 (0-63)
 * @param  width   图片宽度
 * @param  height  图片高度
 * @param  pic     图片数据数组指针
 * @param  mode    0-反色显示, 1-正常显示
 */

/**
 * @brief  显示汉字
 * @param  Line    行号 (1-4)
 * @param  Column  列号 (1-16)
 * @param  Num     汉字在字库中的索引
 * @param  mode    0-反色显示, 1-正常显示
 */
void OLED_ShowCN(uint8_t Line, uint8_t Column, uint8_t Num, uint8_t mode);

/**
 * @brief  显示BMP图片
 * @param  x       起始X坐标 (0-127)
 * @param  y       起始Y坐标 (0-63)
 * @param  width   图片宽度
 * @param  height  图片高度
 * @param  bmp     图片数据数组指针
 * @param  mode    0-反色显示, 1-正常显示
 */
void OLED_ShowBMP(uint8_t x, uint8_t y, uint8_t width, uint8_t height,
                  const uint8_t *bmp, uint8_t mode);

#endif
