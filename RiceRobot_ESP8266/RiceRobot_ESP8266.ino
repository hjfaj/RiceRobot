/*
 * ================================================================
 *  RiceRobot ESP8266 Arduino 固件
 * ================================================================
 *  功能说明：
 *    1. 自动连接 WiFi 路由器，断线自动重连（非阻塞）
 *    2. 以 TCP Client 模式连接后台服务器 (paddy-server)
 *    3. 上行透传：读取 STM32 串口数据 → 解析帧协议 → 转发完整帧到 TCP
 *    4. 下行透传：读取服务器 TCP 数据 → 原样转发到 STM32 串口
 *    5. GPIO2 LED 指示连接状态
 *
 *  帧协议：
 *    [帧头 0xAA 0xBB] [长度 2字节大端] [JSON 载荷] [帧尾 0xCC 0xDD]
 *
 *  硬件连接：
 *    ESP8266 RX  ← STM32 USART3 TX (PB10)
 *    ESP8266 TX  → STM32 USART3 RX (PB11)
 *    波特率: 115200
 *
 *  烧录方式：Arduino IDE，开发板选 "Generic ESP8266 Module"
 * ================================================================
 */

#include <ESP8266WiFi.h>

// ===================== 用户配置区 =====================
const char *WIFI_SSID = "Test";          // WiFi 路由器名称
const char *WIFI_PASSWORD = "88888888";  // WiFi 密码
const char *SERVER_IP = "113.45.231.111"; // paddy-server 服务器 IP
const uint16_t SERVER_PORT = 8888;       // paddy-server 端口

// ===================== 帧协议常量 =====================
#define FRAME_HEADER_1 0xAA
#define FRAME_HEADER_2 0xBB
#define FRAME_TAIL_1 0xCC
#define FRAME_TAIL_2 0xDD
#define MAX_FRAME_SIZE 1200 // 最大帧缓冲区大小(含帧头帧尾)

// ===================== 状态机枚举 =====================
enum FrameParseState {
  STATE_WAIT_HEADER1,  // 等待帧头第1字节 0xAA
  STATE_WAIT_HEADER2,  // 等待帧头第2字节 0xBB
  STATE_WAIT_LEN_HIGH, // 等待长度高字节
  STATE_WAIT_LEN_LOW,  // 等待长度低字节
  STATE_RECV_PAYLOAD,  // 接收 JSON 载荷
  STATE_WAIT_TAIL1,    // 等待帧尾第1字节 0xCC
  STATE_WAIT_TAIL2     // 等待帧尾第2字节 0xDD
};

// ===================== 全局变量 =====================
WiFiClient tcpClient;

// 帧解析相关
uint8_t frameBuf[MAX_FRAME_SIZE]; // 帧缓冲区
uint16_t frameIndex = 0;          // 当前写入位置
uint16_t payloadLen = 0;          // JSON 载荷长度
uint16_t payloadReceived = 0;     // 已接收载荷字节数
FrameParseState parseState = STATE_WAIT_HEADER1;

// 重连定时器
unsigned long lastWifiRetry = 0;
unsigned long lastTcpRetry = 0;
const unsigned long WIFI_RETRY_INTERVAL = 5000; // WiFi 重连间隔 5 秒
const unsigned long TCP_RETRY_INTERVAL = 3000;  // TCP 重连间隔 3 秒

// LED 引脚 (ESP8266 板载 LED 通常为 GPIO2，低电平点亮)
#define LED_PIN 2

// ===================== 函数声明 =====================
void connectWiFi(void);
void connectTCP(void);
void handleSerialData(void);
void handleTCPData(void);
void resetParser(void);
void sendFrameToTCP(void);

// ===================== 初始化 =====================
void setup() {
  // 初始化串口，波特率与 STM32 USART3 一致
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F(" RiceRobot ESP8266 固件 v1.0"));
  Serial.println(F("========================================"));

  // 初始化 LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // 熄灭 LED（低电平点亮）

  // 设置 WiFi 模式为 STA
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  // 启动 WiFi 连接
  Serial.print(F("[WiFi] 正在连接: "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // 阻塞等待首次 WiFi 连接（最多等 15 秒）
  uint8_t waitCount = 0;
  while (WiFi.status() != WL_CONNECTED && waitCount < 30) {
    delay(500);
    Serial.print(".");
    waitCount++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("[WiFi] 连接成功! IP: "));
    Serial.println(WiFi.localIP());
    // 尝试连接 TCP 服务器
    connectTCP();
  } else {
    Serial.println(F("[WiFi] 首次连接超时，将在主循环中重试"));
  }

  // 初始化帧解析状态
  resetParser();
}

// ===================== 主循环 =====================
void loop() {
  unsigned long now = millis();

  // --- WiFi 断线重连 ---
  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, HIGH); // 熄灭 LED
    if (now - lastWifiRetry >= WIFI_RETRY_INTERVAL) {
      lastWifiRetry = now;
      Serial.println(F("[WiFi] 断线，尝试重连..."));
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    return; // WiFi 未连接时跳过后续逻辑
  }

  // --- TCP 断线重连 ---
  if (!tcpClient.connected()) {
    digitalWrite(LED_PIN, HIGH); // 熄灭 LED
    if (now - lastTcpRetry >= TCP_RETRY_INTERVAL) {
      lastTcpRetry = now;
      connectTCP();
    }
  } else {
    digitalWrite(LED_PIN, LOW); // 点亮 LED，表示连接正常
  }

  // --- 上行：STM32 串口 → TCP 服务器 ---
  handleSerialData();

  // --- 下行：TCP 服务器 → STM32 串口 ---
  handleTCPData();
}

// ===================== WiFi 连接 =====================
void connectWiFi(void) {
  Serial.print(F("[WiFi] 正在连接: "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

// ===================== TCP 连接 =====================
void connectTCP(void) {
  Serial.print(F("[TCP] 正在连接: "));
  Serial.print(SERVER_IP);
  Serial.print(":");
  Serial.println(SERVER_PORT);

  if (tcpClient.connect(SERVER_IP, SERVER_PORT)) {
    Serial.println(F("[TCP] 连接成功!"));
    tcpClient.setNoDelay(true); // 关闭 Nagle 算法，减少延迟
  } else {
    Serial.println(F("[TCP] 连接失败，稍后重试"));
  }
}

// ===================== 帧解析状态机重置 =====================
void resetParser(void) {
  parseState = STATE_WAIT_HEADER1;
  frameIndex = 0;
  payloadLen = 0;
  payloadReceived = 0;
}

// ===================== 处理串口数据（上行透传） =====================
/*
 * 从 STM32 串口读取数据，使用状态机逐字节解析帧协议。
 * 只有完整接收到一帧（帧头 + 长度 + 载荷 + 帧尾）后才转发到 TCP，
 * 防止半帧数据发送导致服务器解析错误。
 */
void handleSerialData(void) {
  while (Serial.available()) {
    uint8_t byte = Serial.read();

    switch (parseState) {
    case STATE_WAIT_HEADER1:
      if (byte == FRAME_HEADER_1) {
        frameBuf[0] = byte;
        frameIndex = 1;
        parseState = STATE_WAIT_HEADER2;
      }
      break;

    case STATE_WAIT_HEADER2:
      if (byte == FRAME_HEADER_2) {
        frameBuf[1] = byte;
        frameIndex = 2;
        parseState = STATE_WAIT_LEN_HIGH;
      } else {
        // 帧头不匹配，回退重新查找
        resetParser();
        // 检查当前字节是否可能是帧头第1字节
        if (byte == FRAME_HEADER_1) {
          frameBuf[0] = byte;
          frameIndex = 1;
          parseState = STATE_WAIT_HEADER2;
        }
      }
      break;

    case STATE_WAIT_LEN_HIGH:
      frameBuf[2] = byte;
      payloadLen = (uint16_t)byte << 8;
      frameIndex = 3;
      parseState = STATE_WAIT_LEN_LOW;
      break;

    case STATE_WAIT_LEN_LOW:
      frameBuf[3] = byte;
      payloadLen |= byte;
      frameIndex = 4;
      payloadReceived = 0;

      // 校验载荷长度合法性
      if (payloadLen == 0 || payloadLen > (MAX_FRAME_SIZE - 6)) {
        Serial.println(F("[帧解析] 载荷长度非法，丢弃"));
        resetParser();
      } else {
        parseState = STATE_RECV_PAYLOAD;
      }
      break;

    case STATE_RECV_PAYLOAD:
      frameBuf[frameIndex++] = byte;
      payloadReceived++;
      if (payloadReceived >= payloadLen) {
        parseState = STATE_WAIT_TAIL1;
      }
      break;

    case STATE_WAIT_TAIL1:
      if (byte == FRAME_TAIL_1) {
        frameBuf[frameIndex++] = byte;
        parseState = STATE_WAIT_TAIL2;
      } else {
        // 帧尾不匹配，丢弃整帧
        Serial.println(F("[帧解析] 帧尾1不匹配，丢弃"));
        resetParser();
      }
      break;

    case STATE_WAIT_TAIL2:
      if (byte == FRAME_TAIL_2) {
        frameBuf[frameIndex++] = byte;
        // 完整帧接收完毕，发送到 TCP
        sendFrameToTCP();
      } else {
        Serial.println(F("[帧解析] 帧尾2不匹配，丢弃"));
      }
      resetParser();
      break;
    }
  }
}

// ===================== 发送完整帧到 TCP =====================
void sendFrameToTCP(void) {
  if (tcpClient.connected()) {
    size_t written = tcpClient.write(frameBuf, frameIndex);
    if (written == frameIndex) {
      Serial.print(F("[TCP→] 帧已发送, 大小: "));
      Serial.println(frameIndex);
    } else {
      Serial.println(F("[TCP→] 帧发送不完整!"));
    }
  } else {
    Serial.println(F("[TCP→] 未连接，帧已丢弃"));
  }
}

// ===================== 处理 TCP 数据（下行透传） =====================
/*
 * 从 TCP 服务器读取数据，原样透传到 STM32 串口。
 * 服务器可以通过 TCP 下发控制指令给 STM32。
 */
void handleTCPData(void) {
  while (tcpClient.available()) {
    uint8_t byte = tcpClient.read();
    Serial.write(byte);
  }
}
