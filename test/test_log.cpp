#include <iostream>

#include "gudov/log.h"
#include "gudov/util.h"

int main(int argc, char const *argv[]) {
  gudov::Logger::ptr logger(new gudov::Logger);
  logger->addAppender(gudov::LogAppender::ptr(new gudov::StdoutLogAppender));

  gudov::FileLogAppender::ptr fileAppender(
      new gudov::FileLogAppender("./log.txt"));

  gudov::LogFormatter::ptr fmt(new gudov::LogFormatter("%d%T%m%n"));
  fileAppender->setFormatter(fmt);

  fileAppender->setLevel(gudov::LogLevel::Level::ERROR);

  logger->addAppender(fileAppender);

  std::cout << "Hello gudov log" << std::endl;

  LOG_INFO(logger) << "test macro";
  LOG_ERROR(logger) << "test macro error";

  LOG_FMT_ERROR(logger, "test macro fmt error %s", "aa");
  return 0;
}
