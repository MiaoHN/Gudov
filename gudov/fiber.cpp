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

/**
 * @brief 程序内协程号
 *
 */
static std::atomic<uint64_t> s_fiberId{0};

/**
 * @brief 协程数量
 *
 */
static std::atomic<uint64_t> s_fiberCount{0};

/**
 * @brief 当前运行协程
 *
 */
static thread_local Fiber* t_fiber = nullptr;

/**
 * @brief 当前线程的主协程
 *
 */
static thread_local Fiber::ptr t_threadFiber = nullptr;

static ConfigVar<uint32_t>::ptr g_fiberStackSize = Config::Lookup<uint32_t>(
    "fiber.stack_size", 1024 * 1024, "fiber stack size");

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

uint64_t Fiber::GetFiberId() {
  if (t_fiber) {
    return t_fiber->getId();
  }
  return 0;
}

Fiber::Fiber() {
  _state = EXEC;
  // 将当前运行的协程设为此协程
  SetThis(this);

  // 获取当前运行栈信息
  if (getcontext(&_ctx)) {
    GUDOV_ASSERT2(false, "getcontext");
  }

  ++s_fiberCount;

  GUDOV_LOG_DEBUG(g_logger) << "Fiber::Fiber";
}

Fiber::Fiber(std::function<void()> cb, size_t stackSize)
    : _id(++s_fiberId), _cb(cb) {
  ++s_fiberCount;

  // 如果未指定栈大小则从配置文件中读取
  _stackSize = stackSize ? stackSize : g_fiberStackSize->getValue();

  // 分配一片栈空间
  _stack = StackAllocator::Alloc(_stackSize);

  if (getcontext(&_ctx)) {
    GUDOV_ASSERT2(false, "getcontext");
  }
  // 当前 context 执行完的下一个 context，这里设为 nullptr
  _ctx.uc_link = nullptr;
  // 设置栈顶指针的位置
  _ctx.uc_stack.ss_sp = _stack;
  // 设置栈空间的大小
  _ctx.uc_stack.ss_size = _stackSize;

  // 为待执行函数指定栈空间
  makecontext(&_ctx, &Fiber::MainFunc, 0);

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
    // 如果不是主协程，则转入主协程
    if (swapcontext(&_ctx, &Scheduler::GetMainFiber()->_ctx)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  } else {
    // 如果是主协程，则转入当前线程的主协程
    if (swapcontext(&_ctx, &t_threadFiber->_ctx)) {
      GUDOV_ASSERT2(false, "swapcontext");
    }
  }
}

void Fiber::SetThis(Fiber* f) { t_fiber = f; }

Fiber::ptr Fiber::GetThis() {
  if (t_fiber) {
    // 当前有正在运行的携程
    return t_fiber->shared_from_this();
  }
  // 创建主协程
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
  // 获得当前运行的协程
  Fiber::ptr cur = GetThis();
  GUDOV_ASSERT(cur);

  cur->_cb();
  cur->_cb    = nullptr;
  cur->_state = TERM;

  auto rawPtr = cur.get();
  cur.reset();
  rawPtr->swapOut();

  GUDOV_ASSERT2(false,
                "never reach fiber_id=" + std::to_string(rawPtr->getId()));
}

};  // namespace gudov
