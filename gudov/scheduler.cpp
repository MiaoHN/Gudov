#include "scheduler.h"

#include <string>

#include "gudov/util.h"
#include "hook.h"
#include "log.h"
#include "macro.h"

namespace gudov {

static gudov::Logger::ptr g_logger = GUDOV_LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;

/**
 * @brief 程序主协程的指针
 * @details 运行 Scheduler::run 函数所在的 Fiber
 *
 */
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string& name)
    : _name(name) {
  GUDOV_ASSERT(threads > 0);

  if (useCaller) {
    // 初始化主协程
    gudov::Fiber::GetThis();
    GUDOV_LOG_DEBUG(g_logger) << "Scheduler::Scheduler Main Fiber Create";
    --threads;  // 减掉主协程

    // 只能有一个 Scheduler
    GUDOV_ASSERT(GetThis() == nullptr);
    t_scheduler = this;

    _rootFiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0));
    GUDOV_LOG_DEBUG(g_logger) << "Scheduler::Scheduler Root Fiber Create";
    gudov::Thread::SetName(_name);

    t_scheduler_fiber = _rootFiber.get();
    _rootThread       = gudov::GetThreadId();
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

Fiber* Scheduler::GetMainFiber() { return t_scheduler_fiber; }

void Scheduler::start() {
  MutexType::Lock lock(_mutex);
  if (!_stopping) {
    return;
  }
  _stopping = false;
  GUDOV_ASSERT(_threads.empty());

  _threads.resize(_threadCount);
  for (size_t i = 0; i < _threadCount; ++i) {
    // 创建指定数量的线程并执行 run
    _threads[i].reset(new Thread(std::bind(&Scheduler::run, this),
                                 _name + "_" + std::to_string(i)));
    _threadIds.push_back(_threads[i]->getId());
  }
  lock.unlock();
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
      // 主协程执行入口
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
  setHookEnable(true);
  setThis();
  // 获得主协程
  if (GetThreadId() != _rootThread) {
    t_scheduler_fiber = Fiber::GetThis().get();
  }

  // 新建 idle 协程
  Fiber::ptr idleFiber(new Fiber(std::bind(&Scheduler::idle, this)));
  GUDOV_LOG_DEBUG(g_logger) << "Scheduler::run idle Fiber Create";
  Fiber::ptr cbFiber;

  FiberAndThread ft;
  while (true) {
    ft.reset();
    bool tickleMe = false;
    bool isActive = false;

    // ~ 拿到一个未调度的 FiberAndThread
    {
      MutexType::Lock lock(_mutex);

      // 遍历执行队列中的所有执行体
      auto it = _fibers.begin();
      while (it != _fibers.end()) {
        if (it->thread != -1 && it->thread != GetThreadId()) {
          // 指定了处理线程，但不是当前线程则跳过
          ++it;
          tickleMe = true;
          continue;
        }

        GUDOV_ASSERT(it->fiber || it->cb);
        if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
          // 正在运行中的协程
          ++it;
          continue;
        }

        // 找到未调度任务，将其从队列中取出
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
      // 如果是可运行的协程，则将该协程执行完
      ft.fiber->swapIn();
      --_activeThreadCount;

      if (ft.fiber->getState() == Fiber::READY) {
        // 如果状态为 READY 则重新调度
        schedule(ft.fiber);
      } else if (ft.fiber->getState() != Fiber::TERM &&
                 ft.fiber->getState() != Fiber::EXCEPT) {
        // 未进行调度，设为挂起状态
        ft.fiber->_state = Fiber::HOLD;
      }
      ft.reset();
    } else if (ft.cb) {
      // 如果是函数 callback，则执行该函数
      // 初始化 cbFiber
      if (cbFiber) {
        cbFiber->reset(ft.cb);
      } else {
        cbFiber.reset(new Fiber(ft.cb));
        GUDOV_LOG_DEBUG(g_logger) << "Scheduler::run cbFiber Create";
      }
      ft.reset();
      // 执行 cbFiber
      cbFiber->swapIn();
      --_activeThreadCount;
      if (cbFiber->getState() == Fiber::READY) {
        // 状态为 READY 时重新调度
        schedule(cbFiber);
        cbFiber.reset();
      } else if (cbFiber->getState() == Fiber::EXCEPT ||
                 cbFiber->getState() == Fiber::TERM) {
        // 执行结束，释放资源
        cbFiber->reset(nullptr);
      } else {
        // 未进行调度转为 HOLD 状态
        // TODO 这里的执行过程有点混乱
        cbFiber->_state = Fiber::HOLD;
        cbFiber.reset();
      }
    } else {
      // 没有待调度的执行体
      if (isActive) {
        --_activeThreadCount;
        continue;
      }

      if (idleFiber->getState() == Fiber::TERM) {
        // idle 协程状态为 TERM 时退出该函数
        GUDOV_LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++_idleThreadCount;
      // 转入 idle 协程处理函数中
      GUDOV_LOG_DEBUG(g_logger) << "swap to idleFiber...";
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
