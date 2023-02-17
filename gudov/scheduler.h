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
   * @param useCaller
   * @param name
   */
  Scheduler(size_t threads = 1, bool useCaller = true,
            const std::string& name = "");
  virtual ~Scheduler();

  const std::string& getName() const { return m_name; }

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
      MutexType::Lock lock(m_mutex);
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
      MutexType::Lock lock(m_mutex);
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

  bool hasIdleThreads() { return m_idle_thread_count > 0; }

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
    bool needTickle = m_tasks.empty();
    Task task(fc, thread);
    if (task.fiber || task.callback) {
      m_tasks.push_back(task);
    }

    return needTickle;
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

    void reset() {
      fiber    = nullptr;
      callback = nullptr;
      thread   = -1;
    }
  };

 private:
  MutexType m_mutex;

  /**
   * @brief 线程池 (不包括主协程)
   *
   */
  std::vector<Thread::ptr> m_threads;

  /**
   * @brief 待处理的协程(业务)
   *
   */
  std::list<Task> m_tasks;

  /**
   * @brief 主协程
   *
   */
  Fiber::ptr m_root_fiber;

  std::string m_name;

 protected:
  // 所有线程的 id (包括主协程)
  std::vector<int> m_thread_ids;
  // 待调度的线程数
  size_t              m_thread_count;
  std::atomic<size_t> m_active_thread_count = {0};
  std::atomic<size_t> m_idle_thread_count   = {0};
  bool                m_stopping            = true;
  bool                m_auto_stop           = false;
  // 主线程 ID
  int m_root_thread = 0;
};

}  // namespace gudov

#endif  // __GUDOV_SCHEDULER_H__
