/**
 * @file main.cpp
 * @brief 水稻收割机器人监控终端 - 程序入口
 *
 * 基于STM32的水稻收割机器人系统设计 - 上位机TCP客户端
 */

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 设置全局字体
    QFont font("Segoe UI", 10);
    a.setFont(font);

    MainWindow w;
    w.show();

    return a.exec();
}
