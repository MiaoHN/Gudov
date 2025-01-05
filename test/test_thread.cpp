#include <gtest/gtest.h>
#include <unistd.h>
#include <yaml-cpp/null.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "gudov/gudov.h"
#include "gudov/log.h"

// 全局计数器，用于验证线程执行的正确性
std::atomic<int> counter(0);

// 测试线程创建和运行
TEST(ThreadTest, ThreadExecution) {
  auto func = []() {
    for (int i = 0; i < 5; ++i) {
      counter++;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };

  // 创建线程
  gudov::Thread::ptr thread = std::make_shared<gudov::Thread>(func, "TestThread");

  // 等待线程执行完成
  thread->Join();

  // 验证计数器
  EXPECT_EQ(counter, 5);
}

// 测试线程名称
TEST(ThreadTest, ThreadName) {
  auto func = []() { EXPECT_EQ(gudov::Thread::GetRunningThreadName(), "NamedThread"); };

  gudov::Thread::SetRunningThreadName("NamedThread");
  EXPECT_EQ(gudov::Thread::GetRunningThreadName(), "NamedThread");

  gudov::Thread::ptr thread = std::make_shared<gudov::Thread>(func, "NamedThread");
  thread->Join();
}

// 测试多个线程并发
TEST(ThreadTest, MultipleThreads) {
  std::atomic<int> sum(0);
  const int        num_threads = 10;

  auto func = [&]() {
    for (int i = 0; i < 100; ++i) {
      sum++;
    }
  };

  // 创建多个线程
  std::vector<gudov::Thread::ptr> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(std::make_shared<gudov::Thread>(func, "WorkerThread"));
  }

  // 等待所有线程完成
  for (auto& thread : threads) {
    thread->Join();
  }

  // 验证总和
  EXPECT_EQ(sum, num_threads * 100);
}

// 测试线程同步
TEST(ThreadTest, SemaphoreSync) {
  gudov::Semaphore semaphore(0);
  bool             task_done = false;

  auto func = [&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    task_done = true;
    semaphore.Notify();
  };

  gudov::Thread::ptr thread = std::make_shared<gudov::Thread>(func, "SyncThread");

  semaphore.Wait();  // 等待线程完成任务
  EXPECT_TRUE(task_done);

  thread->Join();
}

// 测试线程 ID
TEST(ThreadTest, ThreadId) {
  auto func = []() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); };

  gudov::Thread::ptr thread = std::make_shared<gudov::Thread>(func, "IDThread");

  EXPECT_GT(thread->GetID(), 0);  // 验证线程 ID 有效
  thread->Join();
}
