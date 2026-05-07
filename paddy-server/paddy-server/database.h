#pragma once

/**
 * @file database.h
 * @brief 稻谷收割机器人 数据库存储模块
 *
 * 功能：
 *   - 历史数据存储（传感器上报记录）
 *   - 设备状态日志（电机故障、电量报警等）
 *   - 数据统计查询（按小时/天聚合）
 *
 * 编译依赖：-lsqlite3
 */

#include <cstdint>
#include <string>
#include <vector>
#include "protocol.h"

// ===================== 数据结构定义 =====================

/// 传感器数据记录（用于存储和查询）
struct SensorRecord {
  int64_t id = 0;                     // 自增主键
  std::string timestamp = "";         // 上报时间 "YYYY-MM-DD HH:MM:SS"
  std::string device_id = "";         // 设备ID
  double weight = 0.0;                // 收割重量 (kg)
  int motor_status = 0;               // 电机状态 0-3
  int battery = 0;                    // 剩余电量 (%)
  bool storage_full = false;          // 存储是否装满
  std::string client_ip = "";         // 客户端IP:Port
};

/// 设备状态日志（用于报警和状态追踪）
struct StatusLog {
  int64_t id = 0;
  std::string timestamp = "";
  std::string device_id = "";
  std::string event_type = "";        // "MOTOR_ERROR", "LOW_BATTERY", "STORAGE_FULL", "CONNECTED", "DISCONNECTED"
  std::string message = "";           // 事件描述
};

/// 小时统计数据
struct HourlyStats {
  std::string hour = "";              // "YYYY-MM-DD HH:00:00"
  std::string device_id = "";
  int report_count = 0;               // 上报次数
  double total_weight = 0.0;          // 总收割重量
  int avg_battery = 0;                // 平均电量
  double max_weight = 0.0;            // 单次最大重量
};

/// 查询参数
struct QueryParams {
  std::string device_id = "";         // 设备ID过滤（空=全部）
  std::string start_time = "";        // 开始时间 "YYYY-MM-DD HH:MM:SS"
  std::string end_time = "";          // 结束时间
  int limit = 100;                    // 最大返回条数
  int offset = 0;                     // 分页偏移
};

// ===================== 数据库操作类 =====================

class Database {
public:
  Database();
  ~Database();

  /**
   * @brief 打开/创建数据库
   * @param db_path  数据库文件路径（如 "rice_robot.db"）
   * @return 成功返回 true
   */
  bool open(const std::string &db_path);

  /**
   * @brief 关闭数据库
   */
  void close();

  /**
   * @brief 检查数据库是否已打开
   */
  bool isOpen() const;

  // ─── 数据写入 ───

  /**
   * @brief 插入一条传感器数据记录
   * @param data       解析后的设备数据
   * @param client_ip  客户端IP地址
   * @return 成功返回 true
   */
  bool insertSensorData(const RobotData &data, const std::string &client_ip);

  /**
   * @brief 插入一条设备状态日志
   * @param device_id   设备ID
   * @param event_type  事件类型
   * @param message     事件描述
   * @return 成功返回 true
   */
  bool insertStatusLog(const std::string &device_id,
                       const std::string &event_type,
                       const std::string &message);

  // ─── 数据查询 ───

  /**
   * @brief 查询历史传感器数据
   * @param params  查询参数
   * @return 记录列表
   */
  std::vector<SensorRecord> querySensorData(const QueryParams &params);

  /**
   * @brief 查询设备状态日志
   * @param device_id   设备ID（空=全部）
   * @param start_time  开始时间
   * @param end_time    结束时间
   * @param limit       最大条数
   * @return 日志列表
   */
  std::vector<StatusLog> queryStatusLogs(const std::string &device_id,
                                         const std::string &start_time,
                                         const std::string &end_time,
                                         int limit = 50);

  /**
   * @brief 查询小时统计数据
   * @param device_id   设备ID（空=全部）
   * @param start_time  开始时间
   * @param end_time    结束时间
   * @return 统计列表
   */
  std::vector<HourlyStats> queryHourlyStats(const std::string &device_id,
                                             const std::string &start_time,
                                             const std::string &end_time);

  /**
   * @brief 获取设备最新状态
   * @param device_id  设备ID
   * @param out        输出记录（如果找到）
   * @return 是否找到
   */
  bool getLatestStatus(const std::string &device_id, SensorRecord &out);

  /**
   * @brief 获取所有已知设备列表
   * @return 设备ID列表
   */
  std::vector<std::string> getDeviceList();

  /**
   * @brief 获取数据统计摘要
   * @param device_id  设备ID（空=全部）
   * @param days       最近天数
   * @return 格式化的统计字符串
   */
  std::string getSummary(const std::string &device_id, int days = 7);

private:
  void *db_;  // sqlite3* 指针（避免头文件依赖）
  bool createTables();
};