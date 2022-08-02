#include "log.h"

#include <cstdint>
#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>

#include "config.h"

namespace gudov {

const char* LogLevel::ToString(LogLevel::Level level) {
  switch (level) {
#define XX(name)       \
  case LogLevel::name: \
    return #name;      \
    break;

    XX(DEBUG);
    XX(INFO);
    XX(WARN);
    XX(ERROR);
    XX(FATAL);
    XX(UNKNOWN);

#undef XX
    default:
      return "UNKNOWN";
  }
}

LogLevel::Level LogLevel::FromString(const std::string& str) {
#define XX(level, v)        \
  if (str == #v) {          \
    return LogLevel::level; \
  }
  XX(DEBUG, debug);
  XX(INFO, info);
  XX(WARN, warn);
  XX(ERROR, error);
  XX(FATAL, fatal);

  XX(DEBUG, DEBUG);
  XX(INFO, INFO);
  XX(WARN, WARN);
  XX(ERROR, ERROR);
  XX(FATAL, FATAL);
  return LogLevel::UNKNOWN;
#undef XX
}

LogEventWrap::LogEventWrap(LogEvent::ptr e) : event_(e) {}

LogEventWrap::~LogEventWrap() {
  event_->getLogger()->log(event_->getLevel(), event_);
}

void LogEvent::format(const char* fmt, ...) {
  va_list al;
  va_start(al, fmt);
  format(fmt, al);
  va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
  char* buf = nullptr;
  int len = vasprintf(&buf, fmt, al);
  if (len != -1) {
    ss_ << std::string(buf, len);
    free(buf);
  }
}

std::stringstream& LogEventWrap::getSS() { return event_->getSS(); }

void LogAppender::setFormatter(LogFormatter::ptr formatter) {
  MutexType::Lock lock(mutex_);
  formatter_ = formatter;
  if (formatter_) {
    hasFormatter_ = true;
  } else {
    hasFormatter_ = false;
  }
}

LogFormatter::ptr LogAppender::getFormatter() {
  MutexType::Lock lock(mutex_);
  return formatter_;
}

class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  MessageFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getContent();
  }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  LevelFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << LogLevel::ToString(level);
  }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  ElapseFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getElapse();
  }
};

class NameFormatItem : public LogFormatter::FormatItem {
 public:
  NameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getLogger()->getName();
  }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  ThreadIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getThreadId();
  }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  FiberIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getFiberId();
  }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S")
      : m_format(format) {
    if (m_format.empty()) {
      m_format = "%Y-%m-%d %H:%M:%S";
    }
  }

  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    struct tm tm;
    time_t time = event->getTime();
    localtime_r(&time, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), m_format.c_str(), &tm);
    os << buf;
  }

 private:
  std::string m_format;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
 public:
  FilenameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getFile();
  }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  LineFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << event->getLine();
  }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  NewLineFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << std::endl;
  }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  StringFormatItem(const std::string& str) : string_(str) {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << string_;
  }

 private:
  std::string string_;
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  TabFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, Logger::ptr logger, LogLevel::Level level,
              LogEvent::ptr event) override {
    os << "\t";
  }
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level,
                   const char* filename, int32_t line, uint32_t elapse,
                   uint32_t threadId, uint32_t fiberId, uint64_t time)
    : file_(filename),
      line_(line),
      elapse_(elapse),
      threadId_(threadId),
      fiberId_(fiberId),
      time_(time),
      logger_(logger),
      level_(level) {}

Logger::Logger(const std::string& name) : name_(name), level_(LogLevel::DEBUG) {
  formatter_.reset(new LogFormatter(
      "%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::setFormatter(LogFormatter::ptr val) {
  MutexType::Lock lock(mutex_);
  formatter_ = val;

  for (auto& i : appenders_) {
    MutexType::Lock ll(i->mutex_);
    if (!i->hasFormatter_) {
      i->formatter_ = formatter_;
    }
  }
}

void Logger::setFormatter(const std::string& val) {
  LogFormatter::ptr newVal(new LogFormatter(val));
  if (newVal->isError()) {
    std::cout << "Logger setFormatter name=" << name_ << " value=" << val
              << " invalid formatter" << std::endl;
    return;
  }
  setFormatter(newVal);
}

std::string Logger::toYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  node["name"] = name_;
  if (level_ != LogLevel::UNKNOWN) {
    node["level"] = LogLevel::ToString(level_);
  }
  if (formatter_) {
    node["formatter"] = formatter_->getPattern();
  }

  for (auto& i : appenders_) {
    node["appenders"].push_back(YAML::Load(i->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogFormatter::ptr Logger::getFormatter() {
  MutexType::Lock lock(mutex_);
  return formatter_;
}

void Logger::addAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(mutex_);
  if (!appender->getFormatter()) {
    MutexType::Lock ll(appender->mutex_);
    appender->formatter_ = formatter_;
  }
  appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
  MutexType::Lock lock(mutex_);
  for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
    if (*it == appender) {
      appenders_.erase(it);
      break;
    }
  }
}

void Logger::clearAppenders() {
  MutexType::Lock lock(mutex_);
  appenders_.clear();
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
  if (level >= level_) {
    auto self = shared_from_this();
    MutexType::Lock lock(mutex_);
    if (!appenders_.empty()) {
      for (auto& i : appenders_) {
        i->log(self, level, event);
      }
    } else if (root_) {
      root_->log(level, event);
    }
  }
}

void Logger::debug(LogEvent::ptr event) { log(LogLevel::DEBUG, event); }

void Logger::info(LogEvent::ptr event) { log(LogLevel::INFO, event); }

void Logger::warn(LogEvent::ptr event) { log(LogLevel::WARN, event); }

void Logger::error(LogEvent::ptr event) { log(LogLevel::ERROR, event); }

void Logger::fatal(LogEvent::ptr event) { log(LogLevel::FATAL, event); }

FileLogAppender::FileLogAppender(const std::string& filename)
    : filename_(filename) {
  reopen();
}

void FileLogAppender::log(std::shared_ptr<Logger> logger, LogLevel::Level level,
                          LogEvent::ptr event) {
  if (level >= level_) {
    uint64_t now = time(0);
    if (now != lastTime_) {
      reopen();
      lastTime_ = now;
    }

    MutexType::Lock lock(mutex_);
    if (!(filestream_ << formatter_->format(logger, level, event))) {
      std::cout << "error" << std::endl;
    }
  }
}

std::string FileLogAppender::toYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  node["type"] = "FileLogAppender";
  node["file"] = filename_;
  if (level_ != LogLevel::UNKNOWN) {
    node["level"] = LogLevel::ToString(level_);
  }
  if (hasFormatter_ && formatter_) {
    node["formatter"] = formatter_->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

bool FileLogAppender::reopen() {
  MutexType::Lock lock(mutex_);
  if (filestream_) {
    filestream_.close();
  }
  filestream_.open(filename_, std::ios::app);
  return !!filestream_;
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger,
                            LogLevel::Level level, LogEvent::ptr event) {
  if (level >= level_) {
    MutexType::Lock lock(mutex_);
    std::cout << formatter_->format(logger, level, event);
  }
}

std::string StdoutLogAppender::toYamlString() {
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  if (level_ != LogLevel::UNKNOWN) {
    node["level"] = LogLevel::ToString(level_);
  }
  if (hasFormatter_ && formatter_) {
    node["formatter"] = formatter_->getPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern) : pattern_(pattern) {
  init();
}

std::string LogFormatter::format(Logger::ptr logger, LogLevel::Level level,
                                 LogEvent::ptr event) {
  std::stringstream ss;
  for (auto& i : items_) {
    i->format(ss, logger, level, event);
  }
  return ss.str();
}

void LogFormatter::init() {
  // str, format, type
  std::vector<std::tuple<std::string, std::string, int> > vec;
  std::string nstr;
  for (size_t i = 0; i < pattern_.size(); ++i) {
    if (pattern_[i] != '%') {
      nstr.append(1, pattern_[i]);
      continue;
    }

    if ((i + 1) < pattern_.size()) {
      if (pattern_[i + 1] == '%') {
        nstr.append(1, '%');
        continue;
      }
    }

    size_t n = i + 1;
    int fmt_status = 0;
    size_t fmt_begin = 0;

    std::string str;
    std::string fmt;
    while (n < pattern_.size()) {
      if (!fmt_status &&
          (!isalpha(pattern_[n]) && pattern_[n] != '{' && pattern_[n] != '}')) {
        str = pattern_.substr(i + 1, n - i - 1);
        break;
      }
      if (fmt_status == 0) {
        if (pattern_[n] == '{') {
          str = pattern_.substr(i + 1, n - i - 1);
          // std::cout << "*" << str << std::endl;
          fmt_status = 1;  // 解析格式
          fmt_begin = n;
          ++n;
          continue;
        }
      } else if (fmt_status == 1) {
        if (pattern_[n] == '}') {
          fmt = pattern_.substr(fmt_begin + 1, n - fmt_begin - 1);
          // std::cout << "#" << fmt << std::endl;
          fmt_status = 0;
          ++n;
          break;
        }
      }
      ++n;
      if (n == pattern_.size()) {
        if (str.empty()) {
          str = pattern_.substr(i + 1);
        }
      }
    }

    if (fmt_status == 0) {
      if (!nstr.empty()) {
        vec.push_back(std::make_tuple(nstr, std::string(), 0));
        nstr.clear();
      }
      vec.push_back(std::make_tuple(str, fmt, 1));
      i = n - 1;
    } else if (fmt_status == 1) {
      std::cout << "pattern parse error: " << pattern_ << " - "
                << pattern_.substr(i) << std::endl;
      error_ = true;
      vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
    }
  }

  if (!nstr.empty()) {
    vec.push_back(std::make_tuple(nstr, "", 0));
  }
  static std::map<std::string,
                  std::function<FormatItem::ptr(const std::string& str)> >
      s_format_items = {
#define XX(str, C)                                                           \
  {                                                                          \
#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt)); } \
  }

          XX(m, MessageFormatItem),  XX(p, LevelFormatItem),
          XX(r, ElapseFormatItem),   XX(c, NameFormatItem),
          XX(t, ThreadIdFormatItem), XX(n, NewLineFormatItem),
          XX(d, DateTimeFormatItem), XX(f, FilenameFormatItem),
          XX(l, LineFormatItem),     XX(T, TabFormatItem),
          XX(F, FiberIdFormatItem),
#undef XX
      };

  for (auto& i : vec) {
    if (std::get<2>(i) == 0) {
      items_.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
    } else {
      auto it = s_format_items.find(std::get<0>(i));
      if (it == s_format_items.end()) {
        items_.push_back(FormatItem::ptr(
            new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
        error_ = true;
      } else {
        items_.push_back(it->second(std::get<1>(i)));
      }
    }
  }
}

LoggerManager::LoggerManager() {
  root_.reset(new Logger());
  root_->addAppender(LogAppender::ptr(new StdoutLogAppender));

  loggers_[root_->name_] = root_;

  init();
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
  MutexType::Lock lock(mutex_);
  auto it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }
  Logger::ptr logger(new Logger(name));
  logger->root_ = root_;
  loggers_[name] = logger;
  return logger;
}

struct LogAppenderDefine {
  int type = 0;  // 1 File, 2 Stdout
  LogLevel::Level level = LogLevel::UNKNOWN;
  std::string formatter;
  std::string file;

  bool operator==(const LogAppenderDefine& oth) const {
    return type == oth.type && level == oth.level &&
           formatter == oth.formatter && file == oth.file;
  }
};

struct LogDefine {
  std::string name;
  LogLevel::Level level = LogLevel::UNKNOWN;
  std::string formatter;
  std::vector<LogAppenderDefine> appenders;

  bool operator==(const LogDefine& oth) const {
    return name == oth.name && level == oth.level &&
           formatter == oth.formatter && appenders == appenders;
  }

  bool operator<(const LogDefine& oth) const { return name < oth.name; }
};

template <>
class LexicalCast<std::string, std::set<LogDefine> > {
 public:
  std::set<LogDefine> operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);
    std::set<LogDefine> vec;
    for (size_t i = 0; i < node.size(); ++i) {
      auto n = node[i];
      if (!n["name"].IsDefined()) {
        std::cout << "log config error: name is null, " << n << std::endl;
        continue;
      }

      LogDefine ld;
      ld.name = n["name"].as<std::string>();
      ld.level = LogLevel::FromString(
          n["level"].IsDefined() ? n["level"].as<std::string>() : "");
      if (n["formatter"].IsDefined()) {
        ld.formatter = n["formatter"].as<std::string>();
      }

      if (n["appenders"].IsDefined()) {
        for (size_t x = 0; x < n["appenders"].size(); ++x) {
          auto a = n["appenders"][x];
          if (!a["type"].IsDefined()) {
            std::cout << "log config error: appender type is null, " << a
                      << std::endl;
            continue;
          }
          std::string type = a["type"].as<std::string>();
          LogAppenderDefine lad;
          if (type == "FileLogAppender") {
            lad.type = 1;
            if (!a["file"].IsDefined()) {
              std::cout << "log config error: fileappender file is null, " << a
                        << std::endl;
              continue;
            }
            lad.file = a["file"].as<std::string>();
            if (a["formatter"].IsDefined()) {
              lad.formatter = a["formatter"].as<std::string>();
            }
          } else if (type == "StdoutLogAppender") {
            lad.type = 2;
          } else {
            std::cout << "log config error: appender type is invalid, " << a
                      << std::endl;
            continue;
          }

          ld.appenders.push_back(lad);
        }
      }
      vec.insert(ld);
    }
    return vec;
  }
};

template <>
class LexicalCast<std::set<LogDefine>, std::string> {
 public:
  std::string operator()(const std::set<LogDefine>& v) {
    YAML::Node node;
    for (auto& i : v) {
      YAML::Node n;
      n["name"] = i.name;
      if (i.level != LogLevel::UNKNOWN) {
        n["level"] = LogLevel::ToString(i.level);
      }
      if (i.formatter.empty()) {
        n["formatter"] = i.formatter;
      }

      for (auto& a : i.appenders) {
        YAML::Node na;
        if (a.type == 1) {
          na["type"] = "FileLogAppender";
          na["file"] = a.file;
        } else if (a.type == 2) {
          na["type"] = "StdoutLogAppender";
        }
        if (a.level != LogLevel::UNKNOWN) {
          na["level"] = LogLevel::ToString(a.level);
        }

        if (!a.formatter.empty()) {
          na["formatter"] = a.formatter;
        }

        n["appenders"].push_back(na);
      }
      node.push_back(n);
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

gudov::ConfigVar<std::set<LogDefine> >::ptr g_log_defines =
    gudov::Config::Lookup("logs", std::set<LogDefine>(), "logs config");

struct LogIniter {
  LogIniter() {
    g_log_defines->addListener([](const std::set<LogDefine>& old_value,
                                  const std::set<LogDefine>& new_value) {
      GUDOV_LOG_INFO(GUDOV_LOG_ROOT()) << "on_logger_conf_changed";
      for (auto& i : new_value) {
        auto it = old_value.find(i);
        gudov::Logger::ptr logger;
        if (it == old_value.end()) {
          // 新增logger
          logger = GUDOV_LOG_NAME(i.name);
        } else {
          if (!(i == *it)) {
            // 修改的logger
            logger = GUDOV_LOG_NAME(i.name);
          }
        }
        logger->setLevel(i.level);
        if (!i.formatter.empty()) {
          logger->setFormatter(i.formatter);
        }

        logger->clearAppenders();
        for (auto& a : i.appenders) {
          gudov::LogAppender::ptr ap;
          if (a.type == 1) {
            ap.reset(new FileLogAppender(a.file));
          } else if (a.type == 2) {
            ap.reset(new StdoutLogAppender);
          }
          ap->setLevel(a.level);
          if (!a.formatter.empty()) {
            LogFormatter::ptr fmt(new LogFormatter(a.formatter));
            if (!fmt->isError()) {
              ap->setFormatter(fmt);
            } else {
              std::cout << "log.name=" << i.name << " appender type=" << a.type
                        << " formatter=" << a.formatter << " is invalid"
                        << std::endl;
            }
          }
          logger->addAppender(ap);
        }
      }

      for (auto& i : old_value) {
        auto it = new_value.find(i);
        if (it == new_value.end()) {
          // 删除logger
          auto logger = GUDOV_LOG_NAME(i.name);
          logger->setLevel((LogLevel::Level)0);
          logger->clearAppenders();
        }
      }
    });
  }
};

static LogIniter __log_init;

std::string LoggerManager::toYamlString() {
  MutexType::Lock lock(mutex_);
  YAML::Node node;
  for (auto& i : loggers_) {
    node.push_back(YAML::Load(i.second->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void LoggerManager::init() {}

}  // namespace gudov
