/**
 * @file esp8266_sender.ino
 * @brief ESP8266 数据发送端 —— 稻谷收割机器人
 *
 * 功能：
 *   - 连接 WiFi，与 Ubuntu 服务器建立 TCP 连接
 *   - 每隔 2 秒将传感器数据打包成帧发送给服务器
 *   - 数据包含：收割重量、电机状态、剩余电量、存储是否装满
 *
 * 使用方法：
 *   1. 修改下方 WIFI_SSID / WIFI_PASS / SERVER_IP / SERVER_PORT
 *   2. 上传到 ESP8266 开发板（NodeMCU / Wemos D1 Mini 均可）
 *   3. 打开串口监视器（波特率 115200）查看连接状态
 *
 * 帧格式（与 Ubuntu 端 protocol.h 保持一致）：
 *   [0xAA][0xBB][len_high][len_low][JSON...][0xCC][0xDD]
 */

#include <ESP8266WiFi.h>

// ==================== 用户配置区域 ====================
const char *WIFI_SSID = "你的WiFi名称";  // ← 改成你的 WiFi SSID
const char *WIFI_PASS = "你的WiFi密码";  // ← 改成你的 WiFi 密码
const char *SERVER_IP = "192.168.1.100"; // ← 改成 Ubuntu 服务器的 IP 地址
const int SERVER_PORT = 8888;            // 与服务器端口保持一致
const char *DEVICE_ID = "ESP_RICE_001";  // 设备唯一编号
// =====================================================

// 发送间隔（毫秒）
const unsigned long SEND_INTERVAL_MS = 2000;

WiFiClient client; // TCP 客户端对象
unsigned long lastSendTime = 0;

// ====================
// 硬件接口（模拟，请替换为实际读取函数）====================

/**
 * @brief 获取收割水稻重量（kg）
 * 实际项目中应接入称重传感器（如 HX711 + 压力传感器）
 */
float getWeight() {
  // TODO: 替换为实际传感器读取代码
  // 示例：return hx711.getUnits(10) / 1000.0;
  return 12.5 + random(0, 100) / 10.0; // 模拟数据
}

/**
 * @brief 获取电机状态
 * 返回值：0=停止, 1=正转, 2=反转, 3=故障
 */
int getMotorStatus() {
  // TODO: 替换为实际电机状态检测
  // 示例：digitalRead(MOTOR_FAULT_PIN) == LOW ? 3 : isRunning ? 1 : 0
  return 1; // 模拟：正转运行中
}

/**
 * @brief 获取设备剩余电量（%）
 */
int getBatteryPercent() {
  // TODO: 替换为实际 ADC 电量检测
  // 示例：map(analogRead(A0), 600, 1024, 0, 100)
  return 75;
}

/**
 * @brief 检测存储仓是否装满
 */
bool isStorageFull() {
  // TODO: 替换为实际限位开关或超声波传感器检测
  // 示例：digitalRead(STORAGE_LIMIT_PIN) == HIGH
  return false;
}

// ==================== 帧打包与发送 ====================

/**
 * @brief 构建 JSON 字符串
 * @param weight        重量（kg）
 * @param motor_status  电机状态（0-3）
 * @param battery       电量（0-100）
 * @param storage_full  存储是否装满
 * @return JSON 字符串
 */
String buildJson(float weight, int motor_status, int battery,
                 bool storage_full) {
  String json = "{";
  json += "\"weight\":" + String(weight, 2) + ",";
  json += "\"motor_status\":" + String(motor_status) + ",";
  json += "\"battery\":" + String(battery) + ",";
  json += "\"storage_full\":" + String(storage_full ? "true" : "false") + ",";
  json += "\"device_id\":\"" + String(DEVICE_ID) + "\"";
  json += "}";
  return json;
}

/**
 * @brief 将 JSON 打包成协议帧并通过 TCP 发送
 * 帧格式：[0xAA][0xBB][len_H][len_L][JSON字节...][0xCC][0xDD]
 * @param json  要发送的 JSON 字符串
 * @return 发送成功返回 true
 */
bool sendFrame(const String &json) {
  if (!client.connected())
    return false;

  uint16_t len = json.length();
  uint8_t header[] = {0xAA, 0xBB,
                      (uint8_t)(len >> 8), (uint8_t)(len & 0xFF)};
  uint8_t tail[] = {0xCC, 0xDD};

  // 写入帧头+长度 (4字节)
  if (client.write(header, sizeof(header)) != sizeof(header))
    return false;

  // 写入 JSON 数据
  if (client.print(json) != len)
    return false;

  // 写入帧尾 (2字节)
  if (client.write(tail, sizeof(tail)) != sizeof(tail))
    return false;

  return true;
}

// ==================== WiFi 连接 ====================

void connectWiFi() {
  Serial.print("[WiFi] 正在连接: ");
  Serial.println(WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] 连接成功！IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] 连接失败，1分钟后重试...");
    delay(60000);
    ESP.restart();
  }
}

// ==================== TCP 连接 ====================

void connectServer() {
  Serial.print("[TCP] 正在连接服务器 ");
  Serial.print(SERVER_IP);
  Serial.print(":");
  Serial.println(SERVER_PORT);

  int attempts = 0;
  while (!client.connect(SERVER_IP, SERVER_PORT) && attempts < 5) {
    Serial.println("[TCP] 连接失败，2秒后重试...");
    delay(2000);
    attempts++;
  }

  if (client.connected()) {
    Serial.println("[TCP] 服务器连接成功！");
  } else {
    Serial.println("[TCP] 无法连接服务器，重启中...");
    delay(5000);
    ESP.restart();
  }
}

// ==================== Arduino 主循环 ====================

void setup() {
  Serial.begin(115200);
  delay(100);
  randomSeed(analogRead(0)); // 用浮空 ADC 引脚初始化随机种子
  Serial.println("\n\n=== 稻谷收割机器人 ESP8266 启动 ===");

  // 连接 WiFi 和服务器
  connectWiFi();
  connectServer();
}

void loop() {
  // 检查 WiFi 是否断开
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] 连接断开，正在重连...");
    connectWiFi();
  }

  // 检查 TCP 连接是否断开
  if (!client.connected()) {
    Serial.println("[TCP] 连接断开，正在重连...");
    connectServer();
    return;
  }

  // 按间隔定时发送数据
  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = now;

    // 读取传感器数据
    float weight = getWeight();
    int motor_status = getMotorStatus();
    int battery = getBatteryPercent();
    bool storage_full = isStorageFull();

    // 构建 JSON 并打包发送
    String json = buildJson(weight, motor_status, battery, storage_full);
    bool ok = sendFrame(json);

    Serial.print("[发送] ");
    Serial.print(ok ? "成功 " : "失败 ");
    Serial.println(json);
  }
}
