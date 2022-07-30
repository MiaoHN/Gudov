#ifndef __GUDOV_LOG_H__
#define __GUDOV_LOG_H__

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
#include "util.h"

#define GUDOV_LOG_LEVEL(logger, level)                                \
  if (logger->getLevel() <= level)                                    \
  gudov::LogEventWrap(                                                \
      gudov::LogEvent::ptr(new gudov::LogEvent(                       \
          logger, level, __FILE__, __LINE__, 0, gudov::GetThreadId(), \
          gudov::GetFiberId(), time(0))))                             \
      .getSS()

#define GUDOV_LOG_FMT_LEVEL(logger, level, fmt, ...)                  \
  if (logger->getLevel() <= level)                                    \
  gudov::LogEventWrap(                                                \
      gudov::LogEvent::ptr(new gudov::LogEvent(                       \
          logger, level, __FILE__, __LINE__, 0, gudov::GetThreadId(), \
          gudov::GetFiberId(), time(0))))                             \
      .getEvent()                                                     \
      ->format(fmt, __VA_ARGS__)

#define GUDOV_LOG_DEBUG(logger) GUDOV_LOG_LEVEL(logger, gudov::LogLevel::DEBUG)
#define GUDOV_LOG_INFO(logger) GUDOV_LOG_LEVEL(logger, gudov::LogLevel::INFO)
#define GUDOV_LOG_WARN(logger) GUDOV_LOG_LEVEL(logger, gudov::LogLevel::WARN)
#define GUDOV_LOG_ERROR(logger) GUDOV_LOG_LEVEL(logger, gudov::LogLevel::ERROR)
#define GUDOV_LOG_FATAL(logger) GUDOV_LOG_LEVEL(logger, gudov::LogLevel::FATAL)

#define GUDOV_LOG_FMT_DEBUG(logger, fmt, ...) \
  GUDOV_LOG_FMT_LEVEL(logger, gudov::LogLevel::DEBUG, fmt, __VA_ARGS__)
#define GUDOV_LOG_FMT_INFO(logger, fmt, ...) \
  GUDOV_LOG_FMT_LEVEL(logger, gudov::LogLevel::INFO, fmt, __VA_ARGS__)
#define GUDOV_LOG_FMT_WARN(logger, fmt, ...) \
  GUDOV_LOG_FMT_LEVEL(logger, gudov::LogLevel::WARN, fmt, __VA_ARGS__)
#define GUDOV_LOG_FMT_ERROR(logger, fmt, ...) \
  GUDOV_LOG_FMT_LEVEL(logger, gudov::LogLevel::ERROR, fmt, __VA_ARGS__)
#define GUDOV_LOG_FMT_FATAL(logger, fmt, ...) \
  GUDOV_LOG_FMT_LEVEL(logger, gudov::LogLevel::FATAL, fmt, __VA_ARGS__)

#define GUDOV_LOG_ROOT() gudov::LoggerMgr::getInstance()->getRoot()

namespace gudov {

class Logger;

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

  static const char* ToString(LogLevel::Level level);
};

class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;

  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
           const char* filename, int32_t line, uint32_t elapse,
           uint32_t threadId, uint32_t fiberId, uint64_t time);

  const char* getFile() const { return file_; }
  int32_t getLine() const { return line_; }
  uint32_t getElapse() const { return elapse_; }
  uint32_t getThreadId() const { return threadId_; }
  uint32_t getFiberId() const { return fiberId_; }
  uint64_t getTime() const { return time_; }
  std::string getContent() const { return ss_.str(); }
  std::shared_ptr<Logger> getLogger() const { return logger_; }
  LogLevel::Level getLevel() const { return level_; }

  std::stringstream& getSS() { return ss_; }
  void format(const char* fmt, ...);
  void format(const char* fmt, va_list al);

 private:
  const char* file_ = nullptr;
  int32_t line_ = 0;
  uint32_t elapse_ = 0;
  uint32_t threadId_ = 0;
  uint32_t fiberId_ = 0;
  uint64_t time_ = 0;
  std::stringstream ss_;

  std::shared_ptr<Logger> logger_;
  LogLevel::Level level_;
};

class LogEventWrap {
 public:
  LogEventWrap(LogEvent::ptr e);
  ~LogEventWrap();
  LogEvent::ptr getEvent() const { return event_; }
  std::stringstream& getSS();

 private:
  LogEvent::ptr event_;
};

class LogFormatter {
 public:
  using ptr = std::shared_ptr<LogFormatter>;
  LogFormatter(const std::string& pattern);

  std::string format(std::shared_ptr<Logger> logger, LogLevel::Level level,
                     LogEvent::ptr event);

 public:
  class FormatItem {
   public:
    using ptr = std::shared_ptr<FormatItem>;
    virtual ~FormatItem() {}
    virtual void format(std::ostream& os, std::shared_ptr<Logger> logger,
                        LogLevel::Level level, LogEvent::ptr event) = 0;
  };

  void init();

 private:
  std::string pattern_;
  std::vector<FormatItem::ptr> items_;
};

class LogAppender {
 public:
  using ptr = std::shared_ptr<LogAppender>;
  virtual ~LogAppender() {}

  virtual void log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   LogEvent::ptr event) = 0;

  void setFormatter(LogFormatter::ptr formatter) { formatter_ = formatter; }
  LogFormatter::ptr getFormatter() const { return formatter_; }

  LogLevel::Level getLevel() const { return level_; }
  void setLevel(LogLevel::Level level) { level_ = level; }

 protected:
  LogLevel::Level level_ = LogLevel::DEBUG;
  LogFormatter::ptr formatter_;
};

class Logger : public std::enable_shared_from_this<Logger> {
 public:
  using ptr = std::shared_ptr<Logger>;

  Logger(const std::string& name = "root");
  void log(LogLevel::Level level, LogEvent::ptr event);

  void debug(LogEvent::ptr event);
  void info(LogEvent::ptr event);
  void warn(LogEvent::ptr event);
  void error(LogEvent::ptr event);
  void fatal(LogEvent::ptr event);

  void addAppender(LogAppender::ptr appender);
  void delAppender(LogAppender::ptr appender);
  LogLevel::Level getLevel() const { return level_; }
  void setLevel(LogLevel::Level level) { level_ = level; }

  const std::string& getName() const { return name_; }

 private:
  std::string name_;
  LogLevel::Level level_;
  std::list<LogAppender::ptr> appenders_;
  LogFormatter::ptr formatter_;
};

class StdoutLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<StdoutLogAppender> ptr;
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;
};

class FileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<FileLogAppender> ptr;
  FileLogAppender(const std::string& filename);
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;

  bool reopen();

 private:
  std::string filename_;
  std::ofstream filestream_;
};

class LoggerManager {
 public:
  LoggerManager();
  Logger::ptr getLogger(const std::string& name);

  void init();
  Logger::ptr getRoot() const { return root_; }

 private:
  std::map<std::string, Logger::ptr> loggers_;
  Logger::ptr root_;
};

using LoggerMgr = gudov::Singleton<LoggerManager>;

}  // namespace gudov

#endif  // __GUDOV_LOG_H__
