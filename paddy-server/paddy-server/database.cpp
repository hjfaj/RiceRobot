/**
 * @file database.cpp
 * @brief 稻谷收割机器人 数据库存储模块实现
 *
 * 使用SQLite3存储传感器数据和设备状态日志
 */

#include "database.h"
#include <sqlite3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

// ===================== 辅助函数 =====================

/**
 * @brief 获取当前时间字符串
 * @return "YYYY-MM-DD HH:MM:SS" 格式的时间
 */
static std::string getCurrentTimestamp() {
  auto now = std::time(nullptr);
  struct tm tm_buf;
  localtime_r(&now, &tm_buf);
  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

/**
 * @brief 执行SQL语句（无返回结果）
 * @param db   SQLite3数据库指针
 * @param sql  SQL语句
 * @return 成功返回true
 */
static bool executeSQL(sqlite3 *db, const std::string &sql) {
  char *errMsg = nullptr;
  int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "[DB] SQL Error: " << errMsg << "\n  SQL: " << sql << std::endl;
    sqlite3_free(errMsg);
    return false;
  }
  return true;
}

/**
 * @brief 转义SQL字符串（防止SQL注入）
 */
static std::string escapeSQL(const std::string &str) {
  std::string result;
  result.reserve(str.size() * 2);
  for (char c : str) {
    if (c == '\'') {
      result += "''";
    } else {
      result += c;
    }
  }
  return result;
}

// ===================== Database类实现 =====================

Database::Database() : db_(nullptr) {}

Database::~Database() {
  close();
}

bool Database::open(const std::string &db_path) {
  if (db_) {
    std::cerr << "[DB] Database already opened" << std::endl;
    return false;
  }

  int rc = sqlite3_open(db_path.c_str(), reinterpret_cast<sqlite3 **>(&db_));
  if (rc != SQLITE_OK) {
    std::cerr << "[DB] Cannot open database: " << sqlite3_errmsg(reinterpret_cast<sqlite3 *>(db_)) << std::endl;
    sqlite3_close(reinterpret_cast<sqlite3 *>(db_));
    db_ = nullptr;
    return false;
  }

  // 启用WAL模式（提高并发性能）
  executeSQL(reinterpret_cast<sqlite3 *>(db_), "PRAGMA journal_mode=WAL;");
  // 设置同步模式为NORMAL（平衡性能和安全）
  executeSQL(reinterpret_cast<sqlite3 *>(db_), "PRAGMA synchronous=NORMAL;");
  // 设置 busy timeout，防止并发写入时 SQLITE_BUSY 导致数据静默丢失
  sqlite3_busy_timeout(reinterpret_cast<sqlite3 *>(db_), 5000);

  // 创建表
  if (!createTables()) {
    close();
    return false;
  }

  std::cout << "[DB] Database opened: " << db_path << std::endl;
  return true;
}

void Database::close() {
  if (db_) {
    sqlite3_close(reinterpret_cast<sqlite3 *>(db_));
    db_ = nullptr;
    std::cout << "[DB] Database closed" << std::endl;
  }
}

bool Database::isOpen() const {
  return db_ != nullptr;
}

bool Database::createTables() {
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);

  // 创建传感器数据表
  const std::string createSensorTable = R"(
    CREATE TABLE IF NOT EXISTS sensor_data (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      timestamp DATETIME NOT NULL,
      device_id TEXT NOT NULL,
      weight REAL NOT NULL DEFAULT 0.0,
      motor_status INTEGER NOT NULL DEFAULT 0,
      battery INTEGER NOT NULL DEFAULT 0,
      storage_full INTEGER NOT NULL DEFAULT 0,
      client_ip TEXT NOT NULL DEFAULT ''
    );
  )";

  // 创建设备状态日志表
  const std::string createLogTable = R"(
    CREATE TABLE IF NOT EXISTS status_logs (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      timestamp DATETIME NOT NULL,
      device_id TEXT NOT NULL,
      event_type TEXT NOT NULL,
      message TEXT NOT NULL DEFAULT ''
    );
  )";

  // 创建索引
  const std::string createIndices = R"(
    CREATE INDEX IF NOT EXISTS idx_sensor_timestamp ON sensor_data(timestamp);
    CREATE INDEX IF NOT EXISTS idx_sensor_device ON sensor_data(device_id);
    CREATE INDEX IF NOT EXISTS idx_sensor_device_time ON sensor_data(device_id, timestamp);
    CREATE INDEX IF NOT EXISTS idx_log_timestamp ON status_logs(timestamp);
    CREATE INDEX IF NOT EXISTS idx_log_device ON status_logs(device_id);
    CREATE INDEX IF NOT EXISTS idx_log_event ON status_logs(event_type);
  )";

  if (!executeSQL(db, createSensorTable)) return false;
  if (!executeSQL(db, createLogTable)) return false;
  if (!executeSQL(db, createIndices)) return false;

  std::cout << "[DB] Tables created successfully" << std::endl;
  return true;
}

bool Database::insertSensorData(const RobotData &data, const std::string &client_ip) {
  if (!db_) {
    std::cerr << "[DB] Database not opened" << std::endl;
    return false;
  }

  std::string timestamp = getCurrentTimestamp();
  std::string device_id = escapeSQL(data.device_id);
  std::string ip = escapeSQL(client_ip);

  std::ostringstream sql;
  sql << "INSERT INTO sensor_data (timestamp, device_id, weight, motor_status, "
      << "battery, storage_full, client_ip) VALUES ("
      << "'" << timestamp << "', "
      << "'" << device_id << "', "
      << std::fixed << std::setprecision(2) << data.weight << ", "
      << static_cast<int>(data.motor_status) << ", "
      << static_cast<int>(data.battery) << ", "
      << (data.storage_full ? 1 : 0) << ", "
      << "'" << ip << "');";

  bool success = executeSQL(reinterpret_cast<sqlite3 *>(db_), sql.str());
  if (success) {
    std::cout << "[DB] Inserted sensor data: device=" << data.device_id
              << ", weight=" << data.weight << "kg" << std::endl;
  }
  return success;
}

bool Database::insertStatusLog(const std::string &device_id,
                               const std::string &event_type,
                               const std::string &message) {
  if (!db_) {
    std::cerr << "[DB] Database not opened" << std::endl;
    return false;
  }

  std::string timestamp = getCurrentTimestamp();
  std::string dev_id = escapeSQL(device_id);
  std::string evt_type = escapeSQL(event_type);
  std::string msg = escapeSQL(message);

  std::ostringstream sql;
  sql << "INSERT INTO status_logs (timestamp, device_id, event_type, message) VALUES ("
      << "'" << timestamp << "', "
      << "'" << dev_id << "', "
      << "'" << evt_type << "', "
      << "'" << msg << "');";

  bool success = executeSQL(reinterpret_cast<sqlite3 *>(db_), sql.str());
  if (success) {
    std::cout << "[DB] Status log: " << event_type << " - " << message << std::endl;
  }
  return success;
}

std::vector<SensorRecord> Database::querySensorData(const QueryParams &params) {
  std::vector<SensorRecord> results;
  if (!db_) return results;

  std::ostringstream sql;
  sql << "SELECT id, timestamp, device_id, weight, motor_status, "
      << "battery, storage_full, client_ip FROM sensor_data WHERE 1=1";

  if (!params.device_id.empty()) {
    sql << " AND device_id='" << escapeSQL(params.device_id) << "'";
  }
  if (!params.start_time.empty()) {
    sql << " AND timestamp>='" << escapeSQL(params.start_time) << "'";
  }
  if (!params.end_time.empty()) {
    sql << " AND timestamp<='" << escapeSQL(params.end_time) << "'";
  }

  sql << " ORDER BY timestamp DESC";
  sql << " LIMIT " << params.limit << " OFFSET " << params.offset << ";";

  sqlite3_stmt *stmt = nullptr;
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);
  int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB] Query failed: " << sqlite3_errmsg(db) << std::endl;
    return results;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    SensorRecord record;
    record.id = sqlite3_column_int64(stmt, 0);
    const char *ts = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    const char *dev = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    const char *cip = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
    record.timestamp = ts ? ts : "";
    record.device_id = dev ? dev : "";
    record.weight = sqlite3_column_double(stmt, 3);
    record.motor_status = sqlite3_column_int(stmt, 4);
    record.battery = sqlite3_column_int(stmt, 5);
    record.storage_full = sqlite3_column_int(stmt, 6) != 0;
    record.client_ip = cip ? cip : "";
    results.push_back(record);
  }

  sqlite3_finalize(stmt);
  return results;
}

std::vector<StatusLog> Database::queryStatusLogs(const std::string &device_id,
                                                  const std::string &start_time,
                                                  const std::string &end_time,
                                                  int limit) {
  std::vector<StatusLog> results;
  if (!db_) return results;

  std::ostringstream sql;
  sql << "SELECT id, timestamp, device_id, event_type, message "
      << "FROM status_logs WHERE 1=1";

  if (!device_id.empty()) {
    sql << " AND device_id='" << escapeSQL(device_id) << "'";
  }
  if (!start_time.empty()) {
    sql << " AND timestamp>='" << escapeSQL(start_time) << "'";
  }
  if (!end_time.empty()) {
    sql << " AND timestamp<='" << escapeSQL(end_time) << "'";
  }

  sql << " ORDER BY timestamp DESC LIMIT " << limit << ";";

  sqlite3_stmt *stmt = nullptr;
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);
  int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB] Query logs failed: " << sqlite3_errmsg(db) << std::endl;
    return results;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    StatusLog log;
    log.id = sqlite3_column_int64(stmt, 0);
    log.timestamp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    log.device_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    log.event_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
    log.message = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
    results.push_back(log);
  }

  sqlite3_finalize(stmt);
  return results;
}

std::vector<HourlyStats> Database::queryHourlyStats(const std::string &device_id,
                                                      const std::string &start_time,
                                                      const std::string &end_time) {
  std::vector<HourlyStats> results;
  if (!db_) return results;

  std::ostringstream sql;
  sql << "SELECT "
      << "strftime('%Y-%m-%d %H:00:00', timestamp) AS hour, "
      << "device_id, "
      << "COUNT(*) AS report_count, "
      << "SUM(weight) AS total_weight, "
      << "AVG(battery) AS avg_battery, "
      << "MAX(weight) AS max_weight "
      << "FROM sensor_data WHERE 1=1";

  if (!device_id.empty()) {
    sql << " AND device_id='" << escapeSQL(device_id) << "'";
  }
  if (!start_time.empty()) {
    sql << " AND timestamp>='" << escapeSQL(start_time) << "'";
  }
  if (!end_time.empty()) {
    sql << " AND timestamp<='" << escapeSQL(end_time) << "'";
  }

  sql << " GROUP BY hour, device_id ORDER BY hour DESC;";

  sqlite3_stmt *stmt = nullptr;
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);
  int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    std::cerr << "[DB] Query stats failed: " << sqlite3_errmsg(db) << std::endl;
    return results;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    HourlyStats stat;
    stat.hour = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    stat.device_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    stat.report_count = sqlite3_column_int(stmt, 2);
    stat.total_weight = sqlite3_column_double(stmt, 3);
    stat.avg_battery = static_cast<int>(sqlite3_column_double(stmt, 4));
    stat.max_weight = sqlite3_column_double(stmt, 5);
    results.push_back(stat);
  }

  sqlite3_finalize(stmt);
  return results;
}

bool Database::getLatestStatus(const std::string &device_id, SensorRecord &out) {
  if (!db_ || device_id.empty()) return false;

  std::ostringstream sql;
  sql << "SELECT id, timestamp, device_id, weight, motor_status, "
      << "battery, storage_full, client_ip FROM sensor_data "
      << "WHERE device_id='" << escapeSQL(device_id) << "' "
      << "ORDER BY timestamp DESC LIMIT 1;";

  sqlite3_stmt *stmt = nullptr;
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);
  int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    return false;
  }

  bool found = false;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    out.id = sqlite3_column_int64(stmt, 0);
    out.timestamp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
    out.device_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
    out.weight = sqlite3_column_double(stmt, 3);
    out.motor_status = sqlite3_column_int(stmt, 4);
    out.battery = sqlite3_column_int(stmt, 5);
    out.storage_full = sqlite3_column_int(stmt, 6) != 0;
    out.client_ip = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 7));
    found = true;
  }

  sqlite3_finalize(stmt);
  return found;
}

std::vector<std::string> Database::getDeviceList() {
  std::vector<std::string> devices;
  if (!db_) return devices;

  const std::string sql = "SELECT DISTINCT device_id FROM sensor_data ORDER BY device_id;";

  sqlite3_stmt *stmt = nullptr;
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    return devices;
  }

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *device_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    if (device_id) {
      devices.push_back(device_id);
    }
  }

  sqlite3_finalize(stmt);
  return devices;
}

std::string Database::getSummary(const std::string &device_id, int days) {
  if (!db_) return "Database not opened";

  // 计算起始时间
  auto now = std::time(nullptr);
  auto seconds = static_cast<time_t>(days) * 24 * 60 * 60;
  auto start = now - seconds;
  struct tm start_tm_buf;
  localtime_r(&start, &start_tm_buf);
  std::ostringstream start_oss;
  start_oss << std::put_time(&start_tm_buf, "%Y-%m-%d %H:%M:%S");
  std::string start_time = start_oss.str();

  std::ostringstream sql;
  sql << "SELECT "
      << "COUNT(*) AS total_reports, "
      << "SUM(weight) AS total_weight, "
      << "AVG(battery) AS avg_battery, "
      << "MAX(weight) AS max_weight, "
      << "COUNT(DISTINCT device_id) AS device_count "
      << "FROM sensor_data WHERE timestamp>='" << start_time << "'";

  if (!device_id.empty()) {
    sql << " AND device_id='" << escapeSQL(device_id) << "'";
  }
  sql << ";";

  sqlite3_stmt *stmt = nullptr;
  sqlite3 *db = reinterpret_cast<sqlite3 *>(db_);
  int rc = sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr);

  if (rc != SQLITE_OK) {
    return "Query failed";
  }

  std::ostringstream result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    int total_reports = sqlite3_column_int(stmt, 0);
    double total_weight = sqlite3_column_double(stmt, 1);
    int avg_battery = static_cast<int>(sqlite3_column_double(stmt, 2));
    double max_weight = sqlite3_column_double(stmt, 3);
    int device_count = sqlite3_column_int(stmt, 4);

    result << "=== 最近 " << days << " 天统计 ===\n"
           << "设备数量: " << device_count << "\n"
           << "上报次数: " << total_reports << "\n"
           << "总收割量: " << std::fixed << std::setprecision(2) << total_weight << " kg\n"
           << "平均电量: " << avg_battery << "%\n"
           << "单次最大: " << max_weight << " kg";
  } else {
    result << "暂无数据";
  }

  sqlite3_finalize(stmt);
  return result.str();
}