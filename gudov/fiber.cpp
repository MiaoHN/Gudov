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

static thread_local Fiber*     t_fiber       = nullptr;
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiberStackSize = Config::Lookup<uint32_t>(
    "fiber.stack_size", 1024 * 1024, "fiber stack size");

class MallocStackAllocator {
 public:
  static void* Alloc(size_t size) { return malloc(size); }
  static void  Dealloc(void* vp, size_t size) { return free(vp); }
};

using StackAllocator = MallocStackAllocator;

uint64_t Fiber::GetFiberId() {
  if (t_fiber) {
    return t_fiber->getId();
  }
  return 0;
}

Fiber::Fiber() {
  _state = EXEC;
  SetThis(this);

  if (getcontext(&_ctx)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  ++s_fiberCount;

  GUDOV_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

Fiber::Fiber(std::function<void()> cb, size_t stackSize, bool useCaller)
    : _id(++s_fiberId), _cb(cb) {
  ++s_fiberCount;
  _stackSize = stackSize ? stackSize : g_fiberStackSize->getValue();

  _stack = StackAllocator::Alloc(_stackSize);
  if (getcontext(&_ctx)) {
    GUDOV_ASSERT2(false, "getcontext");
  }
  _ctx.uc_link          = nullptr;
  _ctx.uc_stack.ss_sp   = _stack;
  _ctx.uc_stack.ss_size = _stackSize;

  if (!useCaller) {
    makecontext(&_ctx, &Fiber::MainFunc, 0);
  } else {
    makecontext(&_ctx, &Fiber::CallerMainFunc, 0);
  }

  GUDOV_LOG_DEBUG(g_logger) << "Fiber::Fiber id=" << _id;
}

Fiber::~Fiber() {
  --s_fiberCount;
  if (_stack) {
    GUDOV_ASSERT(_state == TERM || _state == EXCEPT || _state == INIT);
    StackAllocator::Dealloc(_stack, _stackSize);
  } else {
    GUDOV_ASSERT(!_cb);
    GUDOV_ASSERT(_state == EXEC);

    Fiber* cur = t_fiber;
    if (cur == this) {
      SetThis(nullptr);
    }
  }
  GUDOV_LOG_DEBUG(g_logger) << "Fiber::~Fiber id=" << _id;
}

void Fiber::reset(std::function<void()> cb) {
  GUDOV_ASSERT(_stack);
  GUDOV_ASSERT(_state == TERM || _state == EXCEPT || _state == INIT);
  _cb = cb;
  if (getcontext(&_ctx)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  _ctx.uc_link          = nullptr;
  _ctx.uc_stack.ss_sp   = _stack;
  _ctx.uc_stack.ss_size = _stackSize;

  makecontext(&_ctx, &Fiber::MainFunc, 0);
  _state = INIT;
}

void Fiber::call() {
  SetThis(this);
  _state = EXEC;
  GUDOV_LOG_ERROR(g_logger) << getId();
  if (swapcontext(&t_threadFiber->_ctx, &_ctx)) {
    GUDOV_ASSERT2(false, "swapcontext");
  }
}

void Fiber::back() {
  SetThis(t_threadFiber.get());
  if (swapcontext(&_ctx, &t_threadFiber->_ctx)) {
    GUDOV_ASSERT2(false, "swapcontext");
  }
}

void Fiber::swapIn() {
  SetThis(this);
  GUDOV_ASSERT(_state != EXEC);
  _state = EXEC;
  if (swapcontext(&Scheduler::GetMainFiber()->_ctx, &_ctx)) {
    GUDOV_ASSERT2(false, "swapcontext");
  }
}

void Fiber::swapOut() {
  SetThis(Scheduler::GetMainFiber());

  if (this != Scheduler::GetMainFiber()) {
    if (swapcontext(&_ctx, &Scheduler::GetMainFiber()->_ctx)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  } else {
    if (swapcontext(&_ctx, &t_threadFiber->_ctx)) {
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
  cur->_state    = READY;
  cur->swapOut();
}

void Fiber::YieldToHold() {
  Fiber::ptr cur = GetThis();
  cur->_state    = HOLD;
  cur->swapOut();
}

uint64_t Fiber::TotalFibers() { return s_fiberCount; }

void Fiber::MainFunc() {
  Fiber::ptr cur = GetThis();
  GUDOV_ASSERT(cur);
  try {
    cur->_cb();
    cur->_cb    = nullptr;
    cur->_state = TERM;
  } catch (std::exception& ex) {
    cur->_state = EXCEPT;
    GUDOV_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                              << " fiber_id=" << cur->getId() << std::endl
                              << gudov::BacktraceToString();
  } catch (...) {
    cur->_state = EXCEPT;
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
    cur->_cb();
    cur->_cb    = nullptr;
    cur->_state = TERM;
  } catch (std::exception& ex) {
    cur->_state = EXCEPT;
    GUDOV_LOG_ERROR(g_logger) << "Fiber Except: " << ex.what()
                              << " fiber_id=" << cur->getId() << std::endl
                              << gudov::BacktraceToString();
  } catch (...) {
    cur->_state = EXCEPT;
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
