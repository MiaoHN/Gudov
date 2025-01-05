#pragma once

#include <memory>
#include <set>
#include <vector>

#include "thread.h"

namespace gudov {

class TimerManager;

class Timer : public std::enable_shared_from_this<Timer> {
  friend class TimerManager;

 public:
  using ptr = std::shared_ptr<Timer>;

  bool Cancel();
  bool Refresh();
  bool Reset(uint64_t ms, bool fromNow);

 private:
  Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager *manager);
  Timer(uint64_t next);

 private:
  bool     recurring_ = false;  // 是否是循环定时器
  uint64_t ms_        = 0;      // 执行周期
  uint64_t next_      = 0;      // 精确的执行时间

  std::function<void()> callback_;  // 待执行的回调函数
  TimerManager         *manager_ = nullptr;

 private:
  struct Comparator {
    bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
  };
};

/**
 * @brief 定时器管理器
 *
 */
class TimerManager {
  friend class Timer;

 public:
  using RWMutexType = RWMutex;

  TimerManager();
  virtual ~TimerManager();

  Timer::ptr AddTimer(uint64_t ms, std::function<void()> callback, bool recurring = false);
  Timer::ptr AddConditionTimer(uint64_t ms, std::function<void()> callback, std::weak_ptr<void> weakCond,
                               bool recurring = false);

  uint64_t GetNextTimeout();
  void     ListExpiredCallbacks(std::vector<std::function<void()>> &cbs);
  bool     HasTimer();

 protected:
  virtual void OnTimerInsertedAtFront() = 0;
  void         AddTimer(Timer::ptr val, RWMutexType::WriteLock &lock);

 private:
  /**
   * @brief 服务器时间是否调后
   *
   * @param now_ms
   * @return true
   * @return false
   */
  bool DetectClockRollover(uint64_t now_ms);

 protected:
  bool tickled_ = false;

 private:
  RWMutexType mutex_;

  std::set<Timer::ptr, Timer::Comparator> timers_;

  uint64_t previous_time_ = 0;
};

}  // namespace gudov
