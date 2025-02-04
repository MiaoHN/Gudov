#pragma once

#include <sys/ucontext.h>
#include <ucontext.h>

#include <cstdint>
#include <functional>
#include <memory>

#include "thread.h"

namespace gudov {

class Scheduler;

/**
 * @brief 协程
 * @details
 * 比线程更轻量的执行单元，协程切换在用户态中进行，其速度比线程的转换还快，
 * 但是在同一线程中协程只能顺序执行，和线程的调度有所区别
 *
 */
class Fiber : public std::enable_shared_from_this<Fiber> {
  friend class Scheduler;

 public:
  using ptr = std::shared_ptr<Fiber>;

  enum State {
    Running,  // 执行状态
    Term,     // 结束状态
    Ready,    // 准备状态
  };

 private:
  /**
   * @brief 创建主协程
   *
   */
  Fiber();

 public:
  Fiber(std::function<void()> callback, size_t stack_size = 0, bool run_in_scheduler = true);
  ~Fiber();

  /**
   * @brief 重置此 fiber 的函数执行体
   *
   * @param callback
   */
  void Reset(std::function<void()> callback);

  void Resume();

  void Yield();

  uint64_t GetID() const { return id_; }
  State    GetState() const { return state_; }

 public:
  /**
   * @brief 设置当前运行协程
   * @warning thread_local
   *
   * @param f
   */
  static void SetRunningFiber(Fiber* f);

  /**
   * @brief 获得当前运行协程
   * @details 如果当前线程还未创建协程，则创建主协程并返回
   * @warning thread_local
   *
   * @return Fiber::ptr
   */
  static Fiber::ptr GetRunningFiber();

  /**
   * @brief 获得协程总数量
   *
   * @return uint64_t
   */
  static uint64_t TotalFibers();

  /**
   * @brief 协程的运行函数
   *
   */
  static void MainFunc();

  /**
   * @brief 获得当前运行的协程 ID
   *
   * @return uint64_t
   */
  static uint64_t GetRunningFiberId();

 private:
  uint64_t id_         = 0;
  uint32_t stack_size_ = 0;
  State    state_      = Ready;

  ucontext_t ctx_;
  void*      stack_ = nullptr;

  std::function<void()> callback_;

  bool run_in_scheduler_;
};

}  // namespace gudov
