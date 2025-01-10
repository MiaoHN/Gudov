#pragma once

#include <cstdarg>
#include <cstdint>
#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "singleton.h"
#include "thread"
#include "thread.h"
#include "util.h"

#define LOG_LEVEL(logger, level)                                                                                     \
  if (logger->GetLevel() <= level)                                                                                   \
  gudov::LogEventWrap(gudov::LogEvent::ptr(new gudov::LogEvent(logger, level, __FILE__, __LINE__, 0,                 \
                                                               gudov::GetThreadId(), gudov::GetFiberId(), time(0)))) \
      .GetSS()

#define LOG_FMT_LEVEL(logger, level, fmt, ...)                                                                       \
  if (logger->GetLevel() <= level)                                                                                   \
  gudov::LogEventWrap(gudov::LogEvent::ptr(new gudov::LogEvent(logger, level, __FILE__, __LINE__, 0,                 \
                                                               gudov::GetThreadId(), gudov::GetFiberId(), time(0)))) \
      .GetEvent()                                                                                                    \
      ->format(fmt, __VA_ARGS__)

#ifdef GUDOV_DEBUG
#define LOG_DEBUG(logger) LOG_LEVEL(logger, gudov::LogLevel::DEBUG)
#else
#define LOG_DEBUG(logger) gudov::LogEmpty().GetSS()
#endif

#define LOG_INFO(logger)  LOG_LEVEL(logger, gudov::LogLevel::INFO)
#define LOG_WARN(logger)  LOG_LEVEL(logger, gudov::LogLevel::WARN)
#define LOG_ERROR(logger) LOG_LEVEL(logger, gudov::LogLevel::ERROR)
#define LOG_FATAL(logger) LOG_LEVEL(logger, gudov::LogLevel::FATAL)

#define LOG_FMT_DEBUG(logger, fmt, ...) LOG_FMT_LEVEL(logger, gudov::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define LOG_FMT_INFO(logger, fmt, ...)  LOG_FMT_LEVEL(logger, gudov::LogLevel::INFO, fmt, __VA_ARGS__)
#define LOG_FMT_WARN(logger, fmt, ...)  LOG_FMT_LEVEL(logger, gudov::LogLevel::WARN, fmt, __VA_ARGS__)
#define LOG_FMT_ERROR(logger, fmt, ...) LOG_FMT_LEVEL(logger, gudov::LogLevel::ERROR, fmt, __VA_ARGS__)
#define LOG_FMT_FATAL(logger, fmt, ...) LOG_FMT_LEVEL(logger, gudov::LogLevel::FATAL, fmt, __VA_ARGS__)

#define LOG_ROOT()     gudov::LoggerMgr::GetInstance()->GetRoot()
#define LOG_NAME(name) gudov::LoggerMgr::GetInstance()->GetLogger(name)

namespace gudov {

class Logger;
class LoggerManager;

class LogLevel {
 public:
  enum Level {
    UNKNOWN = 0,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
  };

  static const char*     ToString(LogLevel::Level level);
  static LogLevel::Level FromString(const std::string& str);
};

class LogEmpty {
 public:
  std::stringstream& GetSS() { return ss_; }

 private:
  std::stringstream ss_;
};

/**
 * @brief 日志事件
 * @details 记录日志发生的时间，文件位置，行数，协程号，线程号以及附加信息
 *
 */
class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;

  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* filename, int32_t line, uint32_t elapse,
           uint32_t threadId, uint32_t fiberId, uint64_t time);

  const char*             GetFile() const { return file_; }
  int32_t                 GetLine() const { return line_; }
  uint32_t                GetElapse() const { return elapse_; }
  uint32_t                GetThreadId() const { return thread_id_; }
  uint32_t                GetFiberId() const { return fiber_id_; }
  uint64_t                GetTime() const { return time_; }
  std::string             GetContent() const { return ss_.str(); }
  std::shared_ptr<Logger> GetLogger() const { return logger_; }
  LogLevel::Level         GetLevel() const { return level_; }

  std::stringstream& GetSS() { return ss_; }
  void               format(const char* fmt, ...);
  void               format(const char* fmt, va_list al);

 private:
  const char*       file_      = nullptr;
  int32_t           line_      = 0;
  uint32_t          elapse_    = 0;
  uint32_t          thread_id_ = 0;
  uint32_t          fiber_id_  = 0;
  uint64_t          time_      = 0;
  std::stringstream ss_;

  std::shared_ptr<Logger> logger_;
  LogLevel::Level         level_;
};

class LogEventWrap {
 public:
  LogEventWrap(LogEvent::ptr e);
  ~LogEventWrap();

  LogEvent::ptr      GetEvent() const { return event_; }
  std::stringstream& GetSS();

 private:
  LogEvent::ptr event_;
};

/**
 * @brief 负责格式化日志内容
 *
 */
class LogFormatter {
 public:
  using ptr = std::shared_ptr<LogFormatter>;
  LogFormatter(const std::string& pattern);

  std::string format(LogEvent::ptr event);

 public:
  class FormatItem {
   public:
    using ptr = std::shared_ptr<FormatItem>;
    virtual ~FormatItem() {}
    virtual void format(std::ostream& os, LogEvent::ptr event) = 0;
  };

  void Init();

  bool              IsError() const { return error_; }
  const std::string GetPattern() const { return pattern_; }

 private:
  std::string                  pattern_;
  std::vector<FormatItem::ptr> items_;
  bool                         error_ = false;
};

/**
 * @brief 日志输出器封装
 *
 */
class LogAppender {
  friend class Logger;

 public:
  using ptr       = std::shared_ptr<LogAppender>;
  using MutexType = Spinlock;

  LogAppender(LogLevel::Level level = LogLevel::UNKNOWN) : level_(level) {}
  virtual ~LogAppender() {}

  /**
   * @brief 输出日志内容
   *
   * @param event 日志事件
   */
  virtual void output(LogEvent::ptr event) = 0;

  virtual std::string toYamlString() = 0;

  void              SetFormatter(LogFormatter::ptr formatter);
  LogFormatter::ptr GetFormatter();

  LogLevel::Level GetLevel() const { return level_; }
  void            SetLevel(LogLevel::Level level) { level_ = level; }

 protected:
  LogLevel::Level   level_ = LogLevel::UNKNOWN;
  MutexType         mutex_;
  LogFormatter::ptr formatter_;
};

class Logger : public std::enable_shared_from_this<Logger> {
  friend class LoggerManager;

 public:
  using ptr       = std::shared_ptr<Logger>;
  using MutexType = Spinlock;

  Logger(const std::string& name = "root");
  void log(LogEvent::ptr event);

  void            addAppender(LogAppender::ptr appender);
  void            DelAppender(LogAppender::ptr appender);
  void            clearAppenders();
  LogLevel::Level GetLevel() const { return level_; }
  void            SetLevel(LogLevel::Level level) { level_ = level; }

  const std::string& GetName() const { return name_; }

  void              SetFormatter(LogFormatter::ptr val);
  void              SetFormatter(const std::string& val);
  LogFormatter::ptr GetFormatter();

  std::string toYamlString();

 private:
  std::string                 name_;
  LogLevel::Level             level_;
  MutexType                   mutex_;
  std::list<LogAppender::ptr> appenders_;
  LogFormatter::ptr           formatter_;
  Logger::ptr                 root_;
};

class StdoutLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<StdoutLogAppender>;

  StdoutLogAppender(LogLevel::Level level = LogLevel::DEBUG);

  /**
   * @brief 输出日志内容
   *
   * @param event 日志事件
   */
  void output(LogEvent::ptr event) override;

  std::string toYamlString() override;
};

class FileLogAppender : public LogAppender {
 public:
  using ptr = std::shared_ptr<FileLogAppender>;
  FileLogAppender(const std::string& filename, LogLevel::Level level = LogLevel::DEBUG);

  /**
   * @brief 输出日志内容
   *
   * @param event 日志事件
   */
  void output(LogEvent::ptr event) override;

  std::string toYamlString() override;

  bool reopen();

 private:
  std::string   filename_;
  std::ofstream filestream_;
  uint64_t      last_time_ = 0;
};

class LoggerManager {
 public:
  using MutexType = Spinlock;

  LoggerManager();
  Logger::ptr GetLogger(const std::string& name);

  void        Init();
  Logger::ptr GetRoot() const { return root_; }

  std::string toYamlString();

 private:
  MutexType                          mutex_;
  std::map<std::string, Logger::ptr> loggers_;
  Logger::ptr                        root_;
};

using LoggerMgr = gudov::Singleton<LoggerManager>;

}  // namespace gudov
