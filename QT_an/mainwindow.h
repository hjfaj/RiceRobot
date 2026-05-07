/**
 * @file mainwindow.h
 * @brief 主窗口头文件 - 水稻收割机器人监控终端
 *
 * 声明主窗口类，包含UI组件、TCP通信、数据解析等功能
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAbstractSocket>
#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTcpSocket>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private slots:
  // ---- TCP 连接相关槽函数 ----
  void onConnectClicked();                             // 连接/断开按钮点击
  void onTcpConnected();                               // TCP连接成功
  void onTcpDisconnected();                            // TCP连接断开
  void onTcpReadyRead();                               // 接收到TCP数据
  void onTcpError(QAbstractSocket::SocketError error); // TCP错误

  // ---- 控制按钮槽函数 ----
  void onStartClicked();    // 启动收割
  void onStopClicked();     // 紧急停止
  void onForwardClicked();  // 前进
  void onBackwardClicked(); // 后退
  void onLeftClicked();     // 左转
  void onRightClicked();    // 右转
  void onSpeedSliderMoved(int value);  // 速度滑块拖动中（更新显示）
  void onSpeedSliderReleased();        // 速度滑块释放（发送命令）

private:
  // ---- 初始化函数 ----
  void setupUi();          // 构建UI界面
  void setupStyles();      // 设置暗黑工业风样式表
  void setupConnections(); // 建立信号槽连接

  // ---- 辅助函数 ----
  void appendLog(const QString &msg);          // 向日志区追加消息
  void sendCommand(const QString &cmd);        // 发送TCP指令
  void tryParseFrame();                        // 尝试提取并解析完整协议帧
  void parseReceivedData(const QByteArray &data); // 解析接收到的传感器数据
  void updateConnectionStatus(bool connected,
                              const QString &info = ""); // 更新连接状态显示

  // ---- TCP 通信 ----
  QTcpSocket *m_tcpSocket; // TCP套接字
  QByteArray m_recvBuffer; // 接收缓冲区，用于处理 TCP 粘包和半包

  // ---- 连接栏组件 ----
  QLineEdit *m_ipEdit;       // IP地址输入框
  QSpinBox *m_portSpin;      // 端口号输入框
  QPushButton *m_connectBtn; // 连接/断开按钮

  // ---- 数据监控组件 ----
  QLabel *m_weightValue;      // 载重数值显示
  QLabel *m_speedValue;       // 行驶速度显示
  QProgressBar *m_batteryBar; // 电量进度条
  QLabel *m_batteryLabel;     // 电量百分比文字
  QLabel *m_lightValue;       // 光照强度显示
  QLabel *m_connStatusLabel;  // 连接状态文字
  QLabel *m_motorStatusLabel; // 电机状态文字
  QLabel *m_robotOnlineLabel; // 机器人在线状态文字
  QLabel *m_storageLabel;     // 存储仓状态文字
  QLabel *m_deviceIdLabel;    // 设备编号文字

  // ---- 控制按钮 ----
  QPushButton *m_startBtn;    // 启动收割
  QPushButton *m_stopBtn;     // 紧急停止
  QPushButton *m_forwardBtn;  // 前进
  QPushButton *m_backwardBtn; // 后退
  QPushButton *m_leftBtn;     // 左转
  QPushButton *m_rightBtn;    // 右转

  // ---- 速度控制 ----
  QSlider *m_speedSlider;     // 速度滑块
  QLabel *m_speedSliderLabel; // 速度显示标签

  // ---- 日志区 ----
  QTextEdit *m_logArea; // 日志文本框
};

#endif // MAINWINDOW_H
