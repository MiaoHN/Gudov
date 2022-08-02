#ifndef __GUDOV_FIBER_H__
#define __GUDOV_FIBER_H__

#include <sys/ucontext.h>
#include <ucontext.h>

#include <cstdint>
#include <functional>
#include <memory>

#include "thread.h"

namespace gudov {

class Scheduler;

class Fiber : public std::enable_shared_from_this<Fiber> {
  friend class Scheduler;

 public:
  using ptr = std::shared_ptr<Fiber>;

  enum State {
    INIT,
    HOLD,
    EXEC,
    TERM,
    READY,
    EXCEPT,
  };

 private:
  Fiber();

 public:
  Fiber(std::function<void()> cb, size_t stackSize = 0, bool useCaller = false);
  ~Fiber();

  void reset(std::function<void()> cb);
  void swapIn();
  void swapOut();

  void call();
  void back();

  uint64_t getId() const { return id_; }
  State getState() const { return state_; }

 public:
  static void SetThis(Fiber* f);
  static Fiber::ptr GetThis();
  static void YieldToReady();
  static void YieldToHold();

  static uint64_t TotalFibers();

  static void MainFunc();
  static void CallerMainFunc();
  static uint64_t GetFiberId();

 private:
  uint64_t id_ = 0;
  uint32_t stackSize_ = 0;
  State state_ = INIT;

  ucontext_t ctx_;
  void* stack_ = nullptr;

  std::function<void()> cb_;
};

}  // namespace gudov

#endif  // __GUDOV_FIBER_H__
