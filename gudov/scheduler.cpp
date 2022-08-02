#include "scheduler.h"

#include <string>

#include "gudov/util.h"
#include "log.h"
#include "macro.h"

namespace gudov {

static gudov::Logger::ptr g_logger = GUDOV_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;
static thread_local Fiber* t_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string& name)
    : name_(name) {
  GUDOV_ASSERT(threads > 0);

  if (useCaller) {
    gudov::Fiber::GetThis();
    --threads;

    GUDOV_ASSERT(GetThis() == nullptr);
    t_scheduler = this;

    rootFiber_.reset(new Fiber(std::bind(&Scheduler::run, this), 0, true));
    gudov::Thread::SetName(name_);

    t_fiber = rootFiber_.get();
    rootThread_ = gudov::GetThreadId();
    threadIds_.push_back(rootThread_);
  } else {
    rootThread_ = -1;
  }
  threadCount_ = threads;
}

Scheduler::~Scheduler() {
  GUDOV_ASSERT(stopping_);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

Scheduler* Scheduler::GetThis() { return t_scheduler; }

Fiber* Scheduler::GetMainFiber() { return t_fiber; }

void Scheduler::start() {
  MutexType::Lock lock(mutex_);
  if (!stopping_) {
    return;
  }
  stopping_ = false;
  GUDOV_ASSERT(threads_.empty());

  threads_.resize(threadCount_);
  for (size_t i = 0; i < threadCount_; ++i) {
    threads_[i].reset(new Thread(std::bind(&Scheduler::run, this),
                                 name_ + "_" + std::to_string(i)));
    threadIds_.push_back(threads_[i]->getId());
  }
  lock.unlock();

  if (rootFiber_) {
    rootFiber_->call();
    GUDOV_LOG_INFO(g_logger) << "call out " << rootFiber_->getState();
  }
}

void Scheduler::stop() {
  autoStop_ = true;
  if (rootFiber_ && threadCount_ == 0 &&
      (rootFiber_->getState() == Fiber::TERM ||
       rootFiber_->getState() == Fiber::INIT)) {
    GUDOV_LOG_INFO(g_logger) << this << " stopped";
    stopping_ = true;

    if (stopping()) {
      return;
    }
  }

  if (rootThread_ != -1) {
    GUDOV_ASSERT(GetThis() == this);
  } else {
    GUDOV_ASSERT(GetThis() != this);
  }

  stopping_ = true;
  for (size_t i = 0; i < threadCount_; ++i) {
    tickle();
  }

  if (rootFiber_) {
    tickle();
  }

  if (stopping()) {
    return;
  }
}

void Scheduler::setThis() { t_scheduler = this; }

void Scheduler::run() {
  setThis();
  if (gudov::GetThreadId() != rootThread_) {
    t_fiber = Fiber::GetThis().get();
  }

  Fiber::ptr idleFiber(new Fiber(std::bind(&Scheduler::idle, this)));
  Fiber::ptr cbFiber;

  FiberAndThread ft;
  while (true) {
    ft.reset();
    bool tickleMe = false;
    {
      MutexType::Lock lock(mutex_);
      auto it = fibers_.begin();
      while (it != fibers_.end()) {
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
        fibers_.erase(it);
        break;
      }
    }
    if (tickleMe) {
      tickle();
    }

    if (ft.fiber && (ft.fiber->getState() != Fiber::TERM &&
                     ft.fiber->getState() != Fiber::EXCEPT)) {
      ++activeThreadCount_;
      ft.fiber->swapIn();
      --activeThreadCount_;

      if (ft.fiber->getState() == Fiber::READY) {
        schedule(ft.fiber);
      } else if (ft.fiber->getState() != Fiber::TERM &&
                 ft.fiber->getState() != Fiber::EXCEPT) {
        ft.fiber->state_ = Fiber::HOLD;
      }
      ft.reset();
    } else if (ft.cb) {
      if (cbFiber) {
        cbFiber->reset(ft.cb);
      } else {
        cbFiber.reset(new Fiber(ft.cb));
      }
      ft.reset();
      ++activeThreadCount_;
      cbFiber->swapIn();
      --activeThreadCount_;
      if (cbFiber->getState() == Fiber::READY) {
        schedule(cbFiber);
        cbFiber.reset();
      } else if (cbFiber->getState() == Fiber::EXCEPT ||
                 cbFiber->getState() == Fiber::TERM) {
        cbFiber->reset(nullptr);
      } else {
        cbFiber->state_ = Fiber::HOLD;
        cbFiber.reset();
      }
    } else {
      if (idleFiber->getState() == Fiber::TERM) {
        GUDOV_LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++idleThreadCount_;
      idleFiber->swapIn();
      --idleThreadCount_;
      if (idleFiber->getState() != Fiber::TERM &&
          idleFiber->getState() != Fiber::EXCEPT) {
        idleFiber->state_ = Fiber::HOLD;
      }
    }
  }
}

void Scheduler::tickle() { GUDOV_LOG_INFO(g_logger) << "tickle"; }

bool Scheduler::stopping() {
  MutexType::Lock lock(mutex_);
  return autoStop_ && stopping_ && fibers_.empty() && activeThreadCount_ == 0;
}

void Scheduler::idle() { GUDOV_LOG_INFO(g_logger) << "idle"; }

}  // namespace gudov
