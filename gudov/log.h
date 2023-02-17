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

#include "gudov/thread.h"
#include "singleton.h"
#include "thread"
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
#define GUDOV_LOG_INFO(logger)  GUDOV_LOG_LEVEL(logger, gudov::LogLevel::INFO)
#define GUDOV_LOG_WARN(logger)  GUDOV_LOG_LEVEL(logger, gudov::LogLevel::WARN)
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

#define GUDOV_LOG_ROOT()     gudov::LoggerMgr::getInstance()->getRoot()
#define GUDOV_LOG_NAME(name) gudov::LoggerMgr::getInstance()->getLogger(name)

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

class LogEvent {
 public:
  using ptr = std::shared_ptr<LogEvent>;

  LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
           const char* filename, int32_t line, uint32_t elapse,
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

  bool              isError() const { return m_error; }
  const std::string getPattern() const { return m_pattern; }

 private:
  std::string                  m_pattern;
  std::vector<FormatItem::ptr> m_items;
  bool                         m_error = false;
};

class LogAppender {
  friend class Logger;

 public:
  using ptr       = std::shared_ptr<LogAppender>;
  using MutexType = Spinlock;
  virtual ~LogAppender() {}

  virtual void        log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                          LogEvent::ptr event) = 0;
  virtual std::string toYamlString()           = 0;

  void              setFormatter(LogFormatter::ptr formatter);
  LogFormatter::ptr getFormatter();

  LogLevel::Level getLevel() const { return m_level; }
  void            setLevel(LogLevel::Level level) { m_level = level; }

 protected:
  LogLevel::Level   m_level         = LogLevel::DEBUG;
  bool              m_has_formatter = false;
  MutexType         m_mutex;
  LogFormatter::ptr m_formatter;
};

class Logger : public std::enable_shared_from_this<Logger> {
  friend class LoggerManager;

 public:
  using ptr       = std::shared_ptr<Logger>;
  using MutexType = Spinlock;

  Logger(const std::string& name = "root");
  void log(LogLevel::Level level, LogEvent::ptr event);

  void debug(LogEvent::ptr event);
  void info(LogEvent::ptr event);
  void warn(LogEvent::ptr event);
  void error(LogEvent::ptr event);
  void fatal(LogEvent::ptr event);

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
  typedef std::shared_ptr<StdoutLogAppender> ptr;
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;

  std::string toYamlString() override;
};

class FileLogAppender : public LogAppender {
 public:
  typedef std::shared_ptr<FileLogAppender> ptr;
  FileLogAppender(const std::string& filename);
  void log(Logger::ptr logger, LogLevel::Level level,
           LogEvent::ptr event) override;

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

#endif  // __GUDOV_LOG_H__
