#include "log.h"

#include <cstring>
#include <ctime>
#include <functional>
#include <iostream>
#include <map>

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
    os << logger->getName();
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

void Logger::addAppender(LogAppender::ptr appender) {
  if (!appender->getFormatter()) {
    appender->setFormatter(formatter_);
  }
  appenders_.push_back(appender);
}

void Logger::delAppender(LogAppender::ptr appender) {
  for (auto it = appenders_.begin(); it != appenders_.end(); ++it) {
    if (*it == appender) {
      appenders_.erase(it);
      break;
    }
  }
}

void Logger::log(LogLevel::Level level, LogEvent::ptr event) {
  if (level >= level_) {
    auto self = shared_from_this();
    for (auto& i : appenders_) {
      i->log(self, level, event);
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
    filestream_ << formatter_->format(logger, level, event);
  }
}

bool FileLogAppender::reopen() {
  if (filestream_) {
    filestream_.close();
  }
  filestream_.open(filename_, std::ios::app);
  return !!filestream_;
}

void StdoutLogAppender::log(std::shared_ptr<Logger> logger,
                            LogLevel::Level level, LogEvent::ptr event) {
  if (level >= level_) {
    std::cout << formatter_->format(logger, level, event);
  }
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
          fmt_status = 1;  //解析格式
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
      } else {
        items_.push_back(it->second(std::get<1>(i)));
      }
    }
  }
}

LoggerManager::LoggerManager() {
  root_.reset(new Logger());
  root_->addAppender(LogAppender::ptr(new StdoutLogAppender));
}

Logger::ptr LoggerManager::getLogger(const std::string& name) {
  auto it = loggers_.find(name);
  return it == loggers_.end() ? root_ : it->second;
}

}  // namespace gudov
