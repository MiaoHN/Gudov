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

Scheduler::Scheduler(size_t threads, bool useCaller, const std::string& name) : m_name(name) {
  GUDOV_ASSERT(threads > 0);

  if (useCaller) {
    // 初始化主协程
    gudov::Fiber::GetThis();
    LOG_DEBUG(g_logger) << "Scheduler::Scheduler Main Fiber Create";
    --threads;  // 减掉主协程

    // 只能有一个 Scheduler
    GUDOV_ASSERT(GetThis() == nullptr);
    t_scheduler = this;

    m_root_fiber.reset(new Fiber(std::bind(&Scheduler::run, this), 0));
    LOG_DEBUG(g_logger) << "Scheduler::Scheduler Root Fiber Create";
    gudov::Thread::SetName(m_name);

    t_scheduler_fiber = m_root_fiber.get();
    m_root_thread     = gudov::GetThreadId();
    m_thread_ids.push_back(m_root_thread);
  } else {
    m_root_thread = -1;
  }
  m_thread_count = threads;
}

Scheduler::~Scheduler() {
  GUDOV_ASSERT(m_stopping);
  if (GetThis() == this) {
    t_scheduler = nullptr;
  }
}

Scheduler* Scheduler::GetThis() { return t_scheduler; }

Fiber* Scheduler::GetMainFiber() { return t_scheduler_fiber; }

void Scheduler::start() {
  MutexType::Lock lock(m_mutex);
  if (!m_stopping) {
    return;
  }
  m_stopping = false;
  GUDOV_ASSERT(m_threads.empty());

  m_threads.resize(m_thread_count);
  for (size_t i = 0; i < m_thread_count; ++i) {
    // 创建指定数量的线程并执行 run
    m_threads[i].reset(new Thread(std::bind(&Scheduler::run, this), m_name + "_" + std::to_string(i)));
    m_thread_ids.push_back(m_threads[i]->getId());
  }
  lock.unlock();
}

void Scheduler::stop() {
  m_auto_stop = true;
  if (m_root_fiber && m_thread_count == 0 &&
      (m_root_fiber->getState() == Fiber::TERM || m_root_fiber->getState() == Fiber::READY)) {
    LOG_INFO(g_logger) << this << " stopped";
    m_stopping = true;

    if (stopping()) {
      return;
    }
  }

  if (m_root_thread != -1) {
    GUDOV_ASSERT(GetThis() == this);
  } else {
    GUDOV_ASSERT(GetThis() != this);
  }

  m_stopping = true;
  for (size_t i = 0; i < m_thread_count; ++i) {
    tickle();
  }

  if (m_root_fiber) {
    tickle();
  }

  if (m_root_fiber) {
    if (!stopping()) {
      // 主协程执行入口
      m_root_fiber->call();
    }
  }

  std::vector<Thread::ptr> threads;
  {
    MutexType::Lock lock(m_mutex);
    threads.swap(m_threads);
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
  if (GetThreadId() != m_root_thread) {
    t_scheduler_fiber = Fiber::GetThis().get();
  }

  // 新建 idle 协程
  Fiber::ptr idle_fiber(new Fiber(std::bind(&Scheduler::idle, this)));
  LOG_DEBUG(g_logger) << "Scheduler::run idle Fiber Create";
  Fiber::ptr callback_fiber;

  Task task;
  while (true) {
    task.reset();
    bool tickleMe = false;
    bool isActive = false;

    // ~ 拿到一个未调度的 Task
    {
      MutexType::Lock lock(m_mutex);

      // 遍历执行队列中的所有执行体
      auto it = m_tasks.begin();
      while (it != m_tasks.end()) {
        if (it->thread != -1 && it->thread != GetThreadId()) {
          // 指定了处理线程，但不是当前线程则跳过
          ++it;
          tickleMe = true;
          continue;
        }

        GUDOV_ASSERT(it->fiber || it->callback);
        if (it->fiber && it->fiber->getState() == Fiber::EXEC) {
          // 正在运行中的协程
          ++it;
          continue;
        }

        // 找到未调度任务，将其从队列中取出
        task = *it;
        m_tasks.erase(it);
        ++m_active_thread_count;
        isActive = true;
        break;
      }
    }

    if (tickleMe) {
      tickle();
    }

    if (task.fiber && (task.fiber->getState() != Fiber::TERM)) {
      // 如果是可运行的协程，则将该协程执行完
      task.fiber->swapIn();
      --m_active_thread_count;

      if (task.fiber->getState() == Fiber::READY) {
        // 如果状态为 READY 则重新调度
        schedule(task.fiber);
      } else if (task.fiber->getState() != Fiber::TERM) {
        // 未进行调度，设为挂起状态
        task.fiber->m_state = Fiber::READY;
      }
      task.reset();
    } else if (task.callback) {
      // 如果是函数 callback，则执行该函数
      // 初始化 callback_fiber
      if (callback_fiber) {
        callback_fiber->reset(task.callback);
      } else {
        callback_fiber.reset(new Fiber(task.callback));
        LOG_DEBUG(g_logger) << "Scheduler::run callback_fiber Create";
      }
      task.reset();
      // 执行 callback_fiber
      callback_fiber->swapIn();
      --m_active_thread_count;
      if (callback_fiber->getState() == Fiber::READY) {
        // 状态为 READY 时重新调度
        schedule(callback_fiber);
        callback_fiber.reset();
      } else if (callback_fiber->getState() == Fiber::TERM) {
        // 执行结束，释放资源
        callback_fiber->reset(nullptr);
      } else {
        // 未进行调度转为 HOLD 状态
        // TODO 这里的执行过程有点混乱
        callback_fiber->m_state = Fiber::READY;
        callback_fiber.reset();
      }
    } else {
      // 没有待调度的执行体
      if (isActive) {
        --m_active_thread_count;
        continue;
      }

      if (idle_fiber->getState() == Fiber::TERM) {
        // idle 协程状态为 TERM 时退出该函数
        LOG_INFO(g_logger) << "idle fiber term";
        break;
      }

      ++m_idle_thread_count;
      // 转入 idle 协程处理函数中
      LOG_DEBUG(g_logger) << "swap to idle_fiber...";
      idle_fiber->swapIn();
      --m_idle_thread_count;
      if (idle_fiber->getState() != Fiber::TERM) {
        idle_fiber->m_state = Fiber::READY;
      }
    }
  }
}

void Scheduler::tickle() { LOG_INFO(g_logger) << "tickle"; }

bool Scheduler::stopping() {
  MutexType::Lock lock(m_mutex);
  return m_auto_stop && m_stopping && m_tasks.empty() && m_active_thread_count == 0;
}

void Scheduler::idle() {
  LOG_INFO(g_logger) << "idle";
  while (!stopping()) {
    Fiber::Yield();
  }
}

}  // namespace gudov
