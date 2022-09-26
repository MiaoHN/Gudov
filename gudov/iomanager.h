#ifndef __GUDOV_IOMANAGER_H__
#define __GUDOV_IOMANAGER_H__

#include "scheduler.h"
#include "timer.h"

namespace gudov {

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
  struct FdContext {
    using MutexType = Mutex;
    struct EventContext {
      Scheduler*            scheduler = nullptr;  // 待执行的 scheduler
      Fiber::ptr            fiber;                // 事件携程
      std::function<void()> cb;                   // 事件的回调函数
    };

    EventContext& getContext(Event event);
    void          resetContext(EventContext& ctx);
    void          triggerEvent(Event event);

    EventContext read;
    EventContext write;
    int          fd;
    Event        events = NONE;
    MutexType    mutex;
  };

 public:
  IOManager(size_t threads = 1, bool useCaller = true,
            const std::string& name = "");
  ~IOManager();

  /**
   * @brief
   *
   * @param fd
   * @param event
   * @param cb
   * @return int 0 success, -1 error
   */
  int  addEvent(int fd, Event event, std::function<void()> cb = nullptr);
  bool delEvent(int fd, Event event);
  bool cancelEvent(int fd, Event event);

  bool cancelAll(int fd);

  static IOManager* GetThis();

 protected:
  void tickle() override;
  bool stopping() override;
  void idle() override;
  void onTimerInsertedAtFront() override;

  void contextResize(size_t size);
  bool stopping(uint64_t& timeout);

 private:
  int _epfd = 0;
  int _tickleFds[2];

  std::atomic<size_t>     _pendingEventCount{0};
  RWMutexType             _mutex;
  std::vector<FdContext*> _fdContexts;
};

}  // namespace gudov

#endif  // __GUDOV_IOMANAGER_H__