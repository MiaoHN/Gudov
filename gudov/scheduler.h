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

  /**
   * @brief 创建一个线程协程调度器
   *
   * @param threads 创建的线程数
   * @param useCaller
   * @param name
   */
  Scheduler(size_t threads = 1, bool useCaller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const { return _name; }

  /**
   * @brief 获取当前 Scheduler
   *
   * @return Scheduler*
   */
  static Scheduler* GetThis();

  /**
   * @brief 获取主协程
   *
   * @return Fiber*
   */
  static Fiber* GetMainFiber();

  /**
   * @brief 开始执行
   *
   */
  void start();

  void stop();

  /**
   * @brief 将待调度的协程或执行体加入调度队列中
   *
   * @tparam FiberOrCb
   * @param fc
   * @param thread
   */
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

  /**
   * @brief 添加多个协程或执行体
   *
   * @tparam InputIterator
   * @param begin
   * @param end
   */
  template <typename InputIterator>
  void schedule(InputIterator begin, InputIterator end) {
    bool needTickle = false;
    {
      MutexType::Lock lock(_mutex);
      while (begin != end) {
        needTickle = scheduleNoLock(&*begin, -1) || needTickle;
        ++begin;
      }
    }

    if (needTickle) {
      tickle();
    }
  }

 protected:
  /**
   * @brief 通知协程有未执行任务
   *
   */
  virtual void tickle();

  /**
   * @brief 处理调度的函数
   *
   */
  void run();

  virtual bool stopping();

  /**
   * @brief 没有待调度执行体时执行该函数
   *
   */
  virtual void idle();

  void setThis();

  bool hasIdleThreads() { return _idleThreadCount > 0; }

 private:
  /**
   * @brief 将执行体加入队列中
   * @details 将执行体加入 fibers 队列，如果队列为空则返回 true 等待 tickle
   *
   * @tparam FiberOrCb
   * @param fc
   * @param thread
   * @return true 执行队列为空
   * @return false 执行队列非空
   */
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
  /**
   * @brief 待运行的协程或线程
   *
   */
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
  MutexType _mutex;
  // 线程池 (不包括主协程)
  std::vector<Thread::ptr>  _threads;
  std::list<FiberAndThread> _fibers;
  Fiber::ptr                _rootFiber;  // 主协程
  std::string               _name;

 protected:
  // 所有线程的 id (包括主协程)
  std::vector<int> _threadIds;
  // 待调度的线程数
  size_t              _threadCount;
  std::atomic<size_t> _activeThreadCount = {0};
  std::atomic<size_t> _idleThreadCount   = {0};
  bool                _stopping          = true;
  bool                _autoStop          = false;
  // 主线程 ID
  int _rootThread = 0;
};

}  // namespace gudov

#endif  // __GUDOV_SCHEDULER_H__
