#ifndef __GUDOV_SCHEDULER_H__
#define __GUDOV_SCHEDULER_H__

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "fiber.h"
#include "thread.h"

namespace gudov {

class Scheduler {
 public:
  using ptr = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;

  Scheduler(size_t threads = 1, bool useCaller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const { return name_; }

  static Scheduler* GetThis();
  static Fiber* GetMainFiber();

  void start();
  void stop();

  template <typename FiberOrCb>
  void schedule(FiberOrCb fc, int thread = -1) {
    bool needTickle = false;
    {
      MutexType::Lock lock(mutex_);
      needTickle = scheduleNoLock(fc, thread);
    }

    if (needTickle) {
      tickle();
    }
  }

  template <typename InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool needTickle = false;
    {
      MutexType::Lock lock(mutex_);
      while (begin != end) {
        needTickle = scheduleNoLock(&*begin) || needTickle;
      }
    }

    if (needTickle) {
      tickle();
    }
  }

 protected:
  virtual void tickle();
  void run();
  virtual bool stopping();
  virtual void idle();

  void setThis();

 private:
  template <typename FiberOrCb>
  bool scheduleNoLock(FiberOrCb fc, int thread) {
    bool needTickle = fibers_.empty();
    FiberAndThread ft(fc, thread);
    if (ft.fiber || ft.cb) {
      fibers_.push_back(ft);
    }

    return needTickle;
  }

 private:
  struct FiberAndThread {
    Fiber::ptr fiber;
    std::function<void()> cb;
    int thread;

    FiberAndThread(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}
    FiberAndThread(Fiber::ptr* f, int thr) : thread(thr) { fiber.swap(*f); }
    FiberAndThread(std::function<void()> f, int thr) : cb(f), thread(thr) {}
    FiberAndThread(std::function<void()>* f, int thr) : thread(thr) {
      cb.swap(*f);
    }

    FiberAndThread() : thread(-1) {}

    void reset() {
      fiber = nullptr;
      cb = nullptr;
      thread = -1;
    }
  };

 private:
  MutexType mutex_;
  std::vector<Thread::ptr> threads_;
  std::list<FiberAndThread> fibers_;
  Fiber::ptr rootFiber_;
  std::string name_;

 protected:
  std::vector<int> threadIds_;
  size_t threadCount_;
  std::atomic<size_t> activeThreadCount_ = {0};
  std::atomic<size_t> idleThreadCount_ = {0};
  bool stopping_ = true;
  bool autoStop_ = false;
  int rootThread_ = 0;
};

}  // namespace gudov

#endif  // __GUDOV_SCHEDULER_H__
