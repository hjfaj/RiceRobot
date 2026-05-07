/**
 * @file rice_robot_server.cpp
 * @brief 稻谷收割机器人 Ubuntu 端 TCP 服务器
 *
 * 架构说明（双端口设计）：
 *   ┌────────────┐  TCP:8888   ┌──────────────────┐  TCP:8889  ┌───────────┐
 *   │  ESP8266   │ ──────────► │  Ubuntu 服务器    │ ─────────► │ Qt 客户端 │
 *   │ (数据上报) │             │ rice_robot_server │ (广播转发) │ (监控终端) │
 *   └────────────┘             └──────────────────┘            └───────────┘
 *                                     │
 *                               ┌─────▼─────┐
 *                               │  SQLite3  │
 *                               │ 数据库存储 │
 *                               └───────────┘
 *
 * 端口职责：
 *   8888 —— ESP8266 连接，主动上报传感器数据帧
 *   8889 —— Qt 客户端连接，被动订阅，服务器广播转发帧
 *
 * 转发格式：
 *   收到 ESP8266 的完整帧后，原帧（含帧头帧尾）直接广播给所有 Qt 客户端
 *   Qt 端使用相同帧解析器即可解包
 *
 * 编译：make
 * 运行：./rice_robot_server
 */

#include <atomic>
#include <csignal>
#include <cstring>
#include <ctime>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Linux Socket 头文件
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

// TCP KeepAlive 宏定义（兼容性）
#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE 4
#endif
#ifndef TCP_KEEPINTVL
#define TCP_KEEPINTVL 5
#endif
#ifndef TCP_KEEPCNT
#define TCP_KEEPCNT 6
#endif

#include "database.h"
#include "protocol.h"

// ===================== ANSI 颜色控制码 =====================
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_BOLD "\033[1m"

// ===================== 全局变量 =====================
std::mutex g_log_mutex;            // 日志写入互斥锁
std::ofstream g_log_file;          // 日志文件流
std::atomic<bool> g_running{true}; // 服务器运行标志

// ─── Qt 客户端列表（存储所有已连接的 Qt 客户端 fd） ───
std::set<int> g_qt_clients;    // Qt 客户端 socket fd 集合
std::mutex g_qt_clients_mutex; // 保护 g_qt_clients 的互斥锁

// ─── ESP8266 客户端列表（用于 shutdown 时关闭所有连接） ───
std::set<int> g_esp_clients;    // ESP8266 客户端 socket fd 集合
std::mutex g_esp_clients_mutex; // 保护 g_esp_clients 的互斥锁

// ─── 数据库 ───
Database g_database;           // SQLite3 数据库实例

// ===================== 工具函数 =====================

/**
 * @brief 获取当前时间字符串
 */
std::string getNow() {
  time_t now = time(nullptr);
  struct tm tm_buf;
  char buf[32];
  localtime_r(&now, &tm_buf);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
  return std::string(buf);
}

/**
 * @brief 打印带颜色的日志并写入文件（线程安全）
 */
void log(const std::string &level, const std::string &msg,
         const char *color = COLOR_RESET) {
  std::lock_guard<std::mutex> lock(g_log_mutex);
  std::string line = "[" + getNow() + "] [" + level + "] " + msg;
  std::cout << color << line << COLOR_RESET << "\n";
  if (g_log_file.is_open()) {
    g_log_file << line << "\n";
    g_log_file.flush();
  }
}

// ===================== JSON 简易解析器 =====================

bool parseDouble(const std::string &json, const std::string &key, double &out) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos)
    return false;
  pos = json.find(":", pos + search.size());
  if (pos == std::string::npos)
    return false;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
    pos++;
  try {
    size_t consumed = 0;
    out = std::stod(json.substr(pos), &consumed);
    return consumed > 0;
  } catch (...) {
    return false;
  }
}

bool parseInt(const std::string &json, const std::string &key, int &out) {
  double tmp;
  if (!parseDouble(json, key, tmp))
    return false;
  out = static_cast<int>(tmp);
  return true;
}

bool parseBool(const std::string &json, const std::string &key, bool &out) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos)
    return false;
  pos = json.find(":", pos + search.size());
  if (pos == std::string::npos)
    return false;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
    pos++;
  if (json.substr(pos, 4) == "true" &&
      (pos + 4 >= json.size() || json[pos + 4] == ',' ||
       json[pos + 4] == '}' || json[pos + 4] == ' ' ||
       json[pos + 4] == '\t' || json[pos + 4] == '\n' ||
       json[pos + 4] == '\r')) {
    out = true;
    return true;
  }
  if (json.substr(pos, 5) == "false" &&
      (pos + 5 >= json.size() || json[pos + 5] == ',' ||
       json[pos + 5] == '}' || json[pos + 5] == ' ' ||
       json[pos + 5] == '\t' || json[pos + 5] == '\n' ||
       json[pos + 5] == '\r')) {
    out = false;
    return true;
  }
  return false;
}

bool parseString(const std::string &json, const std::string &key,
                 std::string &out) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos)
    return false;
  pos = json.find(":", pos + search.size());
  if (pos == std::string::npos)
    return false;
  pos = json.find("\"", pos);
  if (pos == std::string::npos)
    return false;
  pos++;
  // 查找未转义的结束引号
  size_t end = pos;
  while (end < json.size()) {
    if (json[end] == '"') {
      break;
    }
    if (json[end] == '\\' && end + 1 < json.size()) {
      end++; // 跳过转义字符
    }
    end++;
  }
  if (end >= json.size())
    return false;
  out = json.substr(pos, end - pos);
  return true;
}

bool parseRobotData(const std::string &json, RobotData &data) {
  data.raw_json = json;
  if (!parseDouble(json, "weight", data.weight))
    return false;
  parseDouble(json, "speed", data.speed);       // 可选字段
  int motor_val = 0;
  if (!parseInt(json, "motor_status", motor_val))
    return false;
  if (motor_val < 0 || motor_val > 3)
    return false;
  data.motor_status = static_cast<MotorStatus>(motor_val);
  if (!parseInt(json, "battery", data.battery))
    return false;
  if (data.battery < 0)
    data.battery = 0;
  if (data.battery > 100)
    data.battery = 100;
  parseInt(json, "light", data.light);          // 可选字段
  if (!parseBool(json, "storage_full", data.storage_full))
    return false;
  parseString(json, "device_id", data.device_id);
  return true;
}

// ===================== Qt 客户端管理 =====================

/**
 * @brief 注册一个新的 Qt 客户端
 */
void addQtClient(int fd) {
  std::lock_guard<std::mutex> lock(g_qt_clients_mutex);
  g_qt_clients.insert(fd);
}

/**
 * @brief 移除一个 Qt 客户端
 */
void removeQtClient(int fd) {
  std::lock_guard<std::mutex> lock(g_qt_clients_mutex);
  g_qt_clients.erase(fd);
}

/**
 * @brief 将数据帧广播给所有已连接的 Qt 客户端
 *
 * 广播格式：与 ESP8266 上报格式完全相同
 *   [0xAA][0xBB][LEN_H][LEN_L][JSON...][0xCC][0xDD]
 * Qt 端使用相同的帧解析器即可解包。
 *
 * @param json  要转发的 JSON 字符串
 * @return      成功发送的客户端数量
 */
int broadcastToQtClients(const std::string &json) {
  // 构建完整数据帧
  uint16_t len = static_cast<uint16_t>(json.size());
  std::vector<uint8_t> frame;
  frame.reserve(6 + len);

  frame.push_back(FRAME_HEADER_1);
  frame.push_back(FRAME_HEADER_2);
  frame.push_back(static_cast<uint8_t>(len >> 8));
  frame.push_back(static_cast<uint8_t>(len & 0xFF));
  for (char c : json)
    frame.push_back(static_cast<uint8_t>(c));
  frame.push_back(FRAME_TAIL_1);
  frame.push_back(FRAME_TAIL_2);

  std::vector<int> broken; // 发现断开的客户端
  int sent_count = 0;

  // 复制 fd 列表后释放锁，避免持锁期间阻塞 send() 调用
  {
    std::lock_guard<std::mutex> lock(g_qt_clients_mutex);
    for (int fd : g_qt_clients) {
      // 循环 send 确保完整发送（处理部分 send 情况）
      size_t total_sent = 0;
      bool write_failed = false;
      while (total_sent < frame.size()) {
        ssize_t result = send(fd, frame.data() + total_sent,
                              frame.size() - total_sent, MSG_NOSIGNAL);
        if (result < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            break; // 缓冲区暂时满，跳过本次发送
          }
          broken.push_back(fd);
          write_failed = true;
          break;
        } else if (result == 0) {
          break;
        }
        total_sent += result;
      }
      if (!write_failed && total_sent > 0) {
        sent_count++;
      }
    }
  }

  // 清理断开的客户端（不在持锁期间调用 log，避免死锁）
  if (!broken.empty()) {
    std::lock_guard<std::mutex> lock(g_qt_clients_mutex);
    for (int fd : broken) {
      g_qt_clients.erase(fd);
    }
  }
  for (int fd : broken) {
    shutdown(fd, SHUT_RDWR);
    close(fd);
    log("INFO", "Qt客户端断开（广播时检测），fd=" + std::to_string(fd),
        COLOR_YELLOW);
  }

  return sent_count;
}

// ===================== 数据展示 =====================

std::string buildBatteryBar(int percent, int width = 20) {
  int filled = (percent * width) / 100;
  std::string bar = "[";
  for (int i = 0; i < width; i++)
    bar += (i < filled) ? "█" : "░";
  bar += "] " + std::to_string(percent) + "%";
  return bar;
}

void printRobotData(const RobotData &data, const std::string &client_ip) {
  // 先查询 Qt 客户端数量（在锁 g_log_mutex 之前），防止与 broadcastToQtClients
  // 中 log() 调用形成锁顺序死锁
  size_t qt_count;
  {
    std::lock_guard<std::mutex> lock(g_qt_clients_mutex);
    qt_count = g_qt_clients.size();
  }

  std::lock_guard<std::mutex> lock(g_log_mutex);

  const char *battery_color = COLOR_GREEN;
  if (data.battery <= 20)
    battery_color = COLOR_RED;
  else if (data.battery <= 40)
    battery_color = COLOR_YELLOW;

  const char *motor_color = COLOR_GREEN;
  if (data.motor_status == MotorStatus::ERROR)
    motor_color = COLOR_RED;
  if (data.motor_status == MotorStatus::STOPPED)
    motor_color = COLOR_YELLOW;

  const char *storage_color = data.storage_full ? COLOR_RED : COLOR_GREEN;

  std::cout << "\n"
            << COLOR_BOLD << COLOR_CYAN
            << "╔══════════════════════════════════════╗\n"
            << "║       稻谷收割机器人 数据上报         ║\n"
            << "╚══════════════════════════════════════╝" << COLOR_RESET
            << "\n";
  std::cout << COLOR_CYAN << "  来源设备 : " << COLOR_RESET
            << (data.device_id.empty()
                    ? client_ip
                    : data.device_id + " (" + client_ip + ")")
            << "\n";
  std::cout << COLOR_CYAN << "  上报时间 : " << COLOR_RESET << getNow() << "\n";
  std::cout << COLOR_CYAN << "  Qt客户端 : " << COLOR_RESET << COLOR_MAGENTA
            << qt_count << " 台已连接" << COLOR_RESET << "\n";
  std::cout << COLOR_CYAN << "  ───────────────────────────────────\n"
            << COLOR_RESET;
  std::cout << COLOR_BOLD << "  收割重量 : " << COLOR_RESET << std::fixed
            << std::setprecision(2) << data.weight << " kg\n";
  std::cout << COLOR_BOLD << "  行驶速度 : " << COLOR_RESET << std::fixed
            << std::setprecision(2) << data.speed << " m/s\n";
  std::cout << COLOR_BOLD << "  电机状态 : " << COLOR_RESET << motor_color
            << motorStatusToString(data.motor_status) << COLOR_RESET << "\n";
  std::cout << COLOR_BOLD << "  剩余电量 : " << COLOR_RESET << battery_color
            << buildBatteryBar(data.battery) << COLOR_RESET << "\n";
  std::cout << COLOR_BOLD << "  光照强度 : " << COLOR_RESET << data.light << "\n";
  std::cout << COLOR_BOLD << "  存储空间 : " << COLOR_RESET << storage_color
            << (data.storage_full ? "【已装满！请及时清空】" : "正常（未装满）")
            << COLOR_RESET << "\n\n";

  if (g_log_file.is_open()) {
    g_log_file << "[" << getNow() << "] "
               << "设备="
               << (data.device_id.empty() ? client_ip : data.device_id)
               << " 重量=" << std::fixed << std::setprecision(2) << data.weight
               << "kg"
               << " 速度=" << std::fixed << std::setprecision(2) << data.speed
               << "m/s"
               << " 电机=" << motorStatusToString(data.motor_status)
               << " 电量=" << data.battery << "%"
               << " 光照=" << data.light
               << " 存储=" << (data.storage_full ? "已满" : "正常")
               << " Qt客户端=" << qt_count << "台\n";
    g_log_file.flush();
  }
}

// ===================== 帧解析状态机 =====================

enum class FrameState {
  WAIT_HEADER_1,
  WAIT_HEADER_2,
  WAIT_LEN_HIGH,
  WAIT_LEN_LOW,
  RECV_PAYLOAD,
  WAIT_TAIL_1,
  WAIT_TAIL_2
};

struct FrameParser {
  FrameState state = FrameState::WAIT_HEADER_1;
  uint16_t payload_len = 0;
  std::vector<char> payload;

  std::string feed(uint8_t byte) {
    switch (state) {
    case FrameState::WAIT_HEADER_1:
      if (byte == FRAME_HEADER_1)
        state = FrameState::WAIT_HEADER_2;
      break;
    case FrameState::WAIT_HEADER_2:
      state = (byte == FRAME_HEADER_2) ? FrameState::WAIT_LEN_HIGH
                                       : FrameState::WAIT_HEADER_1;
      break;
    case FrameState::WAIT_LEN_HIGH:
      payload_len = static_cast<uint16_t>(byte) << 8;
      state = FrameState::WAIT_LEN_LOW;
      break;
    case FrameState::WAIT_LEN_LOW:
      payload_len |= byte;
      if (payload_len == 0 || payload_len > MAX_PAYLOAD_LEN) {
        state = FrameState::WAIT_HEADER_1;
      } else {
        payload.clear();
        payload.reserve(payload_len);
        state = FrameState::RECV_PAYLOAD;
      }
      break;
    case FrameState::RECV_PAYLOAD:
      payload.push_back(static_cast<char>(byte));
      if (payload.size() == payload_len)
        state = FrameState::WAIT_TAIL_1;
      break;
    case FrameState::WAIT_TAIL_1:
      state = (byte == FRAME_TAIL_1) ? FrameState::WAIT_TAIL_2
                                     : FrameState::WAIT_HEADER_1;
      break;
    case FrameState::WAIT_TAIL_2:
      state = FrameState::WAIT_HEADER_1;
      if (byte == FRAME_TAIL_2)
        return std::string(payload.begin(), payload.end());
      break;
    }
    return "";
  }
};

// ===================== ESP8266 客户端处理 =====================

/**
 * @brief 处理 ESP8266 上报连接的线程函数
 * 解析数据帧 → 打印到终端 → 广播给所有 Qt 客户端
 */
void addEspClient(int fd) {
  std::lock_guard<std::mutex> lock(g_esp_clients_mutex);
  g_esp_clients.insert(fd);
}

void removeEspClient(int fd) {
  std::lock_guard<std::mutex> lock(g_esp_clients_mutex);
  g_esp_clients.erase(fd);
}

void handleEsp8266Client(int client_fd, const std::string &client_ip) {
  log("INFO", "[ESP8266] 设备已连接: " + client_ip, COLOR_GREEN);
  addEspClient(client_fd);

  FrameParser parser;
  uint8_t buffer[512];

  while (g_running) {
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read == 0) {
      log("INFO", "[ESP8266] 设备断开连接: " + client_ip, COLOR_YELLOW);
      break;
    } else if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      log("ERROR", "[ESP8266] recv错误: " + std::string(strerror(errno)),
          COLOR_RED);
      break;
    }

    for (ssize_t i = 0; i < bytes_read; i++) {
      std::string json = parser.feed(buffer[i]);
      if (json.empty())
        continue;

      // 1. 解析 JSON
      RobotData data;
      if (parseRobotData(json, data)) {
        printRobotData(data, client_ip);

        // 2. 存入数据库
        g_database.insertSensorData(data, client_ip);

        // 3. 报警检测并记录状态日志
        if (data.motor_status == MotorStatus::ERROR) {
          log("WARN", "⚠  电机故障！设备: " + client_ip, COLOR_RED);
          g_database.insertStatusLog(data.device_id, "MOTOR_ERROR",
                                     "电机故障，当前状态=" +
                                         motorStatusToString(data.motor_status));
        }
        if (data.storage_full) {
          log("WARN", "⚠  存储空间已满！设备: " + client_ip, COLOR_RED);
          g_database.insertStatusLog(data.device_id, "STORAGE_FULL",
                                     "存储仓已装满，重量=" +
                                         std::to_string(data.weight) + "kg");
        }
        if (data.battery <= 20) {
          log("WARN",
              "⚠  电量严重不足(" + std::to_string(data.battery) + "%)！",
              COLOR_RED);
          g_database.insertStatusLog(data.device_id, "LOW_BATTERY",
                                     "电量低于20%，当前=" +
                                         std::to_string(data.battery) + "%");
        }

        // 4. 转发给所有 Qt 客户端
        int qt_count = broadcastToQtClients(json);
        if (qt_count > 0) {
          log("INFO", "已转发给 " + std::to_string(qt_count) + " 个Qt客户端",
              COLOR_MAGENTA);
        }
      } else {
        log("ERROR", "[ESP8266] JSON解析失败: " + json, COLOR_RED);
      }
    }
  }

  removeEspClient(client_fd);
  close(client_fd);
}

// ===================== Qt 客户端处理 =====================

/**
 * @brief 处理 Qt 客户端连接的线程函数
 * Qt 客户端只负责接收广播，此线程主要用于检测连接是否断开。
 * 如果 Qt 客户端发来命令（如控制指令），可在此处扩展解析。
 */
void handleQtClient(int client_fd, const std::string &client_ip) {
  log("INFO", "[Qt客户端] 已连接: " + client_ip, COLOR_MAGENTA);
  addQtClient(client_fd);

  uint8_t buffer[256];

  while (g_running) {
    // 监听 Qt 客户端是否断开（或发来控制命令）
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes_read == 0) {
      // Qt 客户端主动断开
      break;
    } else if (bytes_read < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        continue;
      break;
    }
    // TODO: 如需处理 Qt 客户端发来的控制命令，在此解析 buffer
    // 例如：电机启停、紧急停止等指令
  }

  removeQtClient(client_fd);
  close(client_fd);
  log("INFO", "[Qt客户端] 已断开: " + client_ip, COLOR_YELLOW);
}

// ===================== Accept 线程工厂 =====================

/**
 * @brief 通用的 accept 循环线程
 * @param server_fd    监听 socket fd
 * @param port         端口号（仅用于日志）
 * @param handler      每个新连接调用的处理函数
 */
void acceptLoop(int server_fd, int port,
                std::function<void(int, const std::string &)> handler) {
  log("INFO", "监听端口 " + std::to_string(port) + " 就绪", COLOR_GREEN);

  while (g_running) {
    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    timeval tv{1, 0};

    int ready = select(server_fd + 1, &read_fds, nullptr, nullptr, &tv);
    if (ready <= 0)
      continue;

    int client_fd = accept(
        server_fd, reinterpret_cast<sockaddr *>(&client_addr), &client_len);
    if (client_fd < 0) {
      if (g_running)
        log("ERROR",
            "accept()失败(端口" + std::to_string(port) +
                "): " + std::string(strerror(errno)),
            COLOR_RED);
      continue;
    }

    // 设置接收超时 2 秒
    timeval recv_timeout{2, 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout,
               sizeof(recv_timeout));

    // ================== 公网适配优化 ==================
    // 开启 TCP KeepAlive（保活机制），防止公网环境下因 NAT
    // 超时或网络波动导致长连接变成死连接 (僵尸连接)
    int keepAlive = 1;
    setsockopt(client_fd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive,
               sizeof(keepAlive));
    
    // 设置激进的 KeepAlive 参数（快速检测断开）
    int keepIdle = 30;     // 空闲 30 秒后开始探测
    int keepInterval = 10; // 每 10 秒探测一次
    int keepCount = 3;     // 探测 3 次失败则断开
    
    setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
    setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
    setsockopt(client_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));
    
    log("INFO", "KeepAlive已启用: " + std::to_string(keepIdle) + "秒空闲后探测, " + 
        std::to_string(keepInterval) + "秒间隔, " + std::to_string(keepCount) + "次失败断开", COLOR_GREEN);
    // ===============================================

    // 获取客户端 IP:Port 字符串
    char ip_buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, ip_buf, sizeof(ip_buf));
    std::string client_ip =
        std::string(ip_buf) + ":" + std::to_string(ntohs(client_addr.sin_port));

    try {
      std::thread(handler, client_fd, client_ip).detach();
    } catch (const std::system_error &e) {
      log("ERROR", "无法创建线程: " + std::string(e.what()) +
          "，关闭连接 fd=" + std::to_string(client_fd), COLOR_RED);
      close(client_fd);
    }
  }
}

/**
 * @brief 创建一个 TCP 监听 socket
 * @param port   要监听的端口
 * @return socket fd，失败返回 -1
 */
int createListenSocket(int port) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    log("ERROR", "socket()创建失败: " + std::string(strerror(errno)),
        COLOR_RED);
    return -1;
  }
  int opt = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    log("ERROR",
        "bind()失败(端口" + std::to_string(port) +
            "): " + std::string(strerror(errno)),
        COLOR_RED);
    close(fd);
    return -1;
  }
  if (listen(fd, 5) < 0) {
    log("ERROR", "listen()失败: " + std::string(strerror(errno)), COLOR_RED);
    close(fd);
    return -1;
  }
  return fd;
}

// ===================== 信号处理 =====================

void signalHandler(int) {
  g_running = false; // 仅设置原子变量，异步信号安全
}

// ===================== 主函数 =====================

int main() {
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  // 打开日志文件
  g_log_file.open("robot_data.log", std::ios::app);
  if (!g_log_file.is_open())
    std::cerr << COLOR_RED << "警告：无法打开日志文件" << COLOR_RESET << "\n";

  // 打开数据库
  if (!g_database.open("rice_robot.db")) {
    log("ERROR", "无法打开数据库，服务器将继续运行但不会存储数据", COLOR_RED);
  } else {
    // 记录本次启动
    g_database.insertStatusLog("SERVER", "SERVER_STARTED", "服务器启动");
  }

  log("INFO", "═══════════════════════════════════════════════════",
      COLOR_CYAN);
  log("INFO", " 稻谷收割机器人 Ubuntu TCP 服务器 启动中...         ",
      COLOR_CYAN);
  log("INFO", "═══════════════════════════════════════════════════",
      COLOR_CYAN);

  // ─── 创建两个监听 socket ───
  int esp_server_fd = createListenSocket(SERVER_PORT);
  int qt_server_fd = createListenSocket(QT_CLIENT_PORT);

  if (esp_server_fd < 0 || qt_server_fd < 0) {
    log("ERROR", "服务器启动失败，端口创建错误", COLOR_RED);
    if (esp_server_fd >= 0)
      close(esp_server_fd);
    if (qt_server_fd >= 0)
      close(qt_server_fd);
    return EXIT_FAILURE;
  }

  log("INFO", "ESP8266 上报端口: " + std::to_string(SERVER_PORT), COLOR_GREEN);
  log("INFO", "Qt 客户端订阅端口: " + std::to_string(QT_CLIENT_PORT),
      COLOR_MAGENTA);
  log("INFO", "按 Ctrl+C 退出服务器", COLOR_YELLOW);

  // ─── 启动 Qt 客户端 accept 线程 ───
  std::thread qt_accept_thread(
      acceptLoop, qt_server_fd, QT_CLIENT_PORT,
      std::function<void(int, const std::string &)>(handleQtClient));

  // ─── 主线程：ESP8266 accept 循环 ───
  acceptLoop(
      esp_server_fd, SERVER_PORT,
      std::function<void(int, const std::string &)>(handleEsp8266Client));

  // ─── 清理 ───
  log("INFO", "收到退出信号，服务器正在关闭...", COLOR_YELLOW);
  qt_accept_thread.join();
  close(esp_server_fd);
  close(qt_server_fd);

  // 关闭所有还在线的 ESP8266 客户端（强制其处理线程退出 recv）
  {
    std::lock_guard<std::mutex> lock(g_esp_clients_mutex);
    for (int fd : g_esp_clients)
      close(fd);
    g_esp_clients.clear();
  }

  // 关闭所有还在线的 Qt 客户端
  {
    std::lock_guard<std::mutex> lock(g_qt_clients_mutex);
    for (int fd : g_qt_clients)
      close(fd);
    g_qt_clients.clear();
  }

  // 等待 detached 线程完成清理（recv 超时 2 秒 + 处理时间）
  std::this_thread::sleep_for(std::chrono::seconds(3));

  log("INFO", "服务器已正常关闭。日志保存在 robot_data.log", COLOR_CYAN);
  if (g_database.isOpen()) {
    g_database.insertStatusLog("SERVER", "SERVER_STOPPED", "服务器正常关闭");
    g_database.close();
  }
  if (g_log_file.is_open())
    g_log_file.close();

  return EXIT_SUCCESS;
}
