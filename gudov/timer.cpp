#include "timer.h"

#include "log.h"
#include "util.h"

namespace gudov {

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

Timer::Timer(uint64_t ms, std::function<void()> cb, bool recurring,
             TimerManager *manager)
    : _recurring(recurring), _ms(ms), _cb(cb), _manager(manager) {
  _next = GetCurrentMS() + _ms;
}

Timer::Timer(uint64_t next) : _next(next) {}

bool Timer::cancel() {
  TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
  if (_cb) {
    _cb     = nullptr;
    auto it = _manager->_timers.find(shared_from_this());
    _manager->_timers.erase(it);
    return true;
  }
  return false;
}

bool Timer::refresh() {
  TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
  if (!_cb) {
    return false;
  }
  auto it = _manager->_timers.find(shared_from_this());
  if (it == _manager->_timers.end()) {
    return false;
  }
  _manager->_timers.erase(it);
  _next = GetCurrentMS() + _ms;
  _manager->_timers.insert(shared_from_this());
  return true;
}

bool Timer::reset(uint64_t ms, bool fromNow) {
  if (ms == _ms && !fromNow) {
    return true;
  }
  TimerManager::RWMutexType::WriteLock lock(_manager->_mutex);
  if (!_cb) {
    return false;
  }
  auto it = _manager->_timers.find(shared_from_this());
  if (it == _manager->_timers.end()) {
    return false;
  }
  _manager->_timers.erase(it);
  uint64_t start = 0;
  if (fromNow) {
    start = GetCurrentMS();
  } else {
    start = _next - _ms;
  }
  _ms   = ms;
  _next = start + _ms;
  _manager->addTimer(shared_from_this(), lock);
  return true;
}

bool Timer::Comparator::operator()(const Timer::ptr &lhs,
                                   const Timer::ptr &rhs) const {
  if (!lhs && !rhs) {
    return false;
  }
  if (!lhs) {
    return true;
  }
  if (!rhs) {
    return false;
  }
  if (lhs->_next < rhs->_next) {
    return true;
  }
  if (lhs->_next > rhs->_next) {
    return false;
  }
  return lhs.get() < rhs.get();
}

TimerManager::TimerManager() { _previousTime = GetCurrentMS(); }

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> cb,
                                  bool recurring) {
  GUDOV_LOG_DEBUG(g_logger) << "TimerManager::addTimer";
  Timer::ptr             timer(new Timer(ms, cb, recurring, this));
  RWMutexType::WriteLock lock(_mutex);
  addTimer(timer, lock);
  return timer;
}

/**
 * @brief 条件的回调函数
 *
 * @param weakCond
 * @param cb
 */
static void OnTimer(std::weak_ptr<void> weakCond, std::function<void()> cb) {
  std::shared_ptr<void> tmp = weakCond.lock();
  if (tmp) {
    cb();
  }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t              ms,
                                           std::function<void()> cb,
                                           std::weak_ptr<void>   weakCond,
                                           bool                  recurring) {
  return addTimer(ms, std::bind(&OnTimer, weakCond, cb), recurring);
}

uint64_t TimerManager::getNextTimer() {
  RWMutexType::ReadLock lock(_mutex);
  _tickled = false;
  if (_timers.empty()) {
    return ~0ull;
  }

  const Timer::ptr &next  = *_timers.begin();
  uint64_t          nowMs = GetCurrentMS();
  if (nowMs >= next->_next) {
    return 0;
  } else {
    return next->_next - nowMs;
  }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
  uint64_t nowMs = GetCurrentMS();

  std::vector<Timer::ptr> expired;
  {
    RWMutexType::ReadLock lock(_mutex);
    if (_timers.empty()) {
      return;
    }
  }
  RWMutexType::WriteLock lock(_mutex);

  bool rollOver = detectClockRollover(nowMs);
  if (!rollOver && ((*_timers.begin())->_next > nowMs)) {
    return;
  }

  Timer::ptr nowTimer(new Timer(nowMs));
  auto       it = rollOver ? _timers.end() : _timers.lower_bound(nowTimer);
  while (it != _timers.end() && (*it)->_next == nowMs) {
    ++it;
  }
  expired.insert(expired.begin(), _timers.begin(), it);
  _timers.erase(_timers.begin(), it);
  cbs.reserve(expired.size());

  for (auto &timer : expired) {
    cbs.push_back(timer->_cb);
    if (timer->_recurring) {
      timer->_next = nowMs + timer->_ms;
      _timers.insert(timer);
    } else {
      timer->_cb = nullptr;
    }
  }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock &lock) {
  auto it = _timers.insert(val).first;

  // _timers 原本为空并且未 tickle
  bool atFront = (it == _timers.begin()) && !_tickled;
  if (atFront) {
    _tickled = true;
  }
  lock.unlock();

  if (atFront) {
    onTimerInsertedAtFront();
  }
}

bool TimerManager::detectClockRollover(uint64_t nowMs) {
  bool rollOver = false;
  if (nowMs < _previousTime && nowMs < (_previousTime - 60 * 60 * 1000)) {
    rollOver = true;
  }
  _previousTime = nowMs;
  return rollOver;
}

bool TimerManager::hasTimer() {
  RWMutexType::ReadLock lock(_mutex);
  return !_timers.empty();
}

}  // namespace gudov
