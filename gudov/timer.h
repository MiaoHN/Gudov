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

  bool cancel();
  bool refresh();
  bool reset(uint64_t ms, bool fromNow);

 private:
  Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager *manager);
  Timer(uint64_t next);

 private:
  bool     m_recurring = false;  // 是否是循环定时器
  uint64_t m_ms        = 0;      // 执行周期
  uint64_t m_next      = 0;      // 精确的执行函数

  std::function<void()> m_callback;  // 待执行的回调函数
  TimerManager         *m_manager = nullptr;

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

  Timer::ptr addTimer(uint64_t ms, std::function<void()> callback, bool recurring = false);
  Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> callback, std::weak_ptr<void> weakCond,
                               bool recurring = false);

  uint64_t getNextTimer();
  void     listExpiredCb(std::vector<std::function<void()>> &cbs);
  bool     hasTimer();

 protected:
  virtual void onTimerInsertedAtFront() = 0;
  void         addTimer(Timer::ptr val, RWMutexType::WriteLock &lock);

 private:
  /**
   * @brief 服务器时间是否调后
   *
   * @param now_ms
   * @return true
   * @return false
   */
  bool detectClockRollover(uint64_t now_ms);

 private:
  RWMutexType mutex_;

  std::set<Timer::ptr, Timer::Comparator> m_timers;

  bool     m_tickled       = false;
  uint64_t m_previous_time = 0;
};

}  // namespace gudov
