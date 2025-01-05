#include <gtest/gtest.h>

#include <atomic>
#include <iostream>
#include <thread>

#include "gudov/gudov.h"
#include "gudov/scheduler.h"

// 全局计数器用于测试 Fiber 的执行效果
std::atomic<int> fiber_counter(0);

// 测试 Fiber 创建和执行
TEST(FiberTest, FiberExecution) {
  gudov::Fiber::GetRunningFiber();  // 初始化主协程

  // 定义一个 Fiber 执行的函数
  auto func = []() { fiber_counter += 10; };

  // 创建 Fiber，并执行
  gudov::Fiber::ptr fiber = std::make_shared<gudov::Fiber>(func);
  EXPECT_EQ(fiber->GetState(), gudov::Fiber::Ready);

  fiber->Resume();  // 进入协程执行
  EXPECT_EQ(fiber_counter, 10);
  EXPECT_EQ(fiber->GetState(), gudov::Fiber::Term);
}

// 测试 Fiber 的重置功能
TEST(FiberTest, FiberReset) {
  // 定义初始函数
  auto func1 = []() { fiber_counter += 1; };

  // 定义重置后的函数
  auto func2 = []() { fiber_counter += 2; };

  // 创建 Fiber，并执行第一个任务
  gudov::Fiber::ptr fiber = std::make_shared<gudov::Fiber>(func1);
  fiber->Resume();
  EXPECT_EQ(fiber_counter, 11);  // 前面的测试累积到 10，此处增加 1

  // 重置 Fiber，并执行第二个任务
  fiber->Reset(func2);
  fiber->Resume();
  EXPECT_EQ(fiber_counter, 13);  // 增加 2
}

// 测试 Fiber 的状态切换
TEST(FiberTest, FiberStateTransition) {
  auto func = []() {
    fiber_counter += 5;
    gudov::Fiber::GetRunningFiber()->Yield();  // 暂停当前 Fiber
    fiber_counter += 5;
  };

  gudov::Fiber::ptr fiber = std::make_shared<gudov::Fiber>(func);
  EXPECT_EQ(fiber->GetState(), gudov::Fiber::Ready);

  // 首次执行 Fiber，进入 Running 状态
  fiber->Resume();
  EXPECT_EQ(fiber_counter, 18);                       // 增加 5
  EXPECT_EQ(fiber->GetState(), gudov::Fiber::Ready);  // Yield 返回时应该是 Ready 状态

  // 再次执行 Fiber
  fiber->Resume();
  EXPECT_EQ(fiber_counter, 23);                      // 增加 5
  EXPECT_EQ(fiber->GetState(), gudov::Fiber::Term);  // 执行完成
}

// 测试多个 Fiber 的协作切换
TEST(FiberTest, MultiFiberSwitching) {
  std::atomic<int> shared_value(0);

  // 定义 Fiber1
  auto func1 = [&]() {
    shared_value += 1;
    gudov::Fiber::GetRunningFiber()->Yield();  // 切换出去
    shared_value += 2;
  };

  // 定义 Fiber2
  auto func2 = [&]() {
    shared_value += 10;
    gudov::Fiber::GetRunningFiber()->Yield();  // 切换出去
    shared_value += 20;
  };

  // 创建两个 Fiber
  gudov::Fiber::ptr fiber1 = std::make_shared<gudov::Fiber>(func1);
  gudov::Fiber::ptr fiber2 = std::make_shared<gudov::Fiber>(func2);

  // 切换进入第一个 Fiber
  fiber1->Resume();
  EXPECT_EQ(shared_value, 1);  // Fiber1 执行到 Yield

  // 切换进入第二个 Fiber
  fiber2->Resume();
  EXPECT_EQ(shared_value, 11);  // Fiber2 执行到 Yield

  // 再次切换进入第一个 Fiber
  fiber1->Resume();
  EXPECT_EQ(shared_value, 13);  // Fiber1 完成剩余任务

  // 再次切换进入第二个 Fiber
  fiber2->Resume();
  EXPECT_EQ(shared_value, 33);  // Fiber2 完成剩余任务
}

// 测试 Fiber 的 ID 和统计功能
TEST(FiberTest, FiberIdAndCount) {
  // Fiber 总数量应该初始为 0 或 1（主协程）
  uint64_t initial_count = gudov::Fiber::TotalFibers();

  auto func = []() { fiber_counter += 1; };

  // 创建 Fiber
  gudov::Fiber::ptr fiber = std::make_shared<gudov::Fiber>(func);
  EXPECT_EQ(fiber->GetID(), initial_count + 1);  // Fiber ID 自增
  EXPECT_EQ(gudov::Fiber::TotalFibers(), initial_count + 1);

  // 执行后 Fiber 数量不变（已完成不会减少计数）
  fiber->Resume();
  EXPECT_EQ(fiber_counter, 24);  // 累计执行
  EXPECT_EQ(gudov::Fiber::TotalFibers(), initial_count + 1);
}

// 测试 Fiber 主协程
TEST(FiberTest, MainFiber) {
  // 获取主协程
  gudov::Fiber::ptr main_fiber = gudov::Fiber::GetRunningFiber();

  // 确保主协程存在且状态为 Running
  EXPECT_NE(main_fiber, nullptr);
  EXPECT_EQ(main_fiber->GetState(), gudov::Fiber::Running);
}

// 测试静态方法 GetFiberId
TEST(FiberTest, FiberGetId) {
  auto fiber_id = gudov::Fiber::GetRunningFiberId();
  EXPECT_GT(fiber_id, 0);  // Fiber ID 应该是有效的正数
}
