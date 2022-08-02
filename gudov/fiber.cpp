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

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

static std::atomic<uint64_t> s_fiberId{0};
static std::atomic<uint64_t> s_fiberCount{0};

static thread_local Fiber* t_fiber = nullptr;
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiberStackSize = Config::Lookup<uint32_t>(
    "fiber.stack_size", 1024 * 1024, "fiber stack size");

class MallocStackAllocator {
 public:
  static void* Alloc(size_t size) { return malloc(size); }
  static void Dealloc(void* vp, size_t size) { return free(vp); }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId() {
  if (t_fiber) {
    return t_fiber->getId();
  }
  return 0;
}

Fiber::Fiber() {
  state_ = EXEC;
  SetThis(this);

  if (getcontext(&ctx_)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  ++s_fiberCount;

  GUDOV_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

Fiber::Fiber(std::function<void()> cb, size_t stackSize, bool useCaller)
    : id_(++s_fiberId), cb_(cb) {
  ++s_fiberCount;
  stackSize_ = stackSize ? stackSize : g_fiberStackSize->getValue();

  stack_ = StackAllocator::Alloc(stackSize_);
  if (getcontext(&ctx_)) {
    GUDOV_ASSERT2(false, "getcontext");
  }
  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_;
  ctx_.uc_stack.ss_size = stackSize_;

  if (!useCaller) {
    makecontext(&ctx_, &Fiber::MainFunc, 0);
  } else {
    makecontext(&ctx_, &Fiber::CallerMainFunc, 0);
  }

  GUDOV_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << id_;
}

Fiber::~Fiber() {
  --s_fiberCount;
  if (stack_) {
    GUDOV_ASSERT(state_ == TERM || state_ == EXCEPT || state_ == INIT);
    StackAllocator::Dealloc(stack_, stackSize_);
  } else {
    GUDOV_ASSERT(!cb_);
    GUDOV_ASSERT(state_ == EXEC);

    Fiber* cur = t_fiber;
    if (cur == this) {
      SetThis(nullptr);
    }
  }
  GUDOV_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << id_;
}

void Fiber::reset(std::function<void()> cb) {
  GUDOV_ASSERT(stack_);
  GUDOV_ASSERT(state_ == TERM || state_ == EXCEPT || state_ == INIT);
  cb_ = cb;
  if (getcontext(&ctx_)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  ctx_.uc_link = nullptr;
  ctx_.uc_stack.ss_sp = stack_;
  ctx_.uc_stack.ss_size = stackSize_;

  makecontext(&ctx_, &Fiber::MainFunc, 0);
  state_ = INIT;
}

void Fiber::call() {
  SetThis(this);
  state_ = EXEC;
  GUDOV_LOG_ERROR(g_logger) << getId();
  if (swapcontext(&t_threadFiber->ctx_, &ctx_)) {
    GUDOV_ASSERT2(false, "swapcontext");
  }
}

void Fiber::back() {
  SetThis(t_threadFiber.get());
  if (swapcontext(&ctx_, &t_threadFiber->ctx_)) {
    GUDOV_ASSERT2(false, "swapcontext");
  }
}

void Fiber::swapIn() {
  SetThis(this);
  GUDOV_ASSERT(state_ != EXEC);
  state_ = EXEC;
  if (swapcontext(&Scheduler::GetMainFiber()->ctx_, &ctx_)) {
    GUDOV_ASSERT2(false, "swapcontext");
  }
}

void Fiber::swapOut() {
  SetThis(Scheduler::GetMainFiber());

  if (this != Scheduler::GetMainFiber()) {
    if (swapcontext(&ctx_, &Scheduler::GetMainFiber()->ctx_)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  } else {
    if (swapcontext(&ctx_, &t_threadFiber->ctx_)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  }
}

void Fiber::SetThis(Fiber* f) { t_fiber = f; }

Fiber::ptr Fiber::GetThis() {
  if (t_fiber) {
    return t_fiber->shared_from_this();
  }
  Fiber::ptr mainFiber(new Fiber);
  GUDOV_ASSERT(t_fiber == mainFiber.get());
  t_threadFiber = mainFiber;
  return t_fiber->shared_from_this();
}

void Fiber::YieldToReady() {
  Fiber::ptr cur = GetThis();
  cur->state_ = READY;
  cur->swapOut();
}

void Fiber::YieldToHold() {
  Fiber::ptr cur = GetThis();
  cur->state_ = HOLD;
  cur->swapOut();
}

uint64_t Fiber::TotalFibers() { return s_fiberCount; }

void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis();
  GUDOV_ASSERT(cur);
  try {
    cur->cb_();
    cur->cb_ = nullptr;
    cur->state_ = TERM;
  } catch (std::exception& ex) {
    cur->state_ = EXCEPT;
    GUDOV_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                              << " fiber_id=" << cur->getId() << std::endl
                              << gudov::BacktraceToString();
  } catch (...) {
    cur->state_ = EXCEPT;
    GUDOV_LOG_ERROR(g_logger) << "Fiber Except: "
                              << " fiber_id=" << cur->getId() << std::endl
                              << gudov::BacktraceToString();
  }

  auto rawPtr = cur.get();
  cur.reset();
  rawPtr->swapOut();

  GUDOV_ASSERT2(false,
                "never reach fiber_id=" + std::to_string(rawPtr->getId()));
}

void Fiber::CallerMainFunc() {
  Fiber::ptr cur = GetThis();
  GUDOV_ASSERT(cur);
  try {
    cur->cb_();
    cur->cb_ = nullptr;
    cur->state_ = TERM;
  } catch (std::exception& ex) {
    cur->state_ = EXCEPT;
    GUDOV_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                              << " fiber_id=" << cur->getId() << std::endl
                              << gudov::BacktraceToString();
  } catch (...) {
    cur->state_ = EXCEPT;
    GUDOV_LOG_ERROR(g_logger) << "Fiber Except: "
                              << " fiber_id=" << cur->getId() << std::endl
                              << gudov::BacktraceToString();
  }

  auto rawPtr = cur.get();
  cur.reset();
  rawPtr->back();
  GUDOV_ASSERT2(false,
                "never reach fiber_id=" + std::to_string(rawPtr->getId()));
}

};  // namespace gudov
