#pragma once

#include "scheduler.h"
#include "timer.h"

namespace gudov {

/**
 * @brief 基于 epoll 的 IO 管理器
 * @details 当
 *
 */
class IOManager : public Scheduler, public TimerManager {
 public:
  using ptr         = std::shared_ptr<IOManager>;
  using RWMutexType = RWMutex;

  enum Event {
    NONE  = 0x0,
    READ  = 0x1,  // EPOLLIN
    WRITE = 0x4,  // EPOLLOUT
  };

 private:
  /**
   * @brief 待处理的 fd 事件单元
   *
   */
  struct FdContext {
    using MutexType = Mutex;
    struct EventContext {
      Scheduler*            scheduler = nullptr;  // 待执行的 scheduler
      Fiber::ptr            fiber;                // 事件携程
      std::function<void()> callback;             // 事件的回调函数
    };

    EventContext& getContext(Event event);
    void          resetContext(EventContext& ctx);

    /**
     * @brief 触发对应事件
     * @details 将对应的执行体放入调度器进行调度
     *
     * @param event
     */
    void triggerEvent(Event event);

    EventContext read;
    EventContext write;
    int          fd;
    Event        events = NONE;
    MutexType    mutex;
  };

 public:
  IOManager(size_t threads = 1, bool useCaller = true, const std::string& name = "");
  ~IOManager();

  /**
   * @brief
   *
   * @param fd
   * @param event
   * @param callback
   * @return int 0 success, -1 error
   */
  int addEvent(int fd, Event event, std::function<void()> callback = nullptr);

  /**
   * @brief 删除 fd 对应事件
   * @attention 删除时不会触发事件
   *
   * @param fd
   * @param event
   * @return true
   * @return false
   */
  bool delEvent(int fd, Event event);

  /**
   * @brief 取消 fd 对应的所有事件
   * @attention 取消前会触发一次对应
   *
   * @param fd
   * @return true
   * @return false
   */
  bool cancelEvent(int fd, Event event);

  /**
   * @brief 取消 fd 对应的所有事件
   * @attention 取消前会全部触发一次
   *
   * @param fd
   * @return true
   * @return false
   */
  bool cancelAll(int fd);

  static IOManager* GetThis();

 protected:
  /**
   * @brief 提醒有事件待处理
   * @details 会触发一次 epoll_wait
   *
   */
  void Tickle() override;
  bool Stopping() override;
  void Idle() override;

  /**
   * @brief 添加定时器后做适当处理
   * @details 执行 tickle 后继而触发 epoll_wait
   *
   */
  void onTimerInsertedAtFront() override;

  void contextResize(size_t size);
  bool Stopping(uint64_t& timeout);

 private:
  int _epfd = 0;

  // 0 为读端  1 为写端
  int _tickleFds[2];

  // 当前未执行的 IO 事件数量
  std::atomic<size_t> _pendingEventCount{0};
  RWMutexType         mutex_;

  std::vector<FdContext*> _fdContexts;
};

}  // namespace gudov
