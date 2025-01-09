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

LogEventWrap::~LogEventWrap() { event_->GetLogger()->log(event_); }

void LogEvent::format(const char* fmt, ...) {
  va_list al;
  va_start(al, fmt);
  format(fmt, al);
  va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
  char* buf = nullptr;
  int   len = vasprintf(&buf, fmt, al);
  if (len != -1) {
    ss_ << std::string(buf, len);
    free(buf);
  }
}

std::stringstream& LogEventWrap::GetSS() { return event_->GetSS(); }

void LogAppender::SetFormatter(LogFormatter::ptr formatter) {
  MutexType::Locker lock(mutex_);
  formatter_ = formatter;
}

LogFormatter::ptr LogAppender::GetFormatter() {
  MutexType::Locker lock(mutex_);
  return formatter_;
}

class MessageFormatItem : public LogFormatter::FormatItem {
 public:
  MessageFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetContent(); }
};

class LevelFormatItem : public LogFormatter::FormatItem {
 public:
  LevelFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << LogLevel::ToString(event->GetLevel()); }
};

class ElapseFormatItem : public LogFormatter::FormatItem {
 public:
  ElapseFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetElapse(); }
};

class NameFormatItem : public LogFormatter::FormatItem {
 public:
  NameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetLogger()->GetName(); }
};

class ThreadIdFormatItem : public LogFormatter::FormatItem {
 public:
  ThreadIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetThreadId(); }
};

class FiberIdFormatItem : public LogFormatter::FormatItem {
 public:
  FiberIdFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetFiberId(); }
};

class DateTimeFormatItem : public LogFormatter::FormatItem {
 public:
  DateTimeFormatItem(const std::string& format = "%Y-%m-%d %H:%M:%S") : format_(format) {
    if (format_.empty()) {
      format_ = "%Y-%m-%d %H:%M:%S";
    }
  }

  void format(std::ostream& os, LogEvent::ptr event) override {
    struct tm tm;
    time_t    time = event->GetTime();
    localtime_r(&time, &tm);
    char buf[64];
    strftime(buf, sizeof(buf), format_.c_str(), &tm);
    os << buf;
  }

 private:
  std::string format_;
};

class FilenameFormatItem : public LogFormatter::FormatItem {
 public:
  FilenameFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetFile(); }
};

class LineFormatItem : public LogFormatter::FormatItem {
 public:
  LineFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << event->GetLine(); }
};

class NewLineFormatItem : public LogFormatter::FormatItem {
 public:
  NewLineFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << std::endl; }
};

class StringFormatItem : public LogFormatter::FormatItem {
 public:
  StringFormatItem(const std::string& str) : string_(str) {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << string_; }

 private:
  std::string string_;
};

class TabFormatItem : public LogFormatter::FormatItem {
 public:
  TabFormatItem(const std::string& str = "") {}
  void format(std::ostream& os, LogEvent::ptr event) override { os << "\t"; }
};

LogEvent::LogEvent(std::shared_ptr<Logger> logger, LogLevel::Level level, const char* filename, int32_t line,
                   uint32_t elapse, uint32_t threadId, uint32_t fiberId, uint64_t time)
    : file_(filename),
      line_(line),
      elapse_(elapse),
      thread_id_(threadId),
      fiber_id_(fiberId),
      time_(time),
      logger_(logger),
      level_(level) {}

Logger::Logger(const std::string& name) : name_(name), level_(LogLevel::DEBUG) {
  formatter_.reset(new LogFormatter("%d{%Y-%m-%d %H:%M:%S}%T%t%T%F%T[%p]%T[%c]%T%f:%l%T%m%n"));
}

void Logger::SetFormatter(LogFormatter::ptr val) {
  MutexType::Locker lock(mutex_);
  formatter_ = val;

  for (auto& i : appenders_) {
    MutexType::Locker ll(i->mutex_);
    if (!i->formatter_) {
      i->formatter_ = formatter_;
    }
  }
}

void Logger::SetFormatter(const std::string& val) {
  LogFormatter::ptr newVal(new LogFormatter(val));
  if (newVal->IsError()) {
    std::cout << "Logger SetFormatter name=" << name_ << " value=" << val << " invalid formatter" << std::endl;
    return;
  }
  SetFormatter(newVal);
}

std::string Logger::toYamlString() {
  MutexType::Locker lock(mutex_);
  YAML::Node      node;
  node["name"] = name_;
  if (level_ != LogLevel::UNKNOWN) {
    node["level"] = LogLevel::ToString(level_);
  }
  if (formatter_) {
    node["formatter"] = formatter_->GetPattern();
  }

  for (auto& i : appenders_) {
    node["appenders"].push_back(YAML::Load(i->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogFormatter::ptr Logger::GetFormatter() {
  MutexType::Locker lock(mutex_);
  return formatter_;
}

void Logger::addAppender(LogAppender::ptr appender) {
  MutexType::Locker lock(mutex_);
  if (!appender->GetFormatter()) {
    MutexType::Locker ll(appender->mutex_);
    appender->formatter_ = formatter_;
  }
  appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
  MutexType::Locker lock(mutex_);
  for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
    if (*it == appender) {
      appenders_.erase(it);
      break;
    }
  }
}

void Logger::clearAppenders() {
  MutexType::Locker lock(mutex_);
  appenders_.clear();
}

void Logger::log(LogEvent::ptr event) {
  if (event->GetLevel() >= level_) {
    auto            self = shared_from_this();
    MutexType::Locker lock(mutex_);
    if (!appenders_.empty()) {
      for (auto& i : appenders_) {
        i->output(event);
      }
    } else if (root_) {
      root_->log(event);
    }
  }
}

FileLogAppender::FileLogAppender(const std::string& filename, LogLevel::Level level)
    : LogAppender(level), filename_(filename) {
  reopen();
}

void FileLogAppender::output(LogEvent::ptr event) {
  uint64_t now = time(0);
  if (now != last_time_) {
    reopen();
    last_time_ = now;
  }

  MutexType::Locker lock(mutex_);
  if (!(filestream_ << formatter_->format(event))) {
    std::cout << "error" << std::endl;
  }
}

std::string FileLogAppender::toYamlString() {
  MutexType::Locker lock(mutex_);
  YAML::Node      node;
  node["type"] = "FileLogAppender";
  node["file"] = filename_;
  if (level_ != LogLevel::UNKNOWN) {
    node["level"] = LogLevel::ToString(level_);
  }
  if (formatter_) {
    node["formatter"] = formatter_->GetPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

bool FileLogAppender::reopen() {
  MutexType::Locker lock(mutex_);
  if (filestream_) {
    filestream_.close();
  }
  filestream_.open(filename_, std::ios::app);
  return !!filestream_;
}

StdoutLogAppender::StdoutLogAppender(LogLevel::Level level) : LogAppender(level) {}

void StdoutLogAppender::output(LogEvent::ptr event) {
  if (event->GetLevel() >= level_) {
    MutexType::Locker lock(mutex_);
    std::cout << formatter_->format(event);
  }
}

std::string StdoutLogAppender::toYamlString() {
  YAML::Node node;
  node["type"] = "StdoutLogAppender";
  if (level_ != LogLevel::UNKNOWN) {
    node["level"] = LogLevel::ToString(level_);
  }
  if (formatter_) {
    node["formatter"] = formatter_->GetPattern();
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

LogFormatter::LogFormatter(const std::string& pattern) : pattern_(pattern) { Init(); }

std::string LogFormatter::format(LogEvent::ptr event) {
  std::stringstream ss;
  for (auto& i : items_) {
    i->format(ss, event);
  }
  return ss.str();
}

void LogFormatter::Init() {
  // str, format, type
  std::vector<std::tuple<std::string, std::string, int> > vec;
  std::string                                             nstr;
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

    size_t n          = i + 1;
    int    fmt_status = 0;
    size_t fmt_begin  = 0;

    std::string str;
    std::string fmt;
    while (n < pattern_.size()) {
      if (!fmt_status && (!isalpha(pattern_[n]) && pattern_[n] != '{' && pattern_[n] != '}')) {
        str = pattern_.substr(i + 1, n - i - 1);
        break;
      }
      if (fmt_status == 0) {
        if (pattern_[n] == '{') {
          str = pattern_.substr(i + 1, n - i - 1);
          // std::cout << "*" << str << std::endl;
          fmt_status = 1;  // 解析格式
          fmt_begin  = n;
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
      std::cout << "pattern parse error: " << pattern_ << " - " << pattern_.substr(i) << std::endl;
      error_ = true;
      vec.push_back(std::make_tuple("<<pattern_error>>", fmt, 0));
    }
  }

  if (!nstr.empty()) {
    vec.push_back(std::make_tuple(nstr, "", 0));
  }
  static std::map<std::string, std::function<FormatItem::ptr(const std::string& str)> > s_format_items = {
#define XX(str, C)                                                           \
  {                                                                          \
#str, [](const std::string& fmt) { return FormatItem::ptr(new C(fmt)); } \
  }

      XX(m, MessageFormatItem),  XX(p, LevelFormatItem),   XX(r, ElapseFormatItem),   XX(c, NameFormatItem),
      XX(t, ThreadIdFormatItem), XX(n, NewLineFormatItem), XX(d, DateTimeFormatItem), XX(f, FilenameFormatItem),
      XX(l, LineFormatItem),     XX(T, TabFormatItem),     XX(F, FiberIdFormatItem),
#undef XX
  };

  for (auto& i : vec) {
    if (std::get<2>(i) == 0) {
      items_.push_back(FormatItem::ptr(new StringFormatItem(std::get<0>(i))));
    } else {
      auto it = s_format_items.find(std::get<0>(i));
      if (it == s_format_items.end()) {
        items_.push_back(FormatItem::ptr(new StringFormatItem("<<error_format %" + std::get<0>(i) + ">>")));
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

  Init();
}

Logger::ptr LoggerManager::GetLogger(const std::string& name) {
  MutexType::Locker lock(mutex_);
  auto            it = loggers_.find(name);
  if (it != loggers_.end()) {
    return it->second;
  }
  Logger::ptr logger(new Logger(name));
  logger->root_  = root_;
  loggers_[name] = logger;
  return logger;
}

struct LogAppenderConfig {
  int             type  = 0;  // 1 File, 2 Stdout
  LogLevel::Level level = LogLevel::UNKNOWN;
  std::string     formatter;
  std::string     file;

  bool operator==(const LogAppenderConfig& oth) const {
    return type == oth.type && level == oth.level && formatter == oth.formatter && file == oth.file;
  }
};

struct LogConfig {
  std::string                    name;
  LogLevel::Level                level = LogLevel::UNKNOWN;
  std::string                    formatter;
  std::vector<LogAppenderConfig> appenders;

  bool operator==(const LogConfig& oth) const {
    return name == oth.name && level == oth.level && formatter == oth.formatter && appenders == appenders;
  }

  bool operator<(const LogConfig& oth) const { return name < oth.name; }
};

template <>
class LexicalCast<std::string, std::set<LogConfig> > {
 public:
  std::set<LogConfig> operator()(const std::string& v) {
    YAML::Node          node = YAML::Load(v);
    std::set<LogConfig> vec;
    for (size_t i = 0; i < node.size(); ++i) {
      auto n = node[i];
      if (!n["name"].IsDefined()) {
        std::cout << "log config error: name is null, " << n << std::endl;
        continue;
      }

      LogConfig ld;
      ld.name  = n["name"].as<std::string>();
      ld.level = LogLevel::FromString(n["level"].IsDefined() ? n["level"].as<std::string>() : "");
      if (n["formatter"].IsDefined()) {
        ld.formatter = n["formatter"].as<std::string>();
      }

      if (n["appenders"].IsDefined()) {
        for (size_t x = 0; x < n["appenders"].size(); ++x) {
          auto a = n["appenders"][x];
          if (!a["type"].IsDefined()) {
            std::cout << "log config error: appender type is null, " << a << std::endl;
            continue;
          }
          std::string       type = a["type"].as<std::string>();
          LogAppenderConfig lad;
          if (type == "FileLogAppender") {
            lad.type = 1;
            if (!a["file"].IsDefined()) {
              std::cout << "log config error: fileappender file is null, " << a << std::endl;
              continue;
            }
            lad.file = a["file"].as<std::string>();
            if (a["formatter"].IsDefined()) {
              lad.formatter = a["formatter"].as<std::string>();
            }
          } else if (type == "StdoutLogAppender") {
            lad.type = 2;
          } else {
            std::cout << "log config error: appender type is invalid, " << a << std::endl;
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
class LexicalCast<std::set<LogConfig>, std::string> {
 public:
  std::string operator()(const std::set<LogConfig>& v) {
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

gudov::ConfigVar<std::set<LogConfig> >::ptr g_log_defines =
    gudov::Config::Lookup("logs", std::set<LogConfig>(), "logs config");

struct LogIniter {
  LogIniter() {
    g_log_defines->addListener([](const std::set<LogConfig>& old_value, const std::set<LogConfig>& new_value) {
      LOG_INFO(LOG_ROOT()) << "on_logger_conf_changed";
      for (auto& i : new_value) {
        auto               it = old_value.find(i);
        gudov::Logger::ptr logger;
        if (it == old_value.end()) {
          // 新增logger
          logger = LOG_NAME(i.name);
        } else {
          if (!(i == *it)) {
            // 修改的logger
            logger = LOG_NAME(i.name);
          }
        }
        logger->SetLevel(i.level);
        if (!i.formatter.empty()) {
          logger->SetFormatter(i.formatter);
        }

        logger->clearAppenders();
        for (auto& appender : i.appenders) {
          gudov::LogAppender::ptr log_appender;
          if (appender.type == 1) {
            log_appender.reset(new FileLogAppender(appender.file));
          } else if (appender.type == 2) {
            log_appender.reset(new StdoutLogAppender);
          }
          log_appender->SetLevel(appender.level);
          if (!appender.formatter.empty()) {
            LogFormatter::ptr fmt(new LogFormatter(appender.formatter));
            if (!fmt->IsError()) {
              log_appender->SetFormatter(fmt);
            } else {
              std::cout << "log.name=" << i.name << " appender type=" << appender.type
                        << " formatter=" << appender.formatter << " is invalid" << std::endl;
            }
          }
          logger->addAppender(log_appender);
        }
      }

      for (auto& i : old_value) {
        auto it = new_value.find(i);
        if (it == new_value.end()) {
          // 删除logger
          auto logger = LOG_NAME(i.name);
          logger->SetLevel((LogLevel::Level)0);
          logger->clearAppenders();
        }
      }
    });
  }
};

static LogIniter __log_init;

std::string LoggerManager::toYamlString() {
  MutexType::Locker lock(mutex_);
  YAML::Node      node;
  for (auto& i : loggers_) {
    node.push_back(YAML::Load(i.second->toYamlString()));
  }
  std::stringstream ss;
  ss << node;
  return ss.str();
}

void LoggerManager::Init() {}

}  // namespace gudov
