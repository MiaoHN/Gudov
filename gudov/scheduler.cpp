#include "scheduler.h"

#include <string>

#include "gudov/util.h"
#include "hook.h"
#include "log.h"
#include "macro.h"

namespace gudov {

static gudov::Logger::ptr g_logger = LOG_NAME("system");

static thread_local Scheduler* t_scheduler = nullptr;

/**
 * @brief 程序主协程的指针
 * @details 运行 Scheduler::run 函数所在的 Fiber
 *
 */
static thread_local Fiber* t_scheduler_fiber = nullptr;

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) : name_(name) {
  GUDOV_ASSERT(threads > 0);

  if (use_caller) {
    // 初始化主协程
    gudov::Fiber::GetRunningFiber();
    LOG_DEBUG(g_logger) << "Scheduler::Scheduler Main Fiber Create";
    --threads;  // 减掉主协程

    // 只能有一个 Scheduler
    GUDOV_ASSERT(GetScheduler() == nullptr);
    t_scheduler = this;

    root_fiber_.reset(new Fiber(std::bind(&Scheduler::Run, this), 0));
    LOG_DEBUG(g_logger) << "Scheduler::Scheduler Root Fiber Create";
    gudov::Thread::SetRunningThreadName(name_);

    t_scheduler_fiber = root_fiber_.get();
    root_thread_      = gudov::GetThreadId();
    thread_ids_.push_back(root_thread_);
  } else {
    root_thread_ = -1;
  }
  thread_count_ = threads;
}

Scheduler::~Scheduler() {
  GUDOV_ASSERT(stopping_);
  if (GetScheduler() == this) {
    t_scheduler = nullptr;
  }
}

Scheduler* Scheduler::GetScheduler() { return t_scheduler; }

Fiber* Scheduler::GetMainFiber() { return t_scheduler_fiber; }

void Scheduler::Start() {
  MutexType::Locker lock(mutex_);
  if (!stopping_) {
    return;
  }
  stopping_ = false;
  GUDOV_ASSERT(threads_.empty());

  threads_.resize(thread_count_);
  for (size_t i = 0; i < thread_count_; ++i) {
    // 创建指定数量的线程并执行 run
    threads_[i].reset(new Thread(std::bind(&Scheduler::Run, this), name_ + "_" + std::to_string(i)));
    thread_ids_.push_back(threads_[i]->GetID());
  }
  lock.Unlock();
}

void Scheduler::Stop() {
  auto_stop_ = true;
  if (root_fiber_ && thread_count_ == 0 &&
      (root_fiber_->GetState() == Fiber::TERM || root_fiber_->GetState() == Fiber::READY)) {
    LOG_INFO(g_logger) << this << " stopped";
    stopping_ = true;

    if (Stopping()) {
      return;
    }
  }

  if (root_thread_ != -1) {
    GUDOV_ASSERT(GetScheduler() == this);
  } else {
    GUDOV_ASSERT(GetScheduler() != this);
  }

  stopping_ = true;
  for (size_t i = 0; i < thread_count_; ++i) {
    Tickle();
  }

  if (root_fiber_) {
    Tickle();
  }

  if (root_fiber_) {
    if (!Stopping()) {
      // 主协程执行入口
      root_fiber_->Call();
    }
  }

  std::vector<Thread::ptr> threads;
  {
    MutexType::Locker lock(mutex_);
    threads.swap(threads_);
  }

  for (auto& i : threads) {
    i->Join();
  }
}

void Scheduler::SetThis() { t_scheduler = this; }

void Scheduler::Run() {
  SetHookEnable(true);
  SetThis();

  // 获得主协程
  if (GetThreadId() != root_thread_) {
    t_scheduler_fiber = Fiber::GetRunningFiber().get();
  }

  // 新建 idle 协程
  Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::Idle, this)));
  LOG_DEBUG(g_logger) << "Scheduler::run idle Fiber Create";
  Fiber::ptr callback_fiber;

  Task task;
  while (true) {
    task.Reset();
    bool tickle_me = false;
    bool is_active = false;

    // ~ 拿到一个未调度的 Task
    {
      MutexType::Locker lock(mutex_);

      // 遍历执行队列中的所有执行体
      auto it = tasks_.begin();
      while (it != tasks_.end()) {
        if (it->thread != -1 && it->thread != GetThreadId()) {
          // 指定了处理线程，但不是当前线程则跳过
          ++it;
          tickle_me = true;
          continue;
        }

        GUDOV_ASSERT(it->fiber || it->callback);
        if (it->fiber && it->fiber->GetState() == Fiber::EXEC) {
          // 正在运行中的协程
          ++it;
          continue;
        }

        // 找到未调度任务，将其从队列中取出
        task = *it;
        tasks_.erase(it);
        ++active_thread_count_;
        is_active = true;
        break;
      }
    }

    if (tickle_me) {
      Tickle();
    }

    if (task.fiber && (task.fiber->GetState() != Fiber::TERM)) {
      // 如果是可运行的协程，则将该协程执行完
      task.fiber->SwapIn();
      --active_thread_count_;

      if (task.fiber->GetState() == Fiber::READY) {
        // 如果状态为 READY 则重新调度
        Schedule(task.fiber);
      } else if (task.fiber->GetState() != Fiber::TERM) {
        // 未进行调度，设为挂起状态
        task.fiber->state_ = Fiber::READY;
      }
      task.Reset();
    } else if (task.callback) {
      // 如果是函数 callback，则执行该函数
      // 初始化 callback_fiber
      if (callback_fiber) {
        callback_fiber->Reset(task.callback);
      } else {
        callback_fiber.reset(new Fiber(task.callback));
        LOG_DEBUG(g_logger) << "Scheduler::run callback_fiber Create";
      }
      task.Reset();
      // 执行 callback_fiber
      callback_fiber->SwapIn();
      --active_thread_count_;
      if (callback_fiber->GetState() == Fiber::READY) {
        // 状态为 READY 时重新调度
        Schedule(callback_fiber);
        callback_fiber.reset();
      } else if (callback_fiber->GetState() == Fiber::TERM) {
        // 执行结束，释放资源
        callback_fiber->Reset(nullptr);
      } else {
        // 未进行调度转为 HOLD 状态
        // TODO 这里的执行过程有点混乱
        callback_fiber->state_ = Fiber::READY;
        callback_fiber.reset();
      }
    } else {
      // 没有待调度的执行体
      if (is_active) {
        --active_thread_count_;
        continue;
      }

      if (idle_fiber->GetState() == Fiber::TERM) {
        // idle 协程状态为 TERM 时退出该函数
        LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++idle_thread_count_;
      // 转入 idle 协程处理函数中
      LOG_DEBUG(g_logger) << "swap to idle_fiber...";
      idle_fiber->SwapIn();
      --idle_thread_count_;
      if (idle_fiber->GetState() != Fiber::TERM) {
        idle_fiber->state_ = Fiber::READY;
      }
    }
  }
}

void Scheduler::Tickle() { LOG_INFO(g_logger) << "tickle"; }

bool Scheduler::Stopping() {
  MutexType::Locker lock(mutex_);
  return auto_stop_ && stopping_ && tasks_.empty() && active_thread_count_ == 0;
}

void Scheduler::Idle() {
  LOG_INFO(g_logger) << "idle";
  while (!Stopping()) {
    Fiber::Yield();
  }
}

}  // namespace gudov
