#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

// 假设 singleton.h, thread.h, util.h 已经包含在 gudov 头文件中
#include "gudov/log.h"  // 假设所有 gudov 相关的头文件都在这个头文件中
#include "gudov/util.h"

// Helper function to capture stdout output
std::string captureStdout(std::function<void()> func) {
  std::ostringstream oss;
  auto               oldBuf = std::cout.rdbuf(oss.rdbuf());
  func();
  std::cout.rdbuf(oldBuf);
  return oss.str();
}

// Test LogLevel functionality
TEST(LogLevelTest, ToStringAndFromString) {
  EXPECT_STREQ(gudov::LogLevel::ToString(gudov::LogLevel::DEBUG), "DEBUG");
  EXPECT_STREQ(gudov::LogLevel::ToString(gudov::LogLevel::INFO), "INFO");
  EXPECT_EQ(gudov::LogLevel::FromString("DEBUG"), gudov::LogLevel::DEBUG);
  EXPECT_EQ(gudov::LogLevel::FromString("ERROR"), gudov::LogLevel::ERROR);
}

// Test LogEvent creation and content
TEST(LogEventTest, CreateLogEvent) {
  auto                 logger = std::make_shared<gudov::Logger>("test_logger");
  gudov::LogEvent::ptr event =
      std::make_shared<gudov::LogEvent>(logger, gudov::LogLevel::INFO, "test_file.cpp", 10, 0, 12345, 1, 1617181920);

  EXPECT_EQ(event->GetFile(), std::string("test_file.cpp"));
  EXPECT_EQ(event->GetLine(), 10);
  EXPECT_EQ(event->GetThreadId(), 12345);
  EXPECT_EQ(event->GetFiberId(), 1);
  EXPECT_EQ(event->GetLevel(), gudov::LogLevel::INFO);

  event->GetSS() << "Test message";
  EXPECT_EQ(event->GetContent(), "Test message");
}

// Test LogFormatter
TEST(LogFormatterTest, FormatLogEvent) {
  auto                 formatter = std::make_shared<gudov::LogFormatter>("%d{%Y-%m-%d %H:%M:%S} %p %f:%l %m%n");
  auto                 logger    = std::make_shared<gudov::Logger>("test_logger");
  gudov::LogEvent::ptr event =
      std::make_shared<gudov::LogEvent>(logger, gudov::LogLevel::INFO, "test_file.cpp", 10, 0, 12345, 1, 1617181920);

  event->GetSS() << "Test message";
  std::string formatted = formatter->format(event);

  EXPECT_TRUE(formatted.find("INFO") != std::string::npos);
  EXPECT_TRUE(formatted.find("test_file.cpp") != std::string::npos);
  EXPECT_TRUE(formatted.find("10") != std::string::npos);
  EXPECT_TRUE(formatted.find("Test message") != std::string::npos);
}

// Test StdoutLogAppender
TEST(StdoutLogAppenderTest, OutputLog) {
  auto appender = std::make_shared<gudov::StdoutLogAppender>();
  auto logger   = std::make_shared<gudov::Logger>("test_logger");
  logger->AddAppender(appender);

  auto captured_output = captureStdout([&]() { LOG_INFO(logger) << "Test stdout message"; });

  EXPECT_TRUE(captured_output.find("Test stdout message") != std::string::npos);
}

// Test FileLogAppender
TEST(FileLogAppenderTest, OutputLogToFile) {
  std::string filename = "test_log_file.log";
  {
    auto appender = std::make_shared<gudov::FileLogAppender>(filename);
    auto logger   = std::make_shared<gudov::Logger>("test_logger");
    logger->AddAppender(appender);

    LOG_INFO(logger) << "Test file message";
  }
  std::ifstream file(filename);
  ASSERT_TRUE(file.is_open());

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string content = buffer.str();

  EXPECT_TRUE(content.find("Test file message") != std::string::npos);

  file.close();
  std::remove(filename.c_str());
}

// Test LoggerManager
TEST(LoggerManagerTest, GetLoggerAndRoot) {
  auto root_logger = gudov::LoggerMgr::GetInstance()->GetRoot();
  EXPECT_EQ(root_logger->GetName(), "root");

  auto test_logger = gudov::LoggerMgr::GetInstance()->GetLogger("test_logger");
  EXPECT_EQ(test_logger->GetName(), "test_logger");
}

// Test Logger log levels
TEST(LoggerTest, LogLevelFiltering) {
  auto logger   = std::make_shared<gudov::Logger>("test_logger");
  auto appender = std::make_shared<gudov::StdoutLogAppender>();
  logger->AddAppender(appender);

  logger->SetLevel(gudov::LogLevel::ERROR);

  auto captured_output = captureStdout([&]() {
    LOG_DEBUG(logger) << "Debug message";
    LOG_INFO(logger) << "Info message";
    LOG_ERROR(logger) << "Error message";
  });

  EXPECT_TRUE(captured_output.find("Error message") != std::string::npos);
  EXPECT_TRUE(captured_output.find("Debug message") == std::string::npos);
  EXPECT_TRUE(captured_output.find("Info message") == std::string::npos);
}
