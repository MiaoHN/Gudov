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

Scheduler::Scheduler(size_t threads, bool use_caller, const std::string& name) : name_(name), use_caller_(use_caller) {
  GUDOV_ASSERT(threads > 0);

  if (use_caller) {
    // 初始化主协程
    --threads;  // 减掉主协程
    gudov::Fiber::GetRunningFiber();
    LOG_DEBUG(g_logger) << "Scheduler::Scheduler Main Fiber Create";

    // 只能有一个 Scheduler
    GUDOV_ASSERT(GetScheduler() == nullptr);
    t_scheduler = this;

    root_fiber_.reset(new Fiber(std::bind(&Scheduler::Run, this), 0, false));
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
  if (stopping_) {
    LOG_ERROR(g_logger) << "Scheduler is stopped";
    return;
  }
  GUDOV_ASSERT(threads_.empty());

  threads_.resize(thread_count_);
  for (size_t i = 0; i < thread_count_; ++i) {
    // 创建指定数量的线程并执行 run
    threads_[i].reset(new Thread(std::bind(&Scheduler::Run, this), name_ + "_" + std::to_string(i)));
    thread_ids_.push_back(threads_[i]->GetID());
  }
}

void Scheduler::Stop() {
  LOG_DEBUG(g_logger) << "stop";

  if (Stopping()) {
    return;
  }
  stopping_ = true;

  if (use_caller_) {
    GUDOV_ASSERT(GetScheduler() == this);
  } else {
    GUDOV_ASSERT(GetScheduler() != this);
  }

  for (size_t i = 0; i < thread_count_; ++i) {
    Tickle();
  }

  if (root_fiber_) {
    Tickle();
  }

  if (root_fiber_) {
    root_fiber_->Resume();
    LOG_DEBUG(g_logger) << "root_fiber end";
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
  Fiber::ptr callback_fiber;

  Task task;
  while (true) {
    task.Reset();
    bool tickle_me = false;

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

        if (it->fiber && it->fiber->GetState() == Fiber::Running) {
          // 正在运行中的协程
          ++it;
          continue;
        }

        // 找到未调度任务，将其从队列中取出
        task = *it;
        tasks_.erase(it);
        ++active_thread_count_;
        break;
      }
      // 当前线程拿完一个任务后，发现队列还有剩余，需要唤醒其他线程
      tickle_me |= (it != tasks_.end());
    }

    if (tickle_me) {
      Tickle();
    }

    if (task.fiber) {
      task.fiber->Resume();
      --active_thread_count_;
      task.Reset();
    } else if (task.callback) {
      if (callback_fiber) {
        callback_fiber->Reset(task.callback);
      } else {
        callback_fiber.reset(new Fiber(task.callback));
      }
      task.Reset();
      callback_fiber->Resume();
      --active_thread_count_;
      callback_fiber.reset();
    } else {
      // 没有待调度的执行体
      if (idle_fiber->GetState() == Fiber::Term) {
        LOG_INFO(g_logger) << "idle fiber term";
        break;
      }
      ++idle_thread_count_;
      idle_fiber->Resume();
      --idle_thread_count_;
    }
  }

  LOG_DEBUG(g_logger) << "Scheduler::run end";
}

void Scheduler::Tickle() { LOG_INFO(g_logger) << "tickle"; }

bool Scheduler::Stopping() {
  MutexType::Locker lock(mutex_);
  return stopping_ && tasks_.empty() && active_thread_count_ == 0;
}

void Scheduler::Idle() {
  LOG_INFO(g_logger) << "idle";
  while (!Stopping()) {
    Fiber::GetRunningFiber()->Yield();
  }
}

}  // namespace gudov
