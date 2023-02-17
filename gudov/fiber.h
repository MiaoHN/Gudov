#ifndef __GUDOV_FIBER_H__
#define __GUDOV_FIBER_H__

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
    EXEC,   // 执行状态
    TERM,   // 结束状态
    READY,  // 准备状态
  };

 private:
  /**
   * @brief 创建主协程
   *
   */
  Fiber();

 public:
  Fiber(std::function<void()> callback, size_t stackSize = 0);
  ~Fiber();

  /**
   * @brief 重置此 fiber 的函数执行体
   *
   * @param callback
   */
  void reset(std::function<void()> callback);

  /**
   * @brief 转入当前协程执行
   *
   */
  void swapIn();

  /**
   * @brief 当前协程返回到主协程
   *
   */
  void swapOut();

  /**
   * @brief 执行此协程
   * @details 从主协程转到子协程
   *
   */
  void call();

  /**
   * @brief 返回主协程
   *
   */
  void back();

  uint64_t getId() const { return _id; }
  State    getState() const { return _state; }

 public:
  /**
   * @brief 设置当前运行协程
   *
   * @param f
   */
  static void SetThis(Fiber* f);

  /**
   * @brief 获得当前运行协程
   * @details 如果当前线程还未创建协程，则创建主协程并返回
   *
   * @return Fiber::ptr
   */
  static Fiber::ptr GetThis();

  /**
   * @brief 暂停执行当前协程，并将当前协程状态转为 Ready
   *
   */
  static void Yield();

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
  static uint64_t GetFiberId();

 private:
  uint64_t _id        = 0;
  uint32_t _stackSize = 0;
  State    _state     = READY;

  ucontext_t _ctx;
  void*      _stack = nullptr;

  std::function<void()> m_cb;
};

}  // namespace gudov

#endif  // __GUDOV_FIBER_H__
