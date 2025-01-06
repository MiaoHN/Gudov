#include <cassert>
#include <chrono>
#include <thread>

#include "gtest/gtest.h"
#include "gudov/gudov.h"

using namespace gudov;

// 测试 GetThreadId 是否正确获取线程 ID
TEST(UtilTest, GetThreadIdTest) {
  pid_t thread_id   = GetThreadId();
  pid_t syscall_tid = static_cast<pid_t>(syscall(SYS_gettid));

  // 验证 GetThreadId 的返回值是否与系统调用一致
  EXPECT_EQ(thread_id, syscall_tid);
}

// 测试 GetFiberId 是否总是返回 0 (假设没有实际协程支持时)
TEST(UtilTest, GetFiberIdTest) {
  uint32_t fiber_id = GetFiberId();

  // 假设未实现协程时，GetFiberId 返回 0
  EXPECT_EQ(fiber_id, 0);
}

// 测试 BackTrace 和 BacktraceToString 是否能正确返回调用栈信息
TEST(UtilTest, BackTraceTest) {
  std::vector<std::string> backtrace;
  BackTrace(backtrace);

  // 验证 BackTrace 的结果非空
  EXPECT_FALSE(backtrace.empty());

  // 打印调用栈以人工验证
  for (const auto& line : backtrace) {
    std::cout << line << std::endl;
  }

  std::string backtrace_str = BacktraceToString();
  EXPECT_FALSE(backtrace_str.empty());

  // 打印调用栈字符串以人工验证
  std::cout << backtrace_str << std::endl;
}

// 测试 GetCurrentMS 和 GetCurrentUS 的返回值是否合理
TEST(UtilTest, GetCurrentTimeTest) {
  uint64_t ms1 = GetCurrentMS();
  uint64_t us1 = GetCurrentUS();

  // 等待一段时间
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  uint64_t ms2 = GetCurrentMS();
  uint64_t us2 = GetCurrentUS();

  // 验证时间是单调递增的
  EXPECT_LT(ms1, ms2);
  EXPECT_LT(us1, us2);

  // 验证 ms 和 us 的时间差合理（大约是 10ms 的差距）
  EXPECT_NEAR(ms2 - ms1, 10, 1);           // 精度误差 1ms
  EXPECT_NEAR((us2 - us1) / 1000, 10, 1);  // 精度误差 1ms
}
