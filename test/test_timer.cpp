#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <thread>

#include "gudov/gudov.h"
#include "gudov/timer.h"

using namespace gudov;

class TimerManagerTest : public TimerManager {
 protected:
  void OnTimerInsertedAtFront() override {
    // 在定时器插入到最前面时的回调，模拟 tickling。
    tickled_ = true;
  }
};

// 测试 TimerManager 基本功能
TEST(TimerManagerTest, AddTimerTest) {
  TimerManagerTest timerManager;

  // 定义一个原子计数器用于回调测试
  std::atomic<int> counter{0};

  // 添加一个非循环定时器，50ms 后执行
  timerManager.AddTimer(50, [&]() {
    counter++;
    std::cout << "Timer executed!" << std::endl;
  });

  // 获取到下一个定时器的超时时间
  uint64_t nextTimeout = timerManager.GetNextTimer();
  EXPECT_GE(nextTimeout, 50);  // 应该至少为 50ms

  // 等待 100ms，确保定时器触发
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 获取已到期的回调
  std::vector<std::function<void()>> expiredCallbacks;
  timerManager.ListExpiredCallbacks(expiredCallbacks);

  // 检查回调是否被正确调用
  EXPECT_EQ(expiredCallbacks.size(), 1);
  for (auto &cb : expiredCallbacks) {
    cb();  // 执行回调
  }
  EXPECT_EQ(counter.load(), 1);
}

// 测试循环定时器
TEST(TimerManagerTest, RecurringTimerTest) {
  TimerManagerTest timerManager;

  // 定义一个原子计数器用于回调测试
  std::atomic<int> counter{0};

  // 添加一个循环定时器，50ms 后执行，且每 50ms 执行一次
  auto timer = timerManager.AddTimer(
      50,
      [&]() {
        counter++;
        std::cout << "Recurring timer executed: " << counter << std::endl;
      },
      true);

  int repeat_times = 8;
  for (int i = 0; i < repeat_times; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(200 / repeat_times)));
    // 获取已到期的回调
    std::vector<std::function<void()>> expiredCallbacks;

    timerManager.ListExpiredCallbacks(expiredCallbacks);

    // 执行所有过期的回调
    for (auto &cb : expiredCallbacks) {
      cb();
    }
  }

  // 检查回调是否被正确调用多次
  EXPECT_GE(counter.load(), 3);  // 应至少被调用 3 次
}

// 测试取消定时器
TEST(TimerManagerTest, CancelTimerTest) {
  TimerManagerTest timerManager;

  // 定义一个原子计数器用于回调测试
  std::atomic<int> counter{0};

  // 添加一个非循环定时器，50ms 后执行
  auto timer = timerManager.AddTimer(50, [&]() {
    counter++;
    std::cout << "Timer executed!" << std::endl;
  });

  // 取消定时器
  timer->Cancel();

  // 等待 100ms，确保定时器原本应该触发
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // 获取已到期的回调
  std::vector<std::function<void()>> expiredCallbacks;
  timerManager.ListExpiredCallbacks(expiredCallbacks);

  // 检查回调列表是否为空（因为定时器被取消了）
  EXPECT_EQ(expiredCallbacks.size(), 0);
  EXPECT_EQ(counter.load(), 0);
}

// 测试刷新定时器
TEST(TimerManagerTest, RefreshTimerTest) {
  TimerManagerTest timerManager;

  // 定义一个原子计数器用于回调测试
  std::atomic<int> counter{0};

  // 添加一个非循环定时器，100ms 后执行
  auto timer = timerManager.AddTimer(100, [&]() {
    counter++;
    std::cout << "Timer executed!" << std::endl;
  });

  // 在 50ms 时刷新定时器，让它重新计时为 100ms
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  timer->Refresh();

  // 等待 120ms，确保定时器触发
  std::this_thread::sleep_for(std::chrono::milliseconds(120));

  // 获取已到期的回调
  std::vector<std::function<void()>> expiredCallbacks;
  timerManager.ListExpiredCallbacks(expiredCallbacks);

  // 检查回调是否被正确调用
  EXPECT_EQ(expiredCallbacks.size(), 1);
  for (auto &cb : expiredCallbacks) {
    cb();
  }
  EXPECT_EQ(counter.load(), 1);
}

// 测试重置定时器
TEST(TimerManagerTest, ResetTimerTest) {
  TimerManagerTest timerManager;

  // 定义一个原子计数器用于回调测试
  std::atomic<int> counter{0};

  // 添加一个非循环定时器，100ms 后执行
  auto timer = timerManager.AddTimer(100, [&]() {
    counter++;
    std::cout << "Timer executed!" << std::endl;
  });

  // 在 50ms 时重置定时器，让它重新计时为 200ms
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  timer->Reset(200, true);

  // 等待 250ms，确保定时器触发
  std::this_thread::sleep_for(std::chrono::milliseconds(250));

  // 获取已到期的回调
  std::vector<std::function<void()>> expiredCallbacks;
  timerManager.ListExpiredCallbacks(expiredCallbacks);

  // 检查回调是否被正确调用
  EXPECT_EQ(expiredCallbacks.size(), 1);
  for (auto &cb : expiredCallbacks) {
    cb();
  }
  EXPECT_EQ(counter.load(), 1);
}
