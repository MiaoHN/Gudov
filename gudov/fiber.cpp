#include "fiber.h"

#include <ucontext.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <memory>
#include <ostream>
#include <string>

#include "config.h"
#include "gudov/util.h"
#include "log.h"
#include "macro.h"
#include "scheduler.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

/// @brief 程序内协程号
static std::atomic<uint64_t> s_fiber_id{0};

/// @brief 协程数量
static std::atomic<uint64_t> s_fiber_count{0};

/// @brief 当前线程运行的协程
static thread_local Fiber* t_running_fiber = nullptr;

/// @brief 当前线程的主协程
static thread_local Fiber::ptr t_thread_fiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiber_stack_size =
    Config::Lookup<uint32_t>("fiber.stack_size", 1024 * 1024, "fiber stack size");

/**
 * @brief 自定义栈内存分配器
 *
 */
class MallocStackAllocator {
 public:
  static void* Alloc(size_t size) { return malloc(size); }
  static void  Dealloc(void* vp, size_t size) { return free(vp); }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetRunningFiberId() {
  if (t_running_fiber) {
    return t_running_fiber->GetID();
  }
  return 0;
}

Fiber::Fiber() {
  state_ = Running;
  // 将当前运行的协程设为此协程
  SetRunningFiber(this);

  // 获取当前运行栈信息
  if (getcontext(&ctx_)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  ++s_fiber_count;

  LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

Fiber::Fiber(std::function<void()> callback, size_t stack_size, bool run_in_scheduler)
    : id_(++s_fiber_id), callback_(callback), run_in_scheduler_(run_in_scheduler) {
  ++s_fiber_count;

  // 如果未指定栈大小则从配置文件中读取
  stack_size_ = stack_size ? stack_size : g_fiber_stack_size->GetValue();

  // 分配一片栈空间
  stack_ = StackAllocator::Alloc(stack_size_);

  if (getcontext(&ctx_)) {
    GUDOV_ASSERT2(false, "getcontext");
  }
  // 当前 context 执行完的下一个 context，这里设为 nullptr
  ctx_.uc_link = nullptr;
  // 设置栈顶指针的位置
  ctx_.uc_stack.ss_sp = stack_;
  // 设置栈空间的大小
  ctx_.uc_stack.ss_size = stack_size_;

  // 为待执行函数指定栈空间
  makecontext(&ctx_, &Fiber::MainFunc, 0);

  LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << id_;
}

Fiber::~Fiber() {
  --s_fiber_count;
  if (stack_) {
    GUDOV_ASSERT(state_ == Term || state_ == Ready);
    StackAllocator::Dealloc(stack_, stack_size_);
  } else {
    GUDOV_ASSERT(!callback_);
    GUDOV_ASSERT(state_ == Running);

    Fiber* cur = t_running_fiber;
    if (cur == this) {
      SetRunningFiber(nullptr);
    }
  }
  LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << id_;
}

void Fiber::Reset(std::function<void()> callback) {
  GUDOV_ASSERT(stack_);
  GUDOV_ASSERT(state_ == Term || state_ == Ready);
  callback_ = callback;
  if (getcontext(&ctx_)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  ctx_.uc_link          = nullptr;
  ctx_.uc_stack.ss_sp   = stack_;
  ctx_.uc_stack.ss_size = stack_size_;

  makecontext(&ctx_, &Fiber::MainFunc, 0);
  state_ = Ready;
}

void Fiber::Resume() {
  GUDOV_ASSERT(state_ != Term && state_ != Running);
  SetRunningFiber(this);
  state_ = Running;

  if (run_in_scheduler_) {
    if (swapcontext(&(Scheduler::GetMainFiber()->ctx_), &ctx_)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  } else {
    if (swapcontext(&t_thread_fiber->ctx_, &ctx_)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  }
}

void Fiber::Yield() {
  GUDOV_ASSERT(state_ == Running || state_ == Term);
  SetRunningFiber(t_thread_fiber.get());
  if (state_ != Term) {
    state_ = Ready;
  }

  if (run_in_scheduler_) {
    if (swapcontext(&ctx_, &(Scheduler::GetMainFiber()->ctx_))) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  } else {
    if (swapcontext(&ctx_, &t_thread_fiber->ctx_)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  }
}

void Fiber::SetRunningFiber(Fiber* f) { t_running_fiber = f; }

Fiber::ptr Fiber::GetRunningFiber() {
  if (t_running_fiber) {
    // 当前有正在运行的携程
    return t_running_fiber->shared_from_this();
  }
  // 创建主协程
  Fiber::ptr mainFiber(new Fiber);
  GUDOV_ASSERT(t_running_fiber == mainFiber.get());
  t_thread_fiber = mainFiber;
  return t_running_fiber->shared_from_this();
}

uint64_t Fiber::TotalFibers() { return s_fiber_count; }

void Fiber::MainFunc() {
  // 获得当前运行的协程
  Fiber::ptr cur = GetRunningFiber();
  GUDOV_ASSERT(cur);

  try {
    cur->callback_();
    cur->callback_ = nullptr;
    cur->state_    = Term;
  } catch (std::exception& ex) {
    LOG_ERROR(g_logger) << "Fiber exception: " << ex.what();
    cur->state_ = Term;
  } catch (...) {
    LOG_ERROR(g_logger) << "Fiber unknown exception";
    cur->state_ = Term;
  }

  auto raw_ptr = cur.get();
  cur.reset();               // 销毁当前协程
  SetRunningFiber(nullptr);  // 防止 t_running_fiber 指向已销毁的协程
  raw_ptr->Yield();

  GUDOV_ASSERT2(false, "never reach fiber_id=" + std::to_string(raw_ptr->GetID()));
}

};  // namespace gudov
