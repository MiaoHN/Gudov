#ifndef __GUDOV_TIMER_H__
#define __GUDOV_TIMER_H__

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
  Timer(uint64_t ms, std::function<void()> cb, bool recurring,
        TimerManager *manager);
  Timer(uint64_t next);

 private:
  bool                  _recurring = false;  // 是否是循环定时器
  uint64_t              _ms        = 0;      // 执行周期
  uint64_t              _next      = 0;      // 精确的执行函数
  std::function<void()> _cb;
  TimerManager *        _manager = nullptr;

 private:
  struct Comparator {
    bool operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const;
  };
};

class TimerManager {
  friend class Timer;

 public:
  using RWMutexType = RWMutex;

  TimerManager();
  virtual ~TimerManager();

  Timer::ptr addTimer(uint64_t ms, std::function<void()> cb,
                      bool recurring = false);
  Timer::ptr addConditionTimer(uint64_t ms, std::function<void()> cb,
                               std::weak_ptr<void> weakCond,
                               bool                recurring = false);

  uint64_t getNextTimer();
  void     listExpiredCb(std::vector<std::function<void()>> &cbs);
  bool     hasTimer();

 protected:
  virtual void onTimerInsertedAtFront() = 0;
  void         addTimer(Timer::ptr val, RWMutexType::WriteLock &lock);

 private:
  bool detectClockRollover(uint64_t nowMs);

 private:
  RWMutexType _mutex;

  std::set<Timer::ptr, Timer::Comparator> _timers;

  bool     _tickled      = false;
  uint64_t _previousTime = 0;
};

}  // namespace gudov

#endif  // __GUDOV_TIMER_H__