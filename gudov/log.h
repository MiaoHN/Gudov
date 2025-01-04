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
  if (logger->getLevel() <= level)                                                                                   \
  gudov::LogEventWrap(gudov::LogEvent::ptr(new gudov::LogEvent(logger, level, __FILE__, __LINE__, 0,                 \
                                                               gudov::GetThreadId(), gudov::GetFiberId(), time(0)))) \
      .getSS()

#define LOG_FMT_LEVEL(logger, level, fmt, ...)                                                                       \
  if (logger->getLevel() <= level)                                                                                   \
  gudov::LogEventWrap(gudov::LogEvent::ptr(new gudov::LogEvent(logger, level, __FILE__, __LINE__, 0,                 \
                                                               gudov::GetThreadId(), gudov::GetFiberId(), time(0)))) \
      .getEvent()                                                                                                    \
      ->format(fmt, __VA_ARGS__)

#ifdef GUDOV_DEBUG
#define LOG_DEBUG(logger) LOG_LEVEL(logger, gudov::LogLevel::DEBUG)
#else
#define LOG_DEBUG(logger) gudov::LogEmpty().getSS()
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

#define LOG_ROOT()     gudov::LoggerMgr::getInstance()->getRoot()
#define LOG_NAME(name) gudov::LoggerMgr::getInstance()->getLogger(name)

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
  std::stringstream& getSS() { return m_ss; }

 private:
  std::stringstream m_ss;
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

  const char*             getFile() const { return m_file; }
  int32_t                 getLine() const { return m_line; }
  uint32_t                getElapse() const { return m_elapse; }
  uint32_t                getThreadId() const { return m_thread_id; }
  uint32_t                getFiberId() const { return m_fiber_id; }
  uint64_t                getTime() const { return m_time; }
  std::string             getContent() const { return m_ss.str(); }
  std::shared_ptr<Logger> getLogger() const { return m_logger; }
  LogLevel::Level         getLevel() const { return m_level; }

  std::stringstream& getSS() { return m_ss; }
  void               format(const char* fmt, ...);
  void               format(const char* fmt, va_list al);

 private:
  const char*       m_file      = nullptr;
  int32_t           m_line      = 0;
  uint32_t          m_elapse    = 0;
  uint32_t          m_thread_id = 0;
  uint32_t          m_fiber_id  = 0;
  uint64_t          m_time      = 0;
  std::stringstream m_ss;

  std::shared_ptr<Logger> m_logger;
  LogLevel::Level         m_level;
};

class LogEventWrap {
 public:
  LogEventWrap(LogEvent::ptr e);
  ~LogEventWrap();

  LogEvent::ptr      getEvent() const { return m_event; }
  std::stringstream& getSS();

 private:
  LogEvent::ptr m_event;
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

  void init();

  bool              isError() const { return m_error; }
  const std::string getPattern() const { return m_pattern; }

 private:
  std::string                  m_pattern;
  std::vector<FormatItem::ptr> m_items;
  bool                         m_error = false;
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

  LogAppender(LogLevel::Level level = LogLevel::UNKNOWN) : m_level(level) {}
  virtual ~LogAppender() {}

  /**
   * @brief 输出日志内容
   *
   * @param event 日志事件
   */
  virtual void output(LogEvent::ptr event) = 0;

  virtual std::string toYamlString() = 0;

  void              setFormatter(LogFormatter::ptr formatter);
  LogFormatter::ptr getFormatter();

  LogLevel::Level getLevel() const { return m_level; }
  void            setLevel(LogLevel::Level level) { m_level = level; }

 protected:
  LogLevel::Level   m_level = LogLevel::UNKNOWN;
  MutexType         m_mutex;
  LogFormatter::ptr m_formatter;
};

class Logger : public std::enable_shared_from_this<Logger> {
  friend class LoggerManager;

 public:
  using ptr       = std::shared_ptr<Logger>;
  using MutexType = Spinlock;

  Logger(const std::string& name = "root");
  void log(LogEvent::ptr event);

  void            addAppender(LogAppender::ptr appender);
  void            delAppender(LogAppender::ptr appender);
  void            clearAppenders();
  LogLevel::Level getLevel() const { return m_level; }
  void            setLevel(LogLevel::Level level) { m_level = level; }

  const std::string& getName() const { return m_name; }

  void              setFormatter(LogFormatter::ptr val);
  void              setFormatter(const std::string& val);
  LogFormatter::ptr getFormatter();

  std::string toYamlString();

 private:
  std::string                 m_name;
  LogLevel::Level             m_level;
  MutexType                   m_mutex;
  std::list<LogAppender::ptr> m_appenders;
  LogFormatter::ptr           m_formatter;
  Logger::ptr                 m_root;
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
  std::string   m_filename;
  std::ofstream m_filestream;
  uint64_t      m_last_time = 0;
};

class LoggerManager {
 public:
  using MutexType = Spinlock;

  LoggerManager();
  Logger::ptr getLogger(const std::string& name);

  void        init();
  Logger::ptr getRoot() const { return m_root; }

  std::string toYamlString();

 private:
  MutexType                          m_mutex;
  std::map<std::string, Logger::ptr> m_loggers;
  Logger::ptr                        m_root;
};

using LoggerMgr = gudov::Singleton<LoggerManager>;

}  // namespace gudov
