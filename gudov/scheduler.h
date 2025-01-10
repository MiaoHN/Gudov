#pragma once

#include <functional>
#include <list>
#include <memory>
#include <string>
#include <vector>

#include "fiber.h"
#include "thread.h"

namespace gudov {

/**
 * @brief 多线程多协程任务调度器
 * @details 一个调度器管理多个线程，一个线程处理多个协程，在 IOManager
 * 中只需处理 Fiber
 *
 */
class Scheduler {
 public:
  using ptr       = std::shared_ptr<Scheduler>;
  using MutexType = Mutex;

  /**
   * @brief 创建一个线程协程调度器
   *
   * @param threads 创建的线程数
   * @param use_caller 是否使用当前线程
   * @param name 调度器名称
   * @param auto_stop 是否自动停止
   */
  Scheduler(size_t threads = 1, bool use_caller = true, const std::string& name = "");
  virtual ~Scheduler();

  const std::string& GetName() const { return name_; }

  /**
   * @brief 获取当前 Scheduler
   * @warning thread_local
   *
   * @return Scheduler*
   */
  static Scheduler* GetScheduler();

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
  void Start();

  void Stop();

  /**
   * @brief 将待调度的协程或执行体加入调度队列中
   *
   * @tparam FiberOrCb
   * @param fc
   * @param thread
   */
  template <typename FiberOrCb>
  void Schedule(FiberOrCb fc, int thread = -1) {
    bool need_tickle = false;
    {
      MutexType::Locker lock(mutex_);
      need_tickle = ScheduleNoLock(fc, thread);
    }

    if (need_tickle) {
      Tickle();
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
  void Schedule(InputIterator begin, InputIterator end) {
    bool need_tickle = false;
    {
      MutexType::Locker lock(mutex_);
      while (begin != end) {
        need_tickle = ScheduleNoLock(&*begin, -1) || need_tickle;
        ++begin;
      }
    }

    if (need_tickle) {
      Tickle();
    }
  }

 protected:
  /**
   * @brief 通知协程有未执行任务
   *
   */
  virtual void Tickle();

  /**
   * @brief 处理调度的函数
   *
   */
  void Run();

  virtual bool Stopping();

  /**
   * @brief 没有待调度执行体时执行该函数
   *
   */
  virtual void Idle();

  void SetThis();

  bool HasIdleThreads() { return idle_thread_count_ > 0; }

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
  bool ScheduleNoLock(FiberOrCb fc, int thread) {
    bool need_tickle = tasks_.empty();
    Task task(fc, thread);
    if (task.fiber || task.callback) {
      tasks_.push_back(task);
    }

    return need_tickle;
  }

 private:
  /**
   * @brief 待运行的协程或线程
   *
   */
  struct Task {
    Fiber::ptr            fiber;
    std::function<void()> callback;
    int                   thread;

    Task(Fiber::ptr f, int thr) : fiber(f), thread(thr) {}
    Task(Fiber::ptr* f, int thr) : thread(thr) { fiber.swap(*f); }
    Task(std::function<void()> f, int thr) : callback(f), thread(thr) {}
    Task(std::function<void()>* f, int thr) : thread(thr) { callback.swap(*f); }

    Task() : thread(-1) {}

    void Reset() {
      fiber    = nullptr;
      callback = nullptr;
      thread   = -1;
    }
  };

 private:
  MutexType mutex_;

  /**
   * @brief 线程池 (不包括主协程)
   *
   */
  std::vector<Thread::ptr> threads_;

  /**
   * @brief 待处理的协程(业务)
   *
   */
  std::list<Task> tasks_;

  /**
   * @brief 主协程
   *
   */
  Fiber::ptr root_fiber_;

  std::string name_;

  bool use_caller_;

 protected:
  // 所有线程的 id (包括主协程)
  std::vector<int> thread_ids_;
  // 待调度的线程数
  size_t              thread_count_ = 0;
  std::atomic<size_t> active_thread_count_ = {0};
  std::atomic<size_t> idle_thread_count_   = {0};
  bool                stopping_            = false;
  // 主线程 ID
  int root_thread_ = 0;
};

}  // namespace gudov
