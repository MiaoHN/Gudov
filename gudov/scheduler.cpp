#include "scheduler.h"

#include <string>

#include "gudov/util.h"
#include "log.h"
#include "macro.h"

namespace gudov {

static gudov::Logger::ptr g_logger = GUDOV_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;

/**
 * @brief 程序主协程
 *
 */
static thread_local Fiber* t_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string& name)
    : _name(name) {
  GUDOV_ASSERT(threads > 0);

  if (useCaller) {
    gudov::Fiber::GetThis();
    --threads;

    GUDOV_ASSERT(GetThis() == nullptr);
    t_scheduler = this;

    _rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0));
    gudov::Thread::SetName(_name);

    t_fiber     = _rootFiber.get();
    _rootThread = gudov::GetThreadId();
    _threadIds.push_back(_rootThread);
  } else {
    _rootThread = -1;
  }
  _threadCount = threads;
}

Scheduler::~Scheduler() {
  GUDOV_ASSERT(_stopping);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

Scheduler* Scheduler::GetThis() { return t_scheduler; }

Fiber* Scheduler::GetMainFiber() { return t_fiber; }

void Scheduler::start() {
  MutexType::Lock lock(_mutex);
  if (!_stopping) {
    return;
  }
  _stopping = false;
  GUDOV_ASSERT(_threads.empty());

  _threads.resize(_threadCount);
  for (size_t i = 0; i < _threadCount; ++i) {
    _threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
                                 _name + "_" + std::to_string(i)));
    _threadIds.push_back(_threads[i]->getId());
  }
  lock.unlock();

  // if (_rootFiber) {
  //   _rootFiber->call();
  //   GUDOV_LOG_INFO(g_logger) << "call out " << _rootFiber->getState();
  // }
}

void Scheduler::stop() {
  _autoStop = true;
  if (_rootFiber && _threadCount == 0 &&
      (_rootFiber->getState() == Fiber::TERM ||
       _rootFiber->getState() == Fiber::INIT)) {
    GUDOV_LOG_INFO(g_logger) << this << " stopped";
    _stopping = true;

    if (stopping()) {
      return;
    }
  }

  if (_rootThread != -1) {
    GUDOV_ASSERT(GetThis() == this);
  } else {
    GUDOV_ASSERT(GetThis() != this);
  }

  _stopping = true;
  for (size_t i = 0; i < _threadCount; ++i) {
    tickle();
  }

  if (_rootFiber) {
    tickle();
  }

  if (_rootFiber) {
    if (!stopping()) {
      _rootFiber->call();
    }
  }

  std::vector<Thread::ptr> threads;
  {
    MutexType::Lock lock(_mutex);
    threads.swap(_threads);
  }

  for (auto& i : threads) {
    i->join();
  }
}

void Scheduler::setThis() { t_scheduler = this; }

void Scheduler::run() {
  setThis();
  if (gudov::GetThreadId() != _rootThread) {
    t_fiber = Fiber::GetThis().get();
  }

  Fiber::ptr idleFiber(new Fiber(std::bind(&Scheduler::idle, this)));
  Fiber::ptr cbFiber;

  FiberAndThread ft;
  while (true) {
    ft.reset();
    bool tickleMe = false;
    bool isActive = false;
    {
      MutexType::Lock lock(_mutex);
      auto            it = _fibers.begin();
      while (it != _fibers.end()) {
        if (it->thread != -1 && it->thread != gudov::GetThreadId()) {
          ++it;
          tickleMe = true;
          continue;
        }

        GUDOV_ASSERT(it->fiber || it->cb);
        if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
          ++it;
          continue;
        }

        ft = *it;
        _fibers.erase(it);
        ++_activeThreadCount;
        isActive = true;
        break;
      }
    }
    if (tickleMe) {
      tickle();
    }

    if (ft.fiber && (ft.fiber->getState() != Fiber::TERM &&
                     ft.fiber->getState() != Fiber::EXCEPT)) {
      ft.fiber->swapIn();
      --_activeThreadCount;

      if (ft.fiber->getState() == Fiber::READY) {
        schedule(ft.fiber);
      } else if (ft.fiber->getState() != Fiber::TERM &&
                 ft.fiber->getState() != Fiber::EXCEPT) {
        ft.fiber->_state = Fiber::HOLD;
      }
      ft.reset();
    } else if (ft.cb) {
      if (cbFiber) {
        cbFiber->reset(ft.cb);
      } else {
        cbFiber.reset(new Fiber(ft.cb));
      }
      ft.reset();
      cbFiber->swapIn();
      --_activeThreadCount;
      if (cbFiber->getState() == Fiber::READY) {
        schedule(cbFiber);
        cbFiber.reset();
      } else if (cbFiber->getState() == Fiber::EXCEPT ||
                 cbFiber->getState() == Fiber::TERM) {
        cbFiber->reset(nullptr);
      } else {
        cbFiber->_state = Fiber::HOLD;
        cbFiber.reset();
      }
    } else {
      if (isActive) {
        --_activeThreadCount;
        continue;
      }
      if (idleFiber->getState() == Fiber::TERM) {
        GUDOV_LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++_idleThreadCount;
      idleFiber->swapIn();
      --_idleThreadCount;
      if (idleFiber->getState() != Fiber::TERM &&
          idleFiber->getState() != Fiber::EXCEPT) {
        idleFiber->_state = Fiber::HOLD;
      }
    }
  }
}

void Scheduler::tickle() { GUDOV_LOG_INFO(g_logger) << "tickle"; }

bool Scheduler::stopping() {
  MutexType::Lock lock(_mutex);
  return _autoStop && _stopping && _fibers.empty() && _activeThreadCount == 0;
}

void Scheduler::idle() {
  GUDOV_LOG_INFO(g_logger) << "idle";
  while (!stopping()) {
    Fiber::YieldToHold();
  }
}

}  // namespace gudov
