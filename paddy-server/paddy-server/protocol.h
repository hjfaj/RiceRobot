#pragma once

#include <cstdint>
#include <string>


/**
 * @file protocol.h
 * @brief 稻谷收割机器人 ESP8266 <-> Ubuntu 通信协议定义
 *
 * 数据格式（JSON）示例：
 * {
 *   "weight": 12.5,          // 单位：kg，收割水稻重量
 *   "speed": 0.8,            // 单位：m/s，行驶速度
 *   "motor_status": 1,       // 0=停止, 1=正转, 2=反转, 3=故障
 *   "battery": 78,           // 单位：%，设备剩余电量
 *   "light": 450,            // 光照强度
 *   "storage_full": false,   // true=存储装满, false=未装满
 *   "device_id": "ESP_001"   // 设备编号（可选）
 * }
 *
 * 帧格式：
 *   [帧头 2字节: 0xAA 0xBB] [数据长度 2字节] [JSON数据] [帧尾 2字节: 0xCC 0xDD]
 */

// ===================== 帧协议常量 =====================
constexpr uint8_t FRAME_HEADER_1 = 0xAA;   // 帧头第1字节
constexpr uint8_t FRAME_HEADER_2 = 0xBB;   // 帧头第2字节
constexpr uint8_t FRAME_TAIL_1 = 0xCC;     // 帧尾第1字节
constexpr uint8_t FRAME_TAIL_2 = 0xDD;     // 帧尾第2字节
constexpr uint16_t MAX_PAYLOAD_LEN = 1024; // 最大JSON数据长度（字节）
constexpr int SERVER_PORT = 8888;          // ESP8266 数据上报端口
constexpr int QT_CLIENT_PORT = 8889;       // Qt 客户端订阅端口

// ===================== 电机状态枚举 =====================
enum class MotorStatus : int {
  STOPPED = 0,  // 停止
  FORWARD = 1,  // 正转（收割中）
  BACKWARD = 2, // 反转
  ERROR = 3     // 故障
};

// 将电机状态转为可读字符串
inline std::string motorStatusToString(MotorStatus s) {
  switch (s) {
  case MotorStatus::STOPPED:
    return "停止";
  case MotorStatus::FORWARD:
    return "正转(收割中)";
  case MotorStatus::BACKWARD:
    return "反转";
  case MotorStatus::ERROR:
    return "【故障】";
  default:
    return "未知";
  }
}

// ===================== 解析后的设备数据结构 =====================
struct RobotData {
  double weight = 0.0;                             // 收割水稻重量（kg）
  double speed = 0.0;                              // 行驶速度（m/s）
  MotorStatus motor_status = MotorStatus::STOPPED; // 电机状态
  int battery = 0;                                 // 剩余电量（%）
  int light = 0;                                   // 光照强度
  bool storage_full = false;                       // 存储空间是否装满
  std::string device_id = "";                      // 设备ID
  std::string raw_json = "";                       // 原始JSON字符串（用于调试）
};
