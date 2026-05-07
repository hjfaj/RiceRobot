/**
 * @file mainwindow.cpp
 * @brief 水稻收割机器人监控终端 - 主窗口实现
 */

#include "mainwindow.h"
#include <QDebug>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
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
  m_weightValue->setText("0.0 KG");
  m_speedValue->setText("0.0 m/s");
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
  // 主窗口设置
  this->setWindowTitle("监控终端");
#ifdef Q_OS_ANDROID
  this->showMaximized();
#else
  this->resize(850, 650);
#endif

  // 中心部件
  QWidget *centralWidget = new QWidget(this);
  this->setCentralWidget(centralWidget);
  centralWidget->setObjectName("centralWidget");

  QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(20);

  // ==========================================
  // 1. 顶部标题栏
  // ==========================================
  QLabel *titleLabel =
      new QLabel("🌾 水稻收割机器人监控终端 (TCP Client)", this);
  titleLabel->setObjectName("headerTitle");
  titleLabel->setAlignment(Qt::AlignCenter);

  // ==========================================
  // 2. 连接控制栏
  // ==========================================
  QHBoxLayout *connLayout = new QHBoxLayout();
  QLabel *ipLabel = new QLabel("服务器地址:", this);
  m_ipEdit = new QLineEdit(this);
  m_ipEdit->setText("113.45.231.111");    // 公网服务器IP
  m_ipEdit->setPlaceholderText("请输入服务器IP或域名");
  m_ipEdit->setFixedWidth(200);

  QLabel *portLabel = new QLabel("端口:", this);
  m_portSpin = new QSpinBox(this);
  m_portSpin->setRange(1, 65535);
  m_portSpin->setValue(8889);  // Qt客户端连接8889端口
  m_portSpin->setFixedWidth(120);

  m_connectBtn = new QPushButton("连接", this);
  m_connectBtn->setObjectName("btnConnect");
  m_connectBtn->setFixedWidth(100);

  connLayout->addWidget(ipLabel);
  connLayout->addWidget(m_ipEdit);
  connLayout->addWidget(portLabel);
  connLayout->addWidget(m_portSpin);
  connLayout->addWidget(m_connectBtn);
  connLayout->addStretch();

  // ==========================================
  // 3. 中间区域：数据与控制区
  // ==========================================
#ifdef Q_OS_ANDROID
  QVBoxLayout *contentLayout = new QVBoxLayout(); // 手机竖屏使用上下布局
#else
  QHBoxLayout *contentLayout = new QHBoxLayout(); // 电脑使用左右布局
#endif
  contentLayout->setSpacing(20);

  // --- 数据监控区 ---
  QVBoxLayout *leftLayout = new QVBoxLayout();
  leftLayout->setSpacing(15);

  // 卡片1：载重
  QFrame *weightCard = new QFrame(this);
  weightCard->setObjectName("dataCardBlue");
  QVBoxLayout *weightLayout = new QVBoxLayout(weightCard);
  QLabel *wLabel = new QLabel("当前载重 (Weight)", this);
  wLabel->setObjectName("cardLabel");
  m_weightValue = new QLabel("0.0 KG", this);
  m_weightValue->setObjectName("lcdNumber");
  weightLayout->addWidget(wLabel);
  weightLayout->addWidget(m_weightValue);

  // 卡片2：行驶速度
  QFrame *speedCard = new QFrame(this);
  speedCard->setObjectName("dataCardBlue");
  QVBoxLayout *speedLayout = new QVBoxLayout(speedCard);
  QLabel *spLabel = new QLabel("行驶速度 (Speed)", this);
  spLabel->setObjectName("cardLabel");
  m_speedValue = new QLabel("0.0 m/s", this);
  m_speedValue->setObjectName("lcdNumber");
  speedLayout->addWidget(spLabel);
  speedLayout->addWidget(m_speedValue);

  // 卡片3：电量
  QFrame *batteryCard = new QFrame(this);
  batteryCard->setObjectName("dataCardYellow");
  QVBoxLayout *batteryLayout = new QVBoxLayout(batteryCard);
  QLabel *bLabel = new QLabel("电池电量 (Battery)", this);
  bLabel->setObjectName("cardLabel");

  m_batteryBar = new QProgressBar(this);
  m_batteryBar->setObjectName("batteryBar");
  m_batteryBar->setRange(0, 100);
  m_batteryBar->setTextVisible(false);

  m_batteryLabel = new QLabel("0%", this);
  m_batteryLabel->setObjectName("batteryText");
  m_batteryLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  batteryLayout->addWidget(bLabel);
  batteryLayout->addWidget(m_batteryBar);
  batteryLayout->addWidget(m_batteryLabel);

  // 卡片4：光照强度
  QFrame *lightCard = new QFrame(this);
  lightCard->setObjectName("dataCardYellow");
  QVBoxLayout *lightLayout = new QVBoxLayout(lightCard);
  QLabel *lLabel = new QLabel("光照强度 (Light)", this);
  lLabel->setObjectName("cardLabel");
  m_lightValue = new QLabel("0", this);
  m_lightValue->setObjectName("lcdNumber");
  lightLayout->addWidget(lLabel);
  lightLayout->addWidget(m_lightValue);

  // 卡片5：电机状态
  QFrame *motorCard = new QFrame(this);
  motorCard->setObjectName("dataCardGrey");
  QVBoxLayout *motorLayout = new QVBoxLayout(motorCard);
  QLabel *mLabel = new QLabel("电机状态 (Motor)", this);
  mLabel->setObjectName("cardLabel");
  m_motorStatusLabel = new QLabel("未知", this);
  m_motorStatusLabel->setObjectName("connStatusText");
  motorLayout->addWidget(mLabel);
  motorLayout->addWidget(m_motorStatusLabel);

  // 卡片6：存储仓状态
  QFrame *storageCard = new QFrame(this);
  storageCard->setObjectName("dataCardGrey");
  QVBoxLayout *storageLayout = new QVBoxLayout(storageCard);
  QLabel *sLabel = new QLabel("存储仓 (Storage)", this);
  sLabel->setObjectName("cardLabel");
  m_storageLabel = new QLabel("未知", this);
  m_storageLabel->setObjectName("connStatusText");
  storageLayout->addWidget(sLabel);
  storageLayout->addWidget(m_storageLabel);

  // 卡片7：设备编号
  QFrame *deviceCard = new QFrame(this);
  deviceCard->setObjectName("dataCardGrey");
  QVBoxLayout *deviceLayout = new QVBoxLayout(deviceCard);
  QLabel *dLabel = new QLabel("检测设备 (Device ID)", this);
  dLabel->setObjectName("cardLabel");
  m_deviceIdLabel = new QLabel("N/A", this);
  m_deviceIdLabel->setObjectName("connStatusText");
  deviceLayout->addWidget(dLabel);
  deviceLayout->addWidget(m_deviceIdLabel);

  // 卡片8：下位机在线状态
  QFrame *onlineCard = new QFrame(this);
  onlineCard->setObjectName("dataCardGrey");
  QVBoxLayout *onlineLayout = new QVBoxLayout(onlineCard);
  QLabel *oLabel = new QLabel("机器人状态 (Device Status)", this);
  oLabel->setObjectName("cardLabel");
  m_robotOnlineLabel = new QLabel("离线 (Offline)", this);
  m_robotOnlineLabel->setObjectName("connStatusText");
  onlineLayout->addWidget(oLabel);
  onlineLayout->addWidget(m_robotOnlineLabel);

  // 卡片9：连接状态
  QFrame *connCard = new QFrame(this);
  connCard->setObjectName("dataCardGrey");
  QVBoxLayout *cLayout = new QVBoxLayout(connCard);
  QLabel *cLabel = new QLabel("连接状态", this);
  cLabel->setObjectName("cardLabel");
  m_connStatusLabel = new QLabel("Disconnected", this);
  m_connStatusLabel->setObjectName("connStatusText");
  cLayout->addWidget(cLabel);
  cLayout->addWidget(m_connStatusLabel);

  leftLayout->addWidget(weightCard);
  leftLayout->addWidget(speedCard);
  leftLayout->addWidget(batteryCard);
  leftLayout->addWidget(lightCard);
  leftLayout->addWidget(motorCard);
  leftLayout->addWidget(storageCard);
  leftLayout->addWidget(deviceCard);
  leftLayout->addWidget(onlineCard);
  leftLayout->addWidget(connCard);
  leftLayout->addStretch();

  // --- 右侧：控制按钮区 ---
  QGridLayout *rightLayout = new QGridLayout();
  rightLayout->setSpacing(15);

  m_startBtn = new QPushButton("启动收割 (START)", this);
  m_startBtn->setObjectName("btnStart");
  m_startBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_forwardBtn = new QPushButton("前进", this);
  m_forwardBtn->setObjectName("btnFunc");
  m_forwardBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_backwardBtn = new QPushButton("后退", this);
  m_backwardBtn->setObjectName("btnFunc");
  m_backwardBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_leftBtn = new QPushButton("左转", this);
  m_leftBtn->setObjectName("btnFunc");
  m_leftBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_rightBtn = new QPushButton("右转", this);
  m_rightBtn->setObjectName("btnFunc");
  m_rightBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_stopBtn = new QPushButton("紧急停止 (STOP)", this);
  m_stopBtn->setObjectName("btnStop");
  m_stopBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  m_speedSlider = new QSlider(Qt::Horizontal, this);
  m_speedSlider->setObjectName("speedSlider");
  m_speedSlider->setRange(10, 100);
  m_speedSlider->setValue(30);
  m_speedSlider->setTickPosition(QSlider::TicksBelow);
  m_speedSlider->setTickInterval(10);
  m_speedSlider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  m_speedSliderLabel = new QLabel("速度: 30%", this);
  m_speedSliderLabel->setObjectName("speedSliderLabel");
  m_speedSliderLabel->setAlignment(Qt::AlignCenter);

  QHBoxLayout *motorSpeedLayout = new QHBoxLayout();
  motorSpeedLayout->addWidget(new QLabel("电机速度:", this));
  motorSpeedLayout->addWidget(m_speedSlider);
  motorSpeedLayout->addWidget(m_speedSliderLabel);

  rightLayout->addWidget(m_startBtn, 0, 0, 1, 2);
  rightLayout->addWidget(m_forwardBtn, 1, 0);
  rightLayout->addWidget(m_backwardBtn, 1, 1);
  rightLayout->addWidget(m_leftBtn, 2, 0);
  rightLayout->addWidget(m_rightBtn, 2, 1);
  rightLayout->addWidget(m_stopBtn, 3, 0, 1, 2);
  rightLayout->addLayout(motorSpeedLayout, 4, 0, 1, 2);

  // 设置两侧比例
  contentLayout->addLayout(leftLayout, 1);
  contentLayout->addLayout(rightLayout, 1);

  // ==========================================
  // 4. 底部日志区
  // ==========================================
  m_logArea = new QTextEdit(this);
  m_logArea->setObjectName("logArea");
  m_logArea->setReadOnly(true);
  m_logArea->setFixedHeight(150);

  // 主布局组装
  mainLayout->addWidget(titleLabel);
  mainLayout->addLayout(connLayout);
  mainLayout->addLayout(contentLayout, 1);
  mainLayout->addWidget(m_logArea);

  // 禁用控制按钮直到连接成功
  m_startBtn->setEnabled(false);
  m_stopBtn->setEnabled(false);
  m_forwardBtn->setEnabled(false);
  m_backwardBtn->setEnabled(false);
  m_leftBtn->setEnabled(false);
  m_rightBtn->setEnabled(false);

  appendLog("System initialized. Waiting for connection...");
}

void MainWindow::setupStyles() {
#ifdef Q_OS_ANDROID
  // 安卓版专属样式：放大字体、加高按钮等，以适配触控
  QString qss = R"(
        /* 全局背景和文本 */
        #centralWidget {
            background-color: #333333;
        }
        QLabel, QLineEdit, QSpinBox, QPushButton, QTextEdit {
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
            color: #ffffff;
        }

        /* 顶部标题栏 */
        #headerTitle {
            font-size: 26px;
            font-weight: bold;
            color: #00d2ff;
            border-bottom: 1px solid #555555;
            padding-bottom: 15px;
        }

        /* 输入框样式 */
        QLineEdit, QSpinBox {
            background-color: #222222;
            border: 1px solid #555555;
            border-radius: 6px;
            padding: 10px;
            font-size: 18px;
            color: #ffffff;
        }
        QLineEdit:hover, QSpinBox:hover {
            border: 1px solid #888888;
            background-color: #2a2a2a;
        }
        QLineEdit:focus, QSpinBox:focus {
            border: 1px solid #00d2ff;
            background-color: #2a2a2a;
        }

        /* 连接状态/小字 */
        QLabel { font-size: 16px; }
        
        /* 各种数据卡片共有样式 */
        QFrame {
            background-color: #444444;
            border-radius: 10px;
            padding: 15px;
        }
        #dataCardBlue { border-left: 8px solid #00d2ff; min-height: 100px; }
        #dataCardYellow { border-left: 8px solid #ffcc00; min-height: 100px; }
        #dataCardGrey { border-left: 8px solid #aaaaaa; min-height: 80px; }

        /* 卡片内标签 */
        #cardLabel {
            font-size: 16px;
            color: #aaaaaa;
            background: transparent;
            min-height: 24px;
        }

        /* LCD 大数字 */
        #lcdNumber {
            font-family: 'Microsoft YaHei', 'SimHei', 'Segoe UI', sans-serif;
            font-size: 48px;
            color: #00ff00;
            font-weight: bold;
            background: transparent;
            min-height: 70px;
        }

        /* 状态文本 */
        #connStatusText {
            color: #00d2ff;
            font-size: 20px;
            font-weight: bold;
            background: transparent;
            min-height: 30px;
        }
        #batteryText {
            color: #ffcc00;
            font-size: 20px;
            font-weight: bold;
            background: transparent;
            min-height: 30px;
        }

        /* 进度条 */
        QProgressBar {
            background-color: #222222;
            border: none;
            border-radius: 10px;
            height: 20px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #ffcc00, stop: 1 #ff6600);
            border-radius: 10px;
        }

        /* 按钮通用样式 */
        QPushButton {
            border: none;
            border-radius: 10px;
            font-size: 20px;
            font-weight: bold;
            color: white;
            padding: 20px;
        }
        QPushButton:disabled {
            background-color: #555555 !important;
            color: #888888;
        }

        /* 连接按钮 */
        #btnConnect {
            background-color: #555555;
            font-size: 18px;
            padding: 10px;
        }
        #btnConnect:pressed { background-color: #444444; padding-top: 12px; padding-bottom: 8px; }

        /* 控制按钮：启动 (绿) */
        #btnStart { background-color: #28a745; min-height: 80px; font-size: 24px; }
        #btnStart:pressed { background-color: #1e7e34; padding-top: 22px; padding-bottom: 18px; }

        /* 控制按钮：停止 (红) */
        #btnStop { background-color: #dc3545; min-height: 80px; font-size: 24px; }
        #btnStop:pressed { background-color: #bd2130; padding-top: 22px; padding-bottom: 18px; }

        /* 控制按钮：功能 (蓝) */
        #btnFunc { background-color: #007bff; min-height: 70px; }
        #btnFunc:pressed { background-color: #0062cc; padding-top: 22px; padding-bottom: 18px; }

        /* 速度滑块 */
        #speedSlider {
            background-color: #222222;
            height: 30px;
            border-radius: 5px;
        }
        #speedSlider::groove:horizontal {
            background: #444444;
            height: 10px;
            border-radius: 5px;
        }
        #speedSlider::handle:horizontal {
            background: #00d2ff;
            width: 30px;
            margin: -10px 0;
            border-radius: 15px;
        }
        #speedSlider::sub-page:horizontal {
            background: #00d2ff;
            border-radius: 5px;
        }
        #speedSliderLabel {
            color: #00ff00;
            font-size: 20px;
            font-weight: bold;
            min-width: 100px;
        }

        /* 日志区 */
        #logArea {
            background-color: #000000;
            color: #00ff00;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 16px;
            border: 1px solid #555555;
            border-radius: 6px;
        }
    )";
#else
  // 暗黑工业风 QSS
  QString qss = R"(
        /* 全局背景和文本 */
        #centralWidget {
            background-color: #333333;
        }
        QLabel, QLineEdit, QSpinBox, QPushButton, QTextEdit {
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
            color: #ffffff;
        }

        /* 顶部标题栏 */
        #headerTitle {
            font-size: 24px;
            font-weight: bold;
            color: #00d2ff;
            border-bottom: 1px solid #555555;
            padding-bottom: 10px;
        }

        /* 输入框样式 */
        QLineEdit, QSpinBox {
            background-color: #222222;
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 5px;
            color: #ffffff;
        }
        QLineEdit:hover, QSpinBox:hover {
            border: 1px solid #888888;
            background-color: #2a2a2a;
        }
        QLineEdit:focus, QSpinBox:focus {
            border: 1px solid #00d2ff;
            background-color: #2a2a2a;
        }

        /* 连接状态/小字 */
        QLabel { font-size: 14px; }
        
        /* 各种数据卡片共有样式 */
        QFrame {
            background-color: #444444;
            border-radius: 8px;
            padding: 10px;
        }
        #dataCardBlue { border-left: 5px solid #00d2ff; min-height: 80px; }
        #dataCardBlue:hover { background-color: #4a4a4a; border-left: 5px solid #33ddff; }
        #dataCardYellow { border-left: 5px solid #ffcc00; min-height: 80px; }
        #dataCardYellow:hover { background-color: #4a4a4a; border-left: 5px solid #ffdd44; }
        #dataCardGrey { border-left: 5px solid #aaaaaa; min-height: 60px; }
        #dataCardGrey:hover { background-color: #4a4a4a; border-left: 5px solid #cccccc; }

        /* 卡片内标签 */
        #cardLabel {
            font-size: 14px;
            color: #aaaaaa;
            background: transparent;
            min-height: 20px;
        }

        /* LCD 大数字 */
        #lcdNumber {
            font-family: 'Microsoft YaHei', 'SimHei', 'Segoe UI', sans-serif;
            font-size: 36px;
            color: #00ff00;
            font-weight: bold;
            background: transparent;
            min-height: 50px;
        }

        /* 状态文本 */
        #connStatusText {
            color: #00d2ff;
            font-size: 16px;
            font-weight: bold;
            background: transparent;
            min-height: 24px;
        }
        #batteryText {
            color: #ffcc00;
            font-size: 16px;
            font-weight: bold;
            background: transparent;
            min-height: 24px;
        }

        /* 进度条 */
        QProgressBar {
            background-color: #222222;
            border: none;
            border-radius: 8px;
            height: 16px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #ffcc00, stop: 1 #ff6600);
            border-radius: 8px;
        }

        /* 按钮通用样式 */
        QPushButton {
            border: none;
            border-radius: 8px;
            font-size: 18px;
            font-weight: bold;
            color: white;
            padding: 15px;
        }
        QPushButton:disabled {
            background-color: #555555 !important;
            color: #888888;
        }

        /* 连接按钮 */
        #btnConnect {
            background-color: #555555;
            font-size: 14px;
            padding: 5px;
        }
        #btnConnect:hover { background-color: #666666; }
        #btnConnect:pressed { background-color: #444444; padding-top: 6px; padding-bottom: 4px; }

        /* 控制按钮：启动 (绿) */
        #btnStart { background-color: #28a745; min-height: 50px; }
        #btnStart:hover { background-color: #218838; }
        #btnStart:pressed { background-color: #1e7e34; padding-top: 17px; padding-bottom: 13px; }

        /* 控制按钮：停止 (红) */
        #btnStop { background-color: #dc3545; min-height: 50px; }
        #btnStop:hover { background-color: #c82333; }
        #btnStop:pressed { background-color: #bd2130; padding-top: 17px; padding-bottom: 13px; }

        /* 控制按钮：功能 (蓝) */
        #btnFunc { background-color: #007bff; min-height: 50px; }
        #btnFunc:hover { background-color: #0069d9; }
        #btnFunc:pressed { background-color: #0062cc; padding-top: 17px; padding-bottom: 13px; }

        /* 速度滑块 */
        #speedSlider {
            background-color: #222222;
            height: 25px;
            border-radius: 5px;
        }
        #speedSlider::groove:horizontal {
            background: #444444;
            height: 8px;
            border-radius: 4px;
        }
        #speedSlider::handle:horizontal {
            background: #00d2ff;
            width: 20px;
            margin: -6px 0;
            border-radius: 10px;
        }
        #speedSlider::sub-page:horizontal {
            background: #00d2ff;
            border-radius: 4px;
        }
        #speedSliderLabel {
            color: #00ff00;
            font-size: 16px;
            font-weight: bold;
            min-width: 80px;
        }

        /* 日志区 */
        #logArea {
            background-color: #000000;
            color: #00ff00;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 13px;
            border: 1px solid #555555;
            border-radius: 4px;
        }
    )";
#endif

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
    m_connStatusLabel->setText(QString("Connected: %1").arg(info));
#ifdef Q_OS_ANDROID
    m_connStatusLabel->setStyleSheet(
        "color: #00d2ff; font-size: 20px; font-weight: bold; background: "
        "transparent;");
#else
    m_connStatusLabel->setStyleSheet(
        "color: #00d2ff; font-size: 16px; font-weight: bold; background: "
        "transparent;");
#endif
    m_connectBtn->setText("断开");
    m_ipEdit->setEnabled(false);
    m_portSpin->setEnabled(false);
  } else {
    m_connStatusLabel->setText(info.isEmpty() ? "Disconnected" : info);
#ifdef Q_OS_ANDROID
    m_connStatusLabel->setStyleSheet(
        "color: #aaaaaa; font-size: 20px; font-weight: bold; background: "
        "transparent;");
#else
    m_connStatusLabel->setStyleSheet(
        "color: #aaaaaa; font-size: 16px; font-weight: bold; background: "
        "transparent;");
#endif
    m_connectBtn->setText("连接");
    m_ipEdit->setEnabled(true);
    m_portSpin->setEnabled(true);
  }

  // 控制按钮使能状态
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
  m_robotOnlineLabel->setText("离线 (Offline)");
  m_robotOnlineLabel->setStyleSheet(
      "color: #aaaaaa; font-weight: bold; background: transparent;");

  m_weightValue->setText("0.0 KG");
  m_speedValue->setText("0.0 m/s");
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
  m_robotOnlineLabel->setText("在线 (Online)");
  m_robotOnlineLabel->setStyleSheet(
      "color: #00ff00; font-weight: bold; background: transparent;");

  QJsonParseError err;
  QJsonDocument doc = QJsonDocument::fromJson(data, &err);
  if (err.error != QJsonParseError::NoError) {
    appendLog(QString("JSON parse error: %1").arg(err.errorString()));
    return;
  }
  QJsonObject obj = doc.object();

  if (obj.contains("weight")) {
    double w = obj["weight"].toDouble();
    m_weightValue->setText(QString::number(w, 'f', 1) + " KG");
  }
  if (obj.contains("speed")) {
    double s = obj["speed"].toDouble();
    m_speedValue->setText(QString::number(s, 'f', 1) + " m/s");
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
    QString color = "#aaaaaa";
    switch (motorVal) {
    case 0: mStr = "停止";       color = "#ffcc00"; break;
    case 1: mStr = "正转(收割)"; color = "#00ff00"; break;
    case 2: mStr = "反转";       color = "#00d2ff"; break;
    case 3: mStr = "【故障】";   color = "#ff0000"; break;
    }
    m_motorStatusLabel->setText(mStr);
    m_motorStatusLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; background: transparent;")
            .arg(color));
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
  m_speedSliderLabel->setText(QString("速度: %1%").arg(value));
}

void MainWindow::onSpeedSliderReleased() {
  sendCommand(QString("CMD:SPEED:%1").arg(m_speedSlider->value()));
}
