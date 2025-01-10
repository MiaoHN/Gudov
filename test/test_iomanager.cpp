#include <arpa/inet.h>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>

#include "gudov/gudov.h"
#include "gudov/iomanager.h"

using namespace gudov;

class IOManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 初始化 IOManager
    io_manager = std::make_shared<IOManager>(1, true, "TestIOManager");
    ASSERT_NE(io_manager, nullptr) << "Failed to initialize IOManager";
  }

  void TearDown() override { io_manager.reset(); }

  IOManager::ptr io_manager;
};

// 测试 AddEvent 功能
TEST_F(IOManagerTest, AddEventTest) {
  int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);  // 创建 eventfd
  ASSERT_GT(efd, 0) << "Failed to create eventfd";

  bool callback_executed = false;

  // 添加事件
  int result = io_manager->AddEvent(efd, IOManager::READ, [&]() { callback_executed = true; });
  EXPECT_EQ(result, 0) << "AddEvent failed";

  // 模拟触发事件
  uint64_t value = 1;
  ASSERT_EQ(write(efd, &value, sizeof(value)), sizeof(value)) << "Failed to write to eventfd";

  // 运行 IOManager
  io_manager->Stop();  // 停止前运行所有事件
  EXPECT_TRUE(callback_executed) << "Callback was not executed";

  close(efd);  // 关闭 eventfd
}

// 测试 DelEvent 功能
TEST_F(IOManagerTest, DelEventTest) {
  int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);  // 创建 eventfd
  ASSERT_GT(efd, 0) << "Failed to create eventfd";

  bool callback_executed = false;

  // 添加事件
  int result = io_manager->AddEvent(efd, IOManager::READ, [&]() { callback_executed = true; });
  EXPECT_EQ(result, 0) << "AddEvent failed";

  // 删除事件
  bool del_result = io_manager->DelEvent(efd, IOManager::READ);
  EXPECT_TRUE(del_result) << "DelEvent failed";

  // 模拟触发事件
  uint64_t value = 1;
  ASSERT_EQ(write(efd, &value, sizeof(value)), sizeof(value)) << "Failed to write to eventfd";

  // 运行 IOManager
  io_manager->Stop();
  EXPECT_FALSE(callback_executed) << "Callback should not be executed after event deletion";

  close(efd);  // 关闭 eventfd
}

// 测试 CancelEvent 功能
TEST_F(IOManagerTest, CancelEventTest) {
  int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);  // 创建 eventfd
  ASSERT_GT(efd, 0) << "Failed to create eventfd";

  bool callback_executed = false;

  // 添加事件
  int result = io_manager->AddEvent(efd, IOManager::READ, [&]() { callback_executed = true; });
  EXPECT_EQ(result, 0) << "AddEvent failed";

  // 取消事件
  bool cancel_result = io_manager->CancelEvent(efd, IOManager::READ);
  EXPECT_TRUE(cancel_result) << "CancelEvent failed";

  // 模拟触发事件
  uint64_t value = 1;
  ASSERT_EQ(write(efd, &value, sizeof(value)), sizeof(value)) << "Failed to write to eventfd";

  // 运行 IOManager
  io_manager->Stop();
  EXPECT_TRUE(callback_executed) << "Callback should be executed after event cancellation";

  close(efd);  // 关闭 eventfd
}

// 测试 CancelAll 功能
TEST_F(IOManagerTest, CancelAllTest) {
  int efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);  // 创建 eventfd
  ASSERT_GT(efd, 0) << "Failed to create eventfd";

  bool read_callback_executed  = false;
  bool write_callback_executed = false;

  // 添加读事件
  int read_result = io_manager->AddEvent(efd, IOManager::READ, [&]() { read_callback_executed = true; });
  EXPECT_EQ(read_result, 0) << "AddEvent (Read) failed";

  // 添加写事件
  int write_result = io_manager->AddEvent(efd, IOManager::WRITE, [&]() { write_callback_executed = true; });
  EXPECT_EQ(write_result, 0) << "AddEvent (Write) failed";

  // 取消所有事件
  bool cancel_all_result = io_manager->CancelAll(efd);
  EXPECT_TRUE(cancel_all_result) << "CancelAll failed";

  // 模拟触发事件
  uint64_t value = 1;
  ASSERT_EQ(write(efd, &value, sizeof(value)), sizeof(value)) << "Failed to write to eventfd";

  // 运行 IOManager
  io_manager->Stop();
  EXPECT_TRUE(read_callback_executed) << "Read callback should be executed after CancelAll";
  EXPECT_TRUE(write_callback_executed) << "Write callback should be executed after CancelAll";

  close(efd);  // 关闭 eventfd
}
