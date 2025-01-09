#include "timer.h"

#include "log.h"
#include "util.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

Timer::Timer(uint64_t ms, std::function<void()> callback, bool recurring, TimerManager *manager)
    : recurring_(recurring), ms_(ms), callback_(callback), manager_(manager) {
  next_ = GetCurrentMS() + ms_;
}

Timer::Timer(uint64_t next) : next_(next) {}

bool Timer::Cancel() {
  TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
  if (callback_) {
    callback_ = nullptr;
    auto it   = manager_->timers_.find(shared_from_this());
    manager_->timers_.erase(it);
    return true;
  }
  return false;
}

bool Timer::Refresh() {
  TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
  if (!callback_) {
    return false;
  }
  auto it = manager_->timers_.find(shared_from_this());
  if (it == manager_->timers_.end()) {
    return false;
  }
  manager_->timers_.erase(it);
  next_ = GetCurrentMS() + ms_;
  manager_->timers_.insert(shared_from_this());
  return true;
}

bool Timer::Reset(uint64_t ms, bool fromNow) {
  if (ms == ms_ && !fromNow) {
    return true;
  }
  TimerManager::RWMutexType::WriteLock lock(manager_->mutex_);
  if (!callback_) {
    return false;
  }
  auto it = manager_->timers_.find(shared_from_this());
  if (it == manager_->timers_.end()) {
    return false;
  }
  manager_->timers_.erase(it);
  uint64_t start = 0;
  if (fromNow) {
    start = GetCurrentMS();
  } else {
    start = next_ - ms_;
  }
  ms_   = ms;
  next_ = start + ms_;
  manager_->AddTimer(shared_from_this(), lock);
  return true;
}

bool Timer::Comparator::operator()(const Timer::ptr &lhs, const Timer::ptr &rhs) const {
  if (!lhs && !rhs) {
    return false;
  }
  if (!lhs) {
    return true;
  }
  if (!rhs) {
    return false;
  }
  if (lhs->next_ < rhs->next_) {
    return true;
  }
  if (lhs->next_ > rhs->next_) {
    return false;
  }
  return lhs.get() < rhs.get();
}

TimerManager::TimerManager() { previous_time_ = GetCurrentMS(); }

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::AddTimer(uint64_t ms, std::function<void()> callback, bool recurring) {
  LOG_DEBUG(g_logger) << "TimerManager::AddTimer";
  Timer::ptr             timer(new Timer(ms, callback, recurring, this));
  RWMutexType::WriteLock lock(mutex_);
  AddTimer(timer, lock);
  return timer;
}

/**
 * @brief 条件的回调函数
 *
 * @param weakCond
 * @param callback
 */
static void OnTimer(std::weak_ptr<void> weak_cond, std::function<void()> callback) {
  std::shared_ptr<void> tmp = weak_cond.lock();
  if (tmp) {
    callback();
  }
}

Timer::ptr TimerManager::AddConditionTimer(uint64_t ms, std::function<void()> callback, std::weak_ptr<void> weak_cond,
                                           bool recurring) {
  return AddTimer(ms, std::bind(&OnTimer, weak_cond, callback), recurring);
}

uint64_t TimerManager::GetNextTimeout() {
  RWMutexType::ReadLock lock(mutex_);
  tickled_ = false;
  if (timers_.empty()) {
    return ~0ull;
  }

  const Timer::ptr &next   = *timers_.begin();
  uint64_t          now_ms = GetCurrentMS();
  if (now_ms >= next->next_) {
    return 0;
  } else {
    return next->next_ - now_ms;
  }
}

void TimerManager::ListExpiredCallbacks(std::vector<std::function<void()>> &callbacks) {
  uint64_t now_ms = GetCurrentMS();

  std::vector<Timer::ptr> expired;
  {
    RWMutexType::ReadLock lock(mutex_);
    if (timers_.empty()) {
      return;
    }
  }
  RWMutexType::WriteLock lock(mutex_);

  if (timers_.empty()) {
    return;
  }
  bool roll_over = DetectClockRollover(now_ms);
  if (!roll_over && ((*timers_.begin())->next_ > now_ms)) {
    return;
  }

  Timer::ptr now_timer(new Timer(now_ms));
  auto       it = roll_over ? timers_.end() : timers_.lower_bound(now_timer);
  while (it != timers_.end() && (*it)->next_ == now_ms) {
    ++it;
  }
  expired.insert(expired.begin(), timers_.begin(), it);
  timers_.erase(timers_.begin(), it);
  callbacks.reserve(expired.size());

  for (auto &timer : expired) {
    callbacks.push_back(timer->callback_);
    if (timer->recurring_) {
      timer->next_ = now_ms + timer->ms_;
      timers_.insert(timer);
    } else {
      timer->callback_ = nullptr;
    }
  }
}

void TimerManager::AddTimer(Timer::ptr val, RWMutexType::WriteLock &lock) {
  auto it = timers_.insert(val).first;

  // timers_ 原本为空并且未 tickle
  bool at_front = (it == timers_.begin()) && !tickled_;
  if (at_front) {
    tickled_ = true;
  }
  lock.Unlock();

  if (at_front) {
    OnTimerInsertedAtFront();
  }
}

bool TimerManager::DetectClockRollover(uint64_t now_ms) {
  bool roll_over = false;
  if (now_ms < previous_time_ && now_ms < (previous_time_ - 60 * 60 * 1000)) {
    roll_over = true;
  }
  previous_time_ = now_ms;
  return roll_over;
}

bool TimerManager::HasTimer() {
  RWMutexType::ReadLock lock(mutex_);
  return !timers_.empty();
}

}  // namespace gudov
