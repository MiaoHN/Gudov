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
  using ptr       = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;

  Scheduler(size_t threads = 1, bool useCaller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const { return _name; }

  static Scheduler* GetThis();
  static Fiber*     GetMainFiber();

  void start();
  void stop();

  template <typename FiberOrCb>
  void schedule(FiberOrCb fc, int thread = -1) {
    bool needTickle = false;
    {
      MutexType::Lock lock(_mutex);
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
      MutexType::Lock lock(_mutex);
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
  void         run();
  virtual bool stopping();
  virtual void idle();

  void setThis();

 private:
  template <typename FiberOrCb>
  bool scheduleNoLock(FiberOrCb fc, int thread) {
    bool           needTickle = _fibers.empty();
    FiberAndThread ft(fc, thread);
    if (ft.fiber || ft.cb) {
      _fibers.push_back(ft);
    }

    return needTickle;
  }

 private:
  struct FiberAndThread {
    Fiber::ptr            fiber;
    std::function<void()> cb;
    int                   thread;

    FiberAndThread(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}
    FiberAndThread(Fiber::ptr* f, int thr) : thread(thr) { fiber.swap(*f); }
    FiberAndThread(std::function<void()> f, int thr) : cb(f), thread(thr) {}
    FiberAndThread(std::function<void()>* f, int thr) : thread(thr) {
      cb.swap(*f);
    }

    FiberAndThread() : thread(-1) {}

    void reset() {
      fiber  = nullptr;
      cb     = nullptr;
      thread = -1;
    }
  };

 private:
  MutexType                 _mutex;
  std::vector<Thread::ptr>  _threads;
  std::list<FiberAndThread> _fibers;
  Fiber::ptr                _rootFiber;
  std::string               _name;

 protected:
  std::vector<int>    _threadIds;
  size_t              _threadCount;
  std::atomic<size_t> _activeThreadCount = {0};
  std::atomic<size_t> _idleThreadCount   = {0};
  bool                _stopping          = true;
  bool                _autoStop          = false;
  int                 _rootThread        = 0;
};

}  // namespace gudov

#endif  // __GUDOV_SCHEDULER_H__
