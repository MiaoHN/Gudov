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
  gudov::Scheduler scheduler(1, false, "SingleThread");

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
  gudov::Scheduler scheduler(4, false, "MultiThread");

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

// 测试任务调度到特定线程
TEST(SchedulerTest, DISABLED_TaskAssignedToSpecificThread) {
  gudov::Scheduler scheduler(4, false, "SpecificThread");

  std::vector<int> thread_ids;
  scheduler.Schedule(
      [&]() {
        thread_ids.push_back(gudov::GetThreadId());
        std::cout << "Task executed on thread: " << gudov::GetThreadId() << std::endl;
      },
      0);  // 指定在线程 0 执行

  scheduler.Start();
  scheduler.Stop();

  ASSERT_EQ(thread_ids.size(), 1);
  EXPECT_EQ(thread_ids[0], scheduler.GetScheduler()->GetMainFiber()->GetID());
}

// 测试空闲协程处理
TEST(SchedulerTest, DISABLED_IdleFiberHandling) {
  gudov::Scheduler scheduler(1, false, "IdleTest");

  bool idle_called = false;
  scheduler.Schedule([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    idle_called = true;
    std::cout << "Idle fiber triggered" << std::endl;
  });

  scheduler.Start();
  scheduler.Stop();

  EXPECT_TRUE(idle_called);
}

// 测试调度器的自动停止功能
TEST(SchedulerTest, AutoStopScheduler) {
  gudov::Scheduler scheduler(2, false, "AutoStop");

  for (int i = 0; i < 5; ++i) {
    scheduler.Schedule([]() {
      counter++;
      // std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });
  }

  scheduler.Start();
  scheduler.Stop();

  EXPECT_EQ(counter, 18);  // 前测试累加值（13）+ 5
}

// 测试多个调度器之间的独立性
TEST(SchedulerTest, MultipleSchedulers) {
  gudov::Scheduler scheduler1(2, false, "Scheduler1");
  gudov::Scheduler scheduler2(2, false, "Scheduler2");

  std::atomic<int> scheduler1_count(0);
  std::atomic<int> scheduler2_count(0);

  for (int i = 0; i < 5; ++i) {
    scheduler1.Schedule([&]() { scheduler1_count++; });
    scheduler2.Schedule([&]() { scheduler2_count++; });
  }

  scheduler1.Start();
  scheduler2.Start();

  scheduler1.Stop();
  scheduler2.Stop();

  EXPECT_EQ(scheduler1_count, 5);
  EXPECT_EQ(scheduler2_count, 5);
}

// 测试调度器的动态任务添加
TEST(SchedulerTest, DynamicTaskAddition) {
  gudov::Scheduler scheduler(2, false, "DynamicAdd");

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

  EXPECT_EQ(counter, 25);  // 前测试累加值（18）+ 5（原任务）+ 2（动态任务）
}
