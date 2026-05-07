#include "WINDOW.h"

// oled显示屏默认显示界面1（主运行监控）
void Window_Show(uint8_t window_num) {
  OLED_Clear(); // 清屏
  switch (window_num) {
  case 1:
    Window_Show1();
    break;
  case 2:
    Window_Show2();
    break;
  case 3:
    Window_Show3();
    break;
  case 4:
    Window_Show4();
    break;
  }
}

void Window_Show1(void) {
  // OLED_ShowString(1,0,"主运行监控");
  OLED_ShowCN(1, 1, 0, 1);
  OLED_ShowCN(1, 2, 1, 1);
  OLED_ShowCN(1, 3, 2, 1);
  OLED_ShowCN(1, 4, 3, 1);
  OLED_ShowCN(1, 5, 4, 1);
  OLED_ShowChar(1, 11, '1');

  // 运行模式
  OLED_ShowCN(2, 1, 5, 1);
  OLED_ShowCN(2, 2, 6, 1);
  OLED_ShowCN(2, 3, 7, 1);
  OLED_ShowCN(2, 4, 8, 1);
  OLED_ShowChar(2, 9, ':');

  // 存储重量
  OLED_ShowCN(3, 1, 9, 1);
  OLED_ShowCN(3, 2, 10, 1);
  OLED_ShowCN(3, 3, 11, 1);
  OLED_ShowCN(3, 4, 12, 1);
  OLED_ShowChar(3, 9, ':');

  // 连接状态
  OLED_ShowCN(4, 1, 13, 1);
  OLED_ShowCN(4, 2, 14, 1);
  OLED_ShowCN(4, 3, 15, 1);
  OLED_ShowCN(4, 4, 16, 1);
  OLED_ShowChar(4, 9, ':');
}

/**
 * @brief 显示窗口2的界面内容
 * 该函数用于在OLED屏幕上显示第二窗口的界面，包括环境感知详情、光照强度、
 * 机身姿态和报警提示等信息
 */
void Window_Show2(void) {
  // 环境感知详情 - 显示中文"环境感知"
  OLED_ShowCN(1, 1, 17, 1);  // 环
  OLED_ShowCN(1, 2, 18, 1);  // 境
  OLED_ShowCN(1, 3, 19, 1);  // 感
  OLED_ShowCN(1, 4, 20, 1);  // 知
  OLED_ShowCN(1, 5, 21, 1);  // 详
  OLED_ShowCN(1, 6, 22, 1);  // 情
  OLED_ShowChar(1, 13, '2');  // 显示窗口编号2

  // 机身姿态 - 显示中文"机身姿态"
  OLED_ShowCN(2, 1, 27, 1);  // 机
  OLED_ShowCN(2, 2, 28, 1);  // 身
  OLED_ShowCN(2, 3, 29, 1);  // 姿
  OLED_ShowCN(2, 4, 30, 1);  // 态
  OLED_ShowChar(2, 9, ':');   // 显示冒号分隔符

  // 报警提示 - 显示中文"报警提示"
  OLED_ShowCN(3, 1, 31, 1);  // 报
  OLED_ShowCN(3, 2, 32, 1);  // 警
  OLED_ShowCN(3, 3, 33, 1);  // 提
  OLED_ShowCN(3, 4, 34, 1);  // 示
  OLED_ShowChar(3, 9, ':');   // 显示冒号分隔符
}

void Window_Show3(void) {
  // 执行机构参数
  OLED_ShowCN(1, 1, 35, 1);
  OLED_ShowCN(1, 2, 36, 1);
  OLED_ShowCN(1, 3, 37, 1);
  OLED_ShowCN(1, 4, 38, 1);
  OLED_ShowCN(1, 5, 39, 1);
  OLED_ShowCN(1, 6, 40, 1);
  OLED_ShowChar(1, 13, '3');

  // 电机速度
  OLED_ShowCN(2, 1, 41, 1);
  OLED_ShowCN(2, 2, 42, 1);
  OLED_ShowCN(2, 3, 43, 1);
  OLED_ShowCN(2, 4, 44, 1);
  OLED_ShowChar(2, 9, ':');

  // 电量监控
  OLED_ShowCN(3, 1, 50, 1);
  OLED_ShowCN(3, 2, 51, 1);
  OLED_ShowCN(3, 3, 52, 1);
  OLED_ShowCN(3, 4, 53, 1);
  OLED_ShowChar(3, 9, ':');
}

void Window_Show4(void) {
  // 系统网络信息
  OLED_ShowCN(1, 1, 54, 1);
  OLED_ShowCN(1, 2, 55, 1);
  OLED_ShowCN(1, 3, 56, 1);
  OLED_ShowCN(1, 4, 57, 1);
  OLED_ShowCN(1, 5, 58, 1);
  OLED_ShowCN(1, 6, 59, 1);
  OLED_ShowChar(1, 13, '4');

  // TCP状态
  OLED_ShowString(2, 1, "TCP ");
  OLED_ShowCN(2, 3, 63, 1);
  OLED_ShowCN(2, 4, 64, 1);
  OLED_ShowChar(2, 9, ':');

  // 超声波距离
  OLED_ShowString(3, 1, "L:");
  OLED_ShowString(3, 7, "cm");
  OLED_ShowString(4, 1, "R:");
  OLED_ShowString(4, 7, "cm");
}
