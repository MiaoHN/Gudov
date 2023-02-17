#include "timer.h"

#include "log.h"
#include "util.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

Timer::Timer(uint64_t ms, std::function<void()> callback, bool recurring,
             TimerManager *manager)
    : m_recurring(recurring), m_ms(ms), m_callback(callback), m_manager(manager) {
  m_next = GetCurrentMS() + m_ms;
}

Timer::Timer(uint64_t next) : m_next(next) {}

bool Timer::cancel() {
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (m_callback) {
    m_callback    = nullptr;
    auto it = m_manager->m_timers.find(shared_from_this());
    m_manager->m_timers.erase(it);
    return true;
  }
  return false;
}

bool Timer::refresh() {
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (!m_callback) {
    return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  if (it == m_manager->m_timers.end()) {
    return false;
  }
  m_manager->m_timers.erase(it);
  m_next = GetCurrentMS() + m_ms;
  m_manager->m_timers.insert(shared_from_this());
  return true;
}

bool Timer::reset(uint64_t ms, bool fromNow) {
  if (ms == m_ms && !fromNow) {
    return true;
  }
  TimerManager::RWMutexType::WriteLock lock(m_manager->m_mutex);
  if (!m_callback) {
    return false;
  }
  auto it = m_manager->m_timers.find(shared_from_this());
  if (it == m_manager->m_timers.end()) {
    return false;
  }
  m_manager->m_timers.erase(it);
  uint64_t start = 0;
  if (fromNow) {
    start = GetCurrentMS();
  } else {
    start = m_next - m_ms;
  }
  m_ms   = ms;
  m_next = start + m_ms;
  m_manager->addTimer(shared_from_this(), lock);
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
  if (lhs->m_next < rhs->m_next) {
    return true;
  }
  if (lhs->m_next > rhs->m_next) {
    return false;
  }
  return lhs.get() < rhs.get();
}

TimerManager::TimerManager() { m_previous_time = GetCurrentMS(); }

TimerManager::~TimerManager() {}

Timer::ptr TimerManager::addTimer(uint64_t ms, std::function<void()> callback,
                                  bool recurring) {
  LOG_DEBUG(g_logger) << "TimerManager::addTimer";
  Timer::ptr             timer(new Timer(ms, callback, recurring, this));
  RWMutexType::WriteLock lock(m_mutex);
  addTimer(timer, lock);
  return timer;
}

/**
 * @brief 条件的回调函数
 *
 * @param weakCond
 * @param callback
 */
static void OnTimer(std::weak_ptr<void> weakCond, std::function<void()> callback) {
  std::shared_ptr<void> tmp = weakCond.lock();
  if (tmp) {
    callback();
  }
}

Timer::ptr TimerManager::addConditionTimer(uint64_t              ms,
                                           std::function<void()> callback,
                                           std::weak_ptr<void>   weakCond,
                                           bool                  recurring) {
  return addTimer(ms, std::bind(&OnTimer, weakCond, callback), recurring);
}

uint64_t TimerManager::getNextTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  m_tickled = false;
  if (m_timers.empty()) {
    return ~0ull;
  }

  const Timer::ptr &next   = *m_timers.begin();
  uint64_t          now_ms = GetCurrentMS();
  if (now_ms >= next->m_next) {
    return 0;
  } else {
    return next->m_next - now_ms;
  }
}

void TimerManager::listExpiredCb(std::vector<std::function<void()>> &cbs) {
  uint64_t now_ms = GetCurrentMS();

  std::vector<Timer::ptr> expired;
  {
    RWMutexType::ReadLock lock(m_mutex);
    if (m_timers.empty()) {
      return;
    }
  }
  RWMutexType::WriteLock lock(m_mutex);

  if (m_timers.empty()) {
    return;
  }
  bool roll_over = detectClockRollover(now_ms);
  if (!roll_over && ((*m_timers.begin())->m_next > now_ms)) {
    return;
  }

  Timer::ptr now_timer(new Timer(now_ms));
  auto       it = roll_over ? m_timers.end() : m_timers.lower_bound(now_timer);
  while (it != m_timers.end() && (*it)->m_next == now_ms) {
    ++it;
  }
  expired.insert(expired.begin(), m_timers.begin(), it);
  m_timers.erase(m_timers.begin(), it);
  cbs.reserve(expired.size());

  for (auto &timer : expired) {
    cbs.push_back(timer->m_callback);
    if (timer->m_recurring) {
      timer->m_next = now_ms + timer->m_ms;
      m_timers.insert(timer);
    } else {
      timer->m_callback = nullptr;
    }
  }
}

void TimerManager::addTimer(Timer::ptr val, RWMutexType::WriteLock &lock) {
  auto it = m_timers.insert(val).first;

  // m_timers 原本为空并且未 tickle
  bool at_front = (it == m_timers.begin()) && !m_tickled;
  if (at_front) {
    m_tickled = true;
  }
  lock.unlock();

  if (at_front) {
    onTimerInsertedAtFront();
  }
}

bool TimerManager::detectClockRollover(uint64_t now_ms) {
  bool roll_over = false;
  if (now_ms < m_previous_time && now_ms < (m_previous_time - 60 * 60 * 1000)) {
    roll_over = true;
  }
  m_previous_time = now_ms;
  return roll_over;
}

bool TimerManager::hasTimer() {
  RWMutexType::ReadLock lock(m_mutex);
  return !m_timers.empty();
}

}  // namespace gudov
