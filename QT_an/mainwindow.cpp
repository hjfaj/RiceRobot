/**
 * @file mainwindow.cpp
 * @brief 水稻收割机器人监控终端 - 主窗口实现
 */

#include "mainwindow.h"
#include <QDebug>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollArea>
#include <QScrollBar>
#include <QTime>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  // 初始化网络
  m_tcpSocket = new QTcpSocket(this);

  // 构建 UI 和连接信号槽
  setupUi();
  setupStyles();
  setupConnections();

  // 初始状态
  updateConnectionStatus(false, "Disconnected");
  m_weightValue->setText("0.0");
  m_speedValue->setText("0.0");
  m_batteryBar->setValue(0);
  m_batteryLabel->setText("0%");
  m_lightValue->setText("0");
  m_motorStatusLabel->setText("未知");
  m_storageLabel->setText("未知");
  m_deviceIdLabel->setText("N/A");
}

MainWindow::~MainWindow() {
  if (m_tcpSocket->isOpen()) {
    m_tcpSocket->disconnectFromHost();
  }
}

void MainWindow::setupUi() {
  this->setWindowTitle("RiceRobot 监控终端");
#ifdef Q_OS_ANDROID
  this->showMaximized();
#else
  this->resize(900, 680);
#endif

  QWidget *centralWidget = new QWidget(this);
  this->setCentralWidget(centralWidget);
  centralWidget->setObjectName("centralWidget");

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(10);

  // ==========================================
  // 1. 顶部栏：标题 + 连接
  // ==========================================
  QHBoxLayout *topBar = new QHBoxLayout();
  QLabel *titleLabel = new QLabel("RiceRobot 监控终端", this);
  titleLabel->setObjectName("headerTitle");

  QLabel *ipLabel = new QLabel("服务器:", this);
  m_ipEdit = new QLineEdit(this);
  m_ipEdit->setText("113.45.231.111");
  m_ipEdit->setPlaceholderText("IP 地址");
  m_ipEdit->setFixedWidth(160);

  QLabel *portLabel = new QLabel("端口:", this);
  m_portSpin = new QSpinBox(this);
  m_portSpin->setRange(1, 65535);
  m_portSpin->setValue(8889);
  m_portSpin->setFixedWidth(100);

  m_connectBtn = new QPushButton("连接", this);
  m_connectBtn->setObjectName("btnConnect");
  m_connectBtn->setFixedSize(80, 32);

  topBar->addWidget(titleLabel);
  topBar->addStretch();
  topBar->addWidget(ipLabel);
  topBar->addWidget(m_ipEdit);
  topBar->addWidget(portLabel);
  topBar->addWidget(m_portSpin);
  topBar->addWidget(m_connectBtn);

  // ==========================================
  // 2. 分页容器
  // ==========================================
  m_tabWidget = new QTabWidget(this);
  m_tabWidget->setObjectName("mainTab");

  // === Tab 1: 数据监控 ===
  QWidget *tabMonitor = new QWidget();
  QVBoxLayout *monitorLayout = new QVBoxLayout(tabMonitor);
  monitorLayout->setContentsMargins(8, 8, 8, 8);
  monitorLayout->setSpacing(10);

  // 数值卡片 2x2 网格
  QGridLayout *dataGrid = new QGridLayout();
  dataGrid->setSpacing(10);

  auto makeCard = [&](const QString &title, QLabel *&value, const QString &unit,
                       const QString &objName) {
    QFrame *card = new QFrame();
    card->setObjectName(objName);
    QVBoxLayout *lay = new QVBoxLayout(card);
    lay->setContentsMargins(12, 8, 12, 8);
    QLabel *tt = new QLabel(title);
    tt->setObjectName("cardLabel");
    value = new QLabel("0");
    value->setObjectName("lcdNumber");
    QLabel *uu = new QLabel(unit);
    uu->setObjectName("unitLabel");
    QHBoxLayout *row = new QHBoxLayout();
    row->addWidget(value);
    row->addWidget(uu);
    row->addStretch();
    lay->addWidget(tt);
    lay->addLayout(row);
    return card;
  };

  // 第一行: 载重、速度
  dataGrid->addWidget(makeCard("当前载重", m_weightValue, "KG", "dataCardBlue"), 0, 0);
  dataGrid->addWidget(makeCard("行驶速度", m_speedValue, "m/s", "dataCardBlue"), 0, 1);

  // 第二行: 电量、光照
  QFrame *batteryCard = new QFrame();
  batteryCard->setObjectName("dataCardYellow");
  QVBoxLayout *batteryLayout = new QVBoxLayout(batteryCard);
  batteryLayout->setContentsMargins(12, 8, 12, 8);
  QLabel *bLabel = new QLabel("电池电量");
  bLabel->setObjectName("cardLabel");
  m_batteryBar = new QProgressBar();
  m_batteryBar->setObjectName("batteryBar");
  m_batteryBar->setRange(0, 100);
  m_batteryBar->setTextVisible(true);
  m_batteryBar->setFormat("%p%");
  m_batteryLabel = new QLabel("0%");
  m_batteryLabel->setObjectName("batteryText");
  batteryLayout->addWidget(bLabel);
  batteryLayout->addWidget(m_batteryBar);
  batteryLayout->addWidget(m_batteryLabel);
  dataGrid->addWidget(batteryCard, 1, 0);

  QFrame *lightCard = new QFrame();
  lightCard->setObjectName("dataCardYellow");
  QVBoxLayout *lightLayout = new QVBoxLayout(lightCard);
  lightLayout->setContentsMargins(12, 8, 12, 8);
  QLabel *lLabel = new QLabel("光照强度");
  lLabel->setObjectName("cardLabel");
  m_lightValue = new QLabel("0");
  m_lightValue->setObjectName("lcdNumber");
  QLabel *luxUnit = new QLabel("Lux");
  luxUnit->setObjectName("unitLabel");
  QHBoxLayout *luxRow = new QHBoxLayout();
  luxRow->addWidget(m_lightValue);
  luxRow->addWidget(luxUnit);
  luxRow->addStretch();
  lightLayout->addWidget(lLabel);
  lightLayout->addLayout(luxRow);
  dataGrid->addWidget(lightCard, 1, 1);

  monitorLayout->addLayout(dataGrid);

  // 状态信息组 (水平排列)
  QHBoxLayout *statusRow = new QHBoxLayout();
  statusRow->setSpacing(8);

  auto makeStatus = [&](const QString &title, QLabel *&label, const QString &init) {
    QFrame *f = new QFrame();
    f->setObjectName("statusCard");
    QVBoxLayout *l = new QVBoxLayout(f);
    l->setContentsMargins(8, 6, 8, 6);
    QLabel *tt = new QLabel(title);
    tt->setObjectName("cardLabel");
    label = new QLabel(init);
    label->setObjectName("connStatusText");
    l->addWidget(tt);
    l->addWidget(label);
    return f;
  };

  statusRow->addWidget(makeStatus("电机状态", m_motorStatusLabel, "未知"));
  statusRow->addWidget(makeStatus("存储仓", m_storageLabel, "未知"));
  statusRow->addWidget(makeStatus("设备编号", m_deviceIdLabel, "N/A"));
  statusRow->addWidget(makeStatus("机器人", m_robotOnlineLabel, "离线"));
  statusRow->addWidget(makeStatus("网络", m_connStatusLabel, "Disconnected"));
  monitorLayout->addLayout(statusRow);
  monitorLayout->addStretch();

  m_tabWidget->addTab(tabMonitor, "  数据监控  ");

  // === Tab 2: 运动控制 ===
  QWidget *tabControl = new QWidget();
  QVBoxLayout *controlLayout = new QVBoxLayout(tabControl);
  controlLayout->setContentsMargins(16, 16, 16, 16);
  controlLayout->setSpacing(12);

  m_startBtn = new QPushButton("启动收割");
  m_startBtn->setObjectName("btnStart");
  m_startBtn->setMinimumHeight(60);

  m_stopBtn = new QPushButton("紧急停止");
  m_stopBtn->setObjectName("btnStop");
  m_stopBtn->setMinimumHeight(60);

  // 方向键布局
  QGridLayout *dirGrid = new QGridLayout();
  dirGrid->setSpacing(8);

  m_forwardBtn = new QPushButton("前进");
  m_forwardBtn->setObjectName("btnFunc");
  m_forwardBtn->setMinimumHeight(64);

  m_backwardBtn = new QPushButton("后退");
  m_backwardBtn->setObjectName("btnFunc");
  m_backwardBtn->setMinimumHeight(64);

  m_leftBtn = new QPushButton("左转");
  m_leftBtn->setObjectName("btnFunc");
  m_leftBtn->setMinimumHeight(64);

  m_rightBtn = new QPushButton("右转");
  m_rightBtn->setObjectName("btnFunc");
  m_rightBtn->setMinimumHeight(64);

  dirGrid->addWidget(m_forwardBtn, 0, 1);
  dirGrid->addWidget(m_leftBtn, 1, 0);
  dirGrid->addWidget(m_rightBtn, 1, 2);
  dirGrid->addWidget(m_backwardBtn, 2, 1);

  // 速度控制
  QHBoxLayout *speedLayout = new QHBoxLayout();
  QLabel *speedLabel = new QLabel("电机速度:");
  speedLabel->setObjectName("cardLabel");
  m_speedSlider = new QSlider(Qt::Horizontal);
  m_speedSlider->setObjectName("speedSlider");
  m_speedSlider->setRange(10, 100);
  m_speedSlider->setValue(30);
  m_speedSliderLabel = new QLabel("30%");
  m_speedSliderLabel->setObjectName("speedSliderLabel");
  m_speedSliderLabel->setFixedWidth(70);
  speedLayout->addWidget(speedLabel);
  speedLayout->addWidget(m_speedSlider);
  speedLayout->addWidget(m_speedSliderLabel);

  controlLayout->addWidget(m_startBtn);
  controlLayout->addLayout(dirGrid);
  controlLayout->addWidget(m_stopBtn);
  controlLayout->addLayout(speedLayout);
  controlLayout->addStretch();

  m_tabWidget->addTab(tabControl, "  运动控制  ");

  // === Tab 3: 日志 ===
  QWidget *tabLog = new QWidget();
  QVBoxLayout *logLayout = new QVBoxLayout(tabLog);
  logLayout->setContentsMargins(4, 4, 4, 4);
  m_logArea = new QTextEdit();
  m_logArea->setObjectName("logArea");
  m_logArea->setReadOnly(true);
  logLayout->addWidget(m_logArea);

  m_tabWidget->addTab(tabLog, "  日志  ");

  // 主布局
  mainLayout->addLayout(topBar);
  mainLayout->addWidget(m_tabWidget, 1);

  m_startBtn->setEnabled(false);
  m_stopBtn->setEnabled(false);
  m_forwardBtn->setEnabled(false);
  m_backwardBtn->setEnabled(false);
  m_leftBtn->setEnabled(false);
  m_rightBtn->setEnabled(false);

  appendLog("系统已初始化，等待连接...");
}

void MainWindow::setupStyles() {
  QString qss = R"(
    /* 全局 */
    #centralWidget { background-color: #2b2b2b; }
    QLabel, QLineEdit, QSpinBox, QPushButton, QTextEdit {
      font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;
      color: #e0e0e0;
    }
    QLabel { font-size: 13px; }

    /* 标题 */
    #headerTitle {
      font-size: 18px; font-weight: bold; color: #00d2ff;
    }

    /* 输入框 */
    QLineEdit, QSpinBox {
      background: #3c3c3c; border: 1px solid #555; border-radius: 4px;
      padding: 4px 8px; font-size: 13px;
    }
    QLineEdit:focus, QSpinBox:focus { border-color: #00d2ff; }

    /* 连接按钮 */
    #btnConnect {
      background: #0078d4; border: none; border-radius: 4px;
      font-size: 13px; font-weight: bold; color: #fff; padding: 4px;
    }
    #btnConnect:hover { background: #1a8ae8; }
    #btnConnect:pressed { background: #005fa3; }

    /* Tab 控件 */
    QTabWidget::pane {
      background: #333; border: 1px solid #555; border-radius: 4px;
    }
    QTabBar::tab {
      background: #444; color: #ccc; padding: 8px 24px;
      border: 1px solid #555; border-bottom: none;
      border-top-left-radius: 6px; border-top-right-radius: 6px;
      font-size: 14px; font-weight: bold;
    }
    QTabBar::tab:selected {
      background: #333; color: #00d2ff;
      border-bottom: 2px solid #00d2ff;
    }
    QTabBar::tab:hover:!selected { background: #4a4a4a; }

    /* 数据卡片 */
    #dataCardBlue, #dataCardYellow {
      background: #3a3a3a; border-left: 4px solid #00d2ff; border-radius: 6px;
    }
    #dataCardYellow { border-left: 4px solid #ffcc00; }

    #cardLabel { color: #999; font-size: 12px; background: transparent; }

    /* LCD 数值 */
    #lcdNumber {
      font-size: 32px; font-weight: bold; color: #00ff00;
      background: transparent;
    }
    #unitLabel {
      font-size: 14px; color: #888; background: transparent;
      padding-left: 4px;
    }

    /* 状态卡片 */
    #statusCard {
      background: #3a3a3a; border-left: 3px solid #888; border-radius: 4px;
      min-width: 80px; padding: 4px 8px;
    }

    /* 状态文本 */
    #connStatusText {
      font-size: 14px; font-weight: bold; background: transparent;
      min-height: 20px;
    }
    #batteryText { font-size: 13px; font-weight: bold; background: transparent; }

    /* 进度条 */
    QProgressBar {
      background: #222; border: none; border-radius: 6px; height: 18px;
      text-align: center; font-size: 12px; color: #fff;
    }
    QProgressBar::chunk {
      background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #ffcc00, stop:1 #ff8800);
      border-radius: 6px;
    }

    /* 控制按钮 */
    #btnStart {
      background: #28a745; border: none; border-radius: 8px;
      font-size: 18px; font-weight: bold; color: #fff;
    }
    #btnStart:hover { background: #34ce57; }
    #btnStart:pressed { background: #1e7e34; }
    #btnStart:disabled { background: #555; color: #888; }

    #btnStop {
      background: #dc3545; border: none; border-radius: 8px;
      font-size: 18px; font-weight: bold; color: #fff;
    }
    #btnStop:hover { background: #f04b5a; }
    #btnStop:pressed { background: #bd2130; }
    #btnStop:disabled { background: #555; color: #888; }

    #btnFunc {
      background: #0078d4; border: none; border-radius: 8px;
      font-size: 16px; font-weight: bold; color: #fff;
    }
    #btnFunc:hover { background: #1a8ae8; }
    #btnFunc:pressed { background: #005fa3; }
    #btnFunc:disabled { background: #555; color: #888; }

    /* 速度滑块 */
    #speedSlider { background: transparent; height: 28px; }
    #speedSlider::groove:horizontal {
      background: #444; height: 8px; border-radius: 4px;
    }
    #speedSlider::handle:horizontal {
      background: #00d2ff; width: 22px; margin: -7px 0; border-radius: 11px;
    }
    #speedSlider::sub-page:horizontal {
      background: #00d2ff; border-radius: 4px;
    }
    #speedSliderLabel { color: #00ff00; font-size: 15px; font-weight: bold; }

    /* 日志 */
    #logArea {
      background: #1e1e1e; color: #00dd00;
      font-family: 'Consolas', 'Courier New', monospace;
      font-size: 13px; border: 1px solid #444; border-radius: 4px;
    }
  )";

  this->setStyleSheet(qss);
}

void MainWindow::setupConnections() {
  // TCP
  connect(m_tcpSocket, &QTcpSocket::connected, this,
          &MainWindow::onTcpConnected);
  connect(m_tcpSocket, &QTcpSocket::disconnected, this,
          &MainWindow::onTcpDisconnected);
  connect(m_tcpSocket, &QTcpSocket::readyRead, this,
          &MainWindow::onTcpReadyRead);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  connect(m_tcpSocket, &QTcpSocket::errorOccurred, this,
          &MainWindow::onTcpError);
#else
  connect(m_tcpSocket,
          QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error), this,
          &MainWindow::onTcpError);
#endif

  // UI
  connect(m_connectBtn, &QPushButton::clicked, this,
          &MainWindow::onConnectClicked);
  connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartClicked);
  connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStopClicked);
  connect(m_forwardBtn, &QPushButton::clicked, this,
          &MainWindow::onForwardClicked);
  connect(m_backwardBtn, &QPushButton::clicked, this,
          &MainWindow::onBackwardClicked);
  connect(m_leftBtn, &QPushButton::clicked, this, &MainWindow::onLeftClicked);
  connect(m_rightBtn, &QPushButton::clicked, this, &MainWindow::onRightClicked);
  connect(m_speedSlider, &QSlider::valueChanged, this, &MainWindow::onSpeedSliderMoved);
  connect(m_speedSlider, &QSlider::sliderReleased, this, &MainWindow::onSpeedSliderReleased);
}

void MainWindow::appendLog(const QString &msg) {
  QString timeStr = QTime::currentTime().toString("hh:mm:ss");
  m_logArea->append(QString("[%1] %2").arg(timeStr).arg(msg));

  // 仅当用户在底部附近时才自动滚动（避免打断阅读）
  QScrollBar *scrollbar = m_logArea->verticalScrollBar();
  if (scrollbar && scrollbar->value() >= scrollbar->maximum() - 50) {
    scrollbar->setValue(scrollbar->maximum());
  }
}

void MainWindow::updateConnectionStatus(bool connected, const QString &info) {
  if (connected) {
    m_connStatusLabel->setText("已连接");
    m_connStatusLabel->setStyleSheet("color: #00ff00; font-weight: bold; background: transparent;");
    m_connectBtn->setText("断开");
    m_ipEdit->setEnabled(false);
    m_portSpin->setEnabled(false);
    m_tabWidget->setTabText(0, "  数据监控  "); // 可加绿色圆点
  } else {
    m_connStatusLabel->setText("未连接");
    m_connStatusLabel->setStyleSheet("color: #999; font-weight: bold; background: transparent;");
    m_connectBtn->setText("连接");
    m_ipEdit->setEnabled(true);
    m_portSpin->setEnabled(true);
  }

  m_startBtn->setEnabled(connected);
  m_stopBtn->setEnabled(connected);
  m_forwardBtn->setEnabled(connected);
  m_backwardBtn->setEnabled(connected);
  m_leftBtn->setEnabled(connected);
  m_rightBtn->setEnabled(connected);
}

// ==========================================
// TCP 槽函数
// ==========================================

void MainWindow::onConnectClicked() {
  if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
    // 请求断开
    appendLog("Disconnecting from server...");
    m_tcpSocket->disconnectFromHost();
  } else if (m_tcpSocket->state() == QAbstractSocket::ConnectingState ||
             m_tcpSocket->state() == QAbstractSocket::HostLookupState) {
    // 正在连接中，取消连接
    appendLog("Aborting current connection attempt...");
    m_tcpSocket->abort();
  } else {
    // 请求连接
    QString ip = m_ipEdit->text().trimmed();
    quint16 port = m_portSpin->value();

    if (ip.isEmpty()) {
      appendLog("错误: 服务器地址不能为空");
      return;
    }

    // IP 地址简单校验
    QHostAddress addr(ip);
    if (addr.isNull()) {
      appendLog("错误: 无效的服务器地址");
      return;
    }

    appendLog(QString("Attempting to connect to %1:%2...").arg(ip).arg(port));
    m_tcpSocket->connectToHost(ip, port);
  }
}

void MainWindow::onTcpConnected() {
  QString peerInfo = m_tcpSocket->peerAddress().toString();
  appendLog(QString("TCP Connected to %1").arg(peerInfo));
  updateConnectionStatus(true, peerInfo);
}

void MainWindow::onTcpDisconnected() {
  appendLog("TCP Disconnected");
  updateConnectionStatus(false, "Disconnected");
  m_robotOnlineLabel->setText("离线");
  m_robotOnlineLabel->setStyleSheet("color: #999; font-weight: bold; background: transparent;");

  m_weightValue->setText("0");
  m_speedValue->setText("0.0");
  m_batteryBar->setValue(0);
  m_batteryLabel->setText("0%");
  m_lightValue->setText("0");
  m_motorStatusLabel->setText("未知");
  m_storageLabel->setText("未知");
  m_deviceIdLabel->setText("N/A");
}

void MainWindow::onTcpError(QAbstractSocket::SocketError error) {
  Q_UNUSED(error);
  QString errStr = m_tcpSocket->errorString();
  appendLog(QString("TCP Error: %1").arg(errStr));
  updateConnectionStatus(false, "Error");
}

void MainWindow::onTcpReadyRead() {
  QByteArray data = m_tcpSocket->readAll();
  // 防止缓冲区无界增长（超过 64KB 则清空并告警）
  if (m_recvBuffer.size() + data.size() > 65536) {
    appendLog("Error: Recv buffer overflow, clearing buffer.");
    m_recvBuffer.clear();
  }
  m_recvBuffer.append(data);
  tryParseFrame();
}

// ==========================================
// 数据解析与发送
// ==========================================

void MainWindow::tryParseFrame() {
  // 帧格式：[0xAA][0xBB][len_H][len_L][JSON][0xCC][0xDD]
  while (m_recvBuffer.size() >= 6) {
    // 寻找帧头 0xAA 0xBB
    int headerPos = -1;
    for (int i = 0; i <= m_recvBuffer.size() - 2; ++i) {
      if ((quint8)m_recvBuffer.at(i) == 0xAA &&
          (quint8)m_recvBuffer.at(i + 1) == 0xBB) {
        headerPos = i;
        break;
      }
    }

    if (headerPos == -1) {
      // 没找到完整的帧头，如果最后一个字节是 0xAA 则保留，防止被截断
      if (!m_recvBuffer.isEmpty() &&
          (quint8)m_recvBuffer.at(m_recvBuffer.size() - 1) == 0xAA) {
        m_recvBuffer = m_recvBuffer.right(1);
      } else {
        m_recvBuffer.clear();
      }
      return;
    }

    // 丢弃帧头前面的垃圾数据
    if (headerPos > 0) {
      m_recvBuffer.remove(0, headerPos);
    }

    if (m_recvBuffer.size() < 6)
      return; // 长度不够包含基础结构，继续等待

    // 读取长度（大端序）
    quint16 len =
        ((quint8)m_recvBuffer.at(2) << 8) | (quint8)m_recvBuffer.at(3);

    // 拒绝过大的帧（JSON 载荷不应超过 4096 字节）
    if (len > 4096) {
      appendLog(QString("Error: Frame too large (%1 bytes), dropping.").arg(len));
      m_recvBuffer.remove(0, 2); // 跳过当前帧头，继续搜索下一个
      continue;
    }

    // 检查是否已收到完整帧: 2(头) + 2(长度) + len + 2(尾) = len + 6
    int frameLen = len + 6;
    if (m_recvBuffer.size() < frameLen) {
      return; // 数据还没有收全，等待后续 TCP 包
    }

    // 校验帧尾 0xCC 0xDD
    if ((quint8)m_recvBuffer.at(frameLen - 2) == 0xCC &&
        (quint8)m_recvBuffer.at(frameLen - 1) == 0xDD) {
      // 提取完整的 JSON 数据段（直接使用 QByteArray 避免双重 UTF-8 转换）
      QByteArray jsonBytes = m_recvBuffer.mid(4, len);
      appendLog(QString("Recv JSON: %1").arg(QString::fromUtf8(jsonBytes)));
      parseReceivedData(jsonBytes);
    } else {
      appendLog("Error: Invalid frame tail, dropping data.");
    }

    // 移除已处理过的帧
    m_recvBuffer.remove(0, frameLen);
  }
}

void MainWindow::parseReceivedData(const QByteArray &data) {
  // 只要收到并成功解析了数据，就说明下位机在线
  m_robotOnlineLabel->setText("在线");
  m_robotOnlineLabel->setStyleSheet("color: #00ff00; font-weight: bold; background: transparent;");

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(data, &err);
  if (err.error != QJsonParseError::NoError) {
    appendLog(QString("JSON parse error: %1").arg(err.errorString()));
    return;
  }
  QJsonObject obj = doc.object();

  if (obj.contains("weight")) {
    double w = obj["weight"].toDouble();
    m_weightValue->setText(QString::number(w, 'f', 1));
  }
  if (obj.contains("speed")) {
    double s = obj["speed"].toDouble();
    m_speedValue->setText(QString::number(s, 'f', 1));
  }
  if (obj.contains("battery")) {
    int b = obj["battery"].toInt();
    b = qBound(0, b, 100);
    m_batteryBar->setValue(b);
    m_batteryLabel->setText(QString("%1%").arg(b));
  }
  if (obj.contains("light")) {
    m_lightValue->setText(QString::number(obj["light"].toInt()));
  }
  if (obj.contains("motor_status")) {
    int motorVal = obj["motor_status"].toInt();
    QString mStr = "未知";
    QString color = "#999";
    switch (motorVal) {
    case 0: mStr = "停止";      color = "#ffcc00"; break;
    case 1: mStr = "收割中";    color = "#00ff00"; break;
    case 2: mStr = "反转";      color = "#00d2ff"; break;
    case 3: mStr = "故障";      color = "#ff0000"; break;
    }
    m_motorStatusLabel->setText(mStr);
    m_motorStatusLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; background: transparent;").arg(color));
  }
  if (obj.contains("storage_full")) {
    if (obj["storage_full"].toBool()) {
      m_storageLabel->setText("【已满】");
      m_storageLabel->setStyleSheet(
          "color: #ff0000; font-weight: bold; background: transparent;");
    } else {
      m_storageLabel->setText("正常");
      m_storageLabel->setStyleSheet(
          "color: #00ff00; font-weight: bold; background: transparent;");
    }
  }
  if (obj.contains("device_id")) {
    QString devId = obj["device_id"].toString();
    m_deviceIdLabel->setText(devId);
    m_deviceIdLabel->setStyleSheet(
        "color: #00d2ff; font-weight: bold; background: transparent;");
  }
}

void MainWindow::sendCommand(const QString &cmd) {
  if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
    QByteArray data = cmd.toUtf8() + "\r\n";
    qint64 written = m_tcpSocket->write(data);
    if (written < 0) {
      appendLog(QString("Error: Failed to send '%1': %2")
                    .arg(cmd, m_tcpSocket->errorString()));
    } else if (written < data.size()) {
      appendLog(QString("Warning: Partial send '%1' (%2/%3 bytes)")
                    .arg(cmd).arg(written).arg(data.size()));
    } else {
      appendLog(QString("Send: %1").arg(cmd));
    }
  } else {
    appendLog("Error: Not connected. Cannot send data.");
  }
}

// ==========================================
// 控制按钮槽函数
// ==========================================

void MainWindow::onStartClicked() { sendCommand("CMD:START"); }

void MainWindow::onStopClicked() { sendCommand("CMD:STOP"); }

void MainWindow::onForwardClicked() { sendCommand("CMD:FORWARD"); }

void MainWindow::onBackwardClicked() { sendCommand("CMD:BACKWARD"); }

void MainWindow::onLeftClicked() { sendCommand("CMD:LEFT"); }

void MainWindow::onRightClicked() { sendCommand("CMD:RIGHT"); }

void MainWindow::onSpeedSliderMoved(int value) {
  m_speedSliderLabel->setText(QString("%1%").arg(value));
}

void MainWindow::onSpeedSliderReleased() {
  sendCommand(QString("CMD:SPEED:%1").arg(m_speedSlider->value()));
}
