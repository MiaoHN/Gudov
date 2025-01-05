#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "gudov/gudov.h"

// 全局计数器，用于验证任务执行的正确性
std::atomic<int> counter(0);

gudov::Logger::ptr g_logger = LOG_ROOT();

// 测试调度器的初始化和单线程调度
TEST(SchedulerTest, SingleThreadScheduler) {
  gudov::Scheduler scheduler(1, true, "SingleThread");

  scheduler.Schedule([]() {
    counter++;
    std::cout << "Task 1 executed" << std::endl;
  });

  scheduler.Schedule([]() {
    counter += 2;
    std::cout << "Task 2 executed" << std::endl;
  });

  scheduler.Start();
  scheduler.Stop();

  EXPECT_EQ(counter, 3);  // Task 1 + Task 2
}

// 测试多线程调度
TEST(SchedulerTest, MultiThreadScheduler) {
  gudov::Scheduler scheduler(4, true, "MultiThread");

  for (int i = 0; i < 10; ++i) {
    scheduler.Schedule([i]() {
      counter++;
      std::cout << "Task " << i + 1 << " executed" << std::endl;
    });
  }

  scheduler.Start();
  scheduler.Stop();

  EXPECT_EQ(counter, 13);  // 累加 10（10 个任务） + 3（前一个测试累加值）
}

// 测试调度器的动态任务添加
TEST(SchedulerTest, DynamicTaskAddition) {
  gudov::Scheduler scheduler(2, true, "DynamicAdd");

  for (int i = 0; i < 5; ++i) {
    scheduler.Schedule([i]() {
      std::cout << "Task " << i + 1 << " executed" << std::endl;
      counter++;
    });
  }

  scheduler.Start();

  scheduler.Schedule([]() {
    std::cout << "Newly added task executed" << std::endl;
    counter += 2;
  });

  scheduler.Stop();

  EXPECT_EQ(counter, 20);
}
