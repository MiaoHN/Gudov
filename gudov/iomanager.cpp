#include "iomanager.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "log.h"
#include "macro.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

IOManager::FdContext::EventContext& IOManager::FdContext::GetContext(IOManager::Event event) {
  switch (event) {
    case IOManager::Event::READ:
      return read;
    case IOManager::Event::WRITE:
      return write;
    default:
      GUDOV_ASSERT2(false, "GetContext");
  }
  throw std::invalid_argument("GetContext invalid event");
}

void IOManager::FdContext::ReSetContext(IOManager::FdContext::EventContext& ctx) {
  ctx.scheduler = nullptr;
  ctx.fiber.reset();
  ctx.callback = nullptr;
}

void IOManager::FdContext::TriggerEvent(IOManager::Event event) {
  GUDOV_ASSERT(events & event);
  events = (Event)(events & ~event);

  EventContext& ctx = GetContext(event);

  if (ctx.callback) {
    ctx.scheduler->Schedule(&ctx.callback);
  } else {
    ctx.scheduler->Schedule(&ctx.fiber);
  }
  ReSetContext(ctx);
}

IOManager::IOManager(size_t threads, bool use_caller, const std::string& name) : Scheduler(threads, use_caller, name) {
  epfd_ = epoll_create(5000);
  GUDOV_ASSERT(epfd_ > 0);

  int rt = pipe(tickle_fds_);
  GUDOV_ASSERT(!rt);

  // 将管道读端放入 epoll 进行监听
  epoll_event event;
  memset(&event, 0, sizeof(epoll_event));
  event.events  = EPOLLIN | EPOLLET;
  event.data.fd = tickle_fds_[0];

  // 非阻塞方式，配合边缘触发
  rt = fcntl(tickle_fds_[0], F_SETFL, O_NONBLOCK);
  GUDOV_ASSERT(!rt);

  // 如果管道可读，epoll_wait() 就会返回
  rt = epoll_ctl(epfd_, EPOLL_CTL_ADD, tickle_fds_[0], &event);
  GUDOV_ASSERT(!rt);

  ContextResize(32);

  Start();
}

IOManager::~IOManager() {
  Stop();
  close(epfd_);
  close(tickle_fds_[0]);
  close(tickle_fds_[1]);

  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    if (fd_contexts_[i]) {
      delete fd_contexts_[i];
    }
  }
}

void IOManager::ContextResize(size_t size) {
  fd_contexts_.resize(size);

  for (size_t i = 0; i < fd_contexts_.size(); ++i) {
    if (!fd_contexts_[i]) {
      fd_contexts_[i]     = new FdContext;
      fd_contexts_[i]->fd = i;
    }
  }
}

int IOManager::AddEvent(int fd, Event event, std::function<void()> callback) {
  FdContext* fd_ctx = nullptr;
  // 得到对应 fd 下标的 context，如果越界就将 _fdContexts 扩容
  RWMutexType::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() > fd) {
    fd_ctx = fd_contexts_[fd];
    lock.Unlock();
  } else {
    lock.Unlock();
    RWMutexType::WriteLock lock2(mutex_);
    ContextResize(fd * 1.5);
    GUDOV_ASSERT((int)fd_contexts_.size() > fd);
    fd_ctx = fd_contexts_[fd];
  }

  FdContext::MutexType::Locker lock2(fd_ctx->mutex);
  if (fd_ctx->events & event) {
    // 已经添加过该事件
    LOG_ERROR(g_logger) << "addEvent assert fd=" << fd << " event=" << event << " fdCtx.event=" << fd_ctx->events;
    GUDOV_ASSERT(!(fd_ctx->events & event));
  }

  // 判断是修改还是添加
  int op = fd_ctx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

  epoll_event epevent;
  epevent.events   = EPOLLET | fd_ctx->events | event;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(epfd_, op, fd, &epevent);
  if (rt) {
    LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", " << op << "," << fd << "," << epevent.events << "):" << rt
                        << " (" << errno << ") (" << strerror(errno) << ")";
    return -1;
  }

  // 待执行 IO 事件数量加 1
  ++pending_event_cnt_;

  fd_ctx->events                     = (Event)(fd_ctx->events | event);
  FdContext::EventContext& event_ctx = fd_ctx->GetContext(event);

  // 确保该事件还未分配 scheduler, fiber 以及 callback 函数
  GUDOV_ASSERT(!event_ctx.scheduler && !event_ctx.fiber && !event_ctx.callback);

  // 为该事件分配调用资源
  event_ctx.scheduler = Scheduler::GetScheduler();
  if (callback) {
    // 如果指定了调度函数则执行该函数
    event_ctx.callback.swap(callback);
  } else {
    // 执行当前协程
    event_ctx.fiber = Fiber::GetRunningFiber();
    GUDOV_ASSERT2(event_ctx.fiber->GetState() == Fiber::Running, "state=" << event_ctx.fiber->GetState());
  }
  return 0;
}

bool IOManager::DelEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() <= fd) {
    // 该 fd 超出范围
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.Unlock();

  FdContext::MutexType::Locker lock2(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    // 该 fd 不包含此事件
    return false;
  }

  Event new_events = (Event)(fd_ctx->events & ~event);
  int   op         = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

  epoll_event epevent;
  epevent.events   = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(epfd_, op, fd, &epevent);
  if (rt) {
    LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", " << op << "," << fd << "," << epevent.events << "):" << rt
                        << " (" << errno << ") (" << strerror(errno) << ")";
    return false;
  }

  --pending_event_cnt_;

  // 重置该 fdContext
  fd_ctx->events                     = new_events;
  FdContext::EventContext& event_ctx = fd_ctx->GetContext(event);

  fd_ctx->ReSetContext(event_ctx);
  return true;
}

bool IOManager::CancelEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.Unlock();

  FdContext::MutexType::Locker lock2(fd_ctx->mutex);
  if (!(fd_ctx->events & event)) {
    return false;
  }

  Event       new_events = (Event)(fd_ctx->events & ~event);
  int         op         = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events   = EPOLLET | new_events;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(epfd_, op, fd, &epevent);
  if (rt) {
    LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", " << op << "," << fd << "," << epevent.events << "):" << rt
                        << " (" << errno << ") (" << strerror(errno) << ")";
    return false;
  }

  // 删除前触发一次
  fd_ctx->TriggerEvent(event);
  --pending_event_cnt_;
  return true;
}

bool IOManager::CancelAll(int fd) {
  RWMutexType::ReadLock lock(mutex_);
  if ((int)fd_contexts_.size() <= fd) {
    return false;
  }
  FdContext* fd_ctx = fd_contexts_[fd];
  lock.Unlock();

  FdContext::MutexType::Locker lock2(fd_ctx->mutex);
  if (!fd_ctx->events) {
    return false;
  }

  int         op = EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events   = 0;
  epevent.data.ptr = fd_ctx;

  int rt = epoll_ctl(epfd_, op, fd, &epevent);
  if (rt) {
    LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", " << op << "," << fd << "," << epevent.events << "):" << rt
                        << " (" << errno << ") (" << strerror(errno) << ")";
    return false;
  }

  if (fd_ctx->events & READ) {
    fd_ctx->TriggerEvent(READ);
    --pending_event_cnt_;
  }
  if (fd_ctx->events & WRITE) {
    fd_ctx->TriggerEvent(WRITE);
    --pending_event_cnt_;
  }

  GUDOV_ASSERT(fd_ctx->events == 0);
  return true;
}

IOManager* IOManager::GetThis() { return dynamic_cast<IOManager*>(Scheduler::GetScheduler()); }

/**
 * 通知调度协程、也就是Scheduler::run()从idle中退出
 * Scheduler::run()每次从idle协程中退出之后，都会重新把任务队列里的所有任务执行完了再重新进入idle
 * 如果没有调度线程处理于idle状态，那也就没必要发通知了
 */
void IOManager::Tickle() {
  if (HasIdleThreads()) {
    return;
  }
  int rt = write(tickle_fds_[1], "T", 1);
  LOG_DEBUG(g_logger) << "write data to tickleFds[1]";
  GUDOV_ASSERT(rt == 1);
}

bool IOManager::Stopping(uint64_t& timeout) {
  // 对于IOManager而言，必须等所有待调度的IO事件都执行完了才可以退出
  // 增加定时器功能后，还应该保证没有剩余的定时器待触发
  timeout = GetNextTimeout();
  return timeout == ~0ull && pending_event_cnt_ == 0 && Scheduler::Stopping();
}

bool IOManager::Stopping() {
  uint64_t timeout = 0;
  return Stopping(timeout);
}

/**
 * 调度器无调度任务时会阻塞idle协程上，对IO调度器而言，idle状态应该关注两件事，一是有没有新的调度任务，对应Schduler::schedule()，
 * 如果有新的调度任务，那应该立即退出idle状态，并执行对应的任务；二是关注当前注册的所有IO事件有没有触发，如果有触发，那么应该执行
 * IO事件对应的回调函数
 */
void IOManager::Idle() {
  LOG_DEBUG(g_logger) << "idle";

  // 一次epoll_wait最多检测256个就绪事件，如果就绪事件超过了这个数，那么会在下轮epoll_wati继续处理
  static const uint64_t MAX_EVENTS = 64;

  epoll_event* events = new epoll_event[MAX_EVENTS]();

  // 函数结束时删除 event
  std::shared_ptr<epoll_event> shared_events(events, [](epoll_event* ptr) { delete[] ptr; });

  while (true) {
    // 获取下一个定时器的超时时间，顺便判断调度器是否停止
    uint64_t next_timeout = 0;
    if (GUDOV_UNLICKLY(Stopping(next_timeout))) {
      LOG_INFO(g_logger) << "name=" << GetName() << " idle stopping exit";
      break;
    }

    // 阻塞在epoll_wait上，等待事件发生或定时器超时
    int rt = 0;
    do {
      static const int MAX_TIMEOUT = 3000;
      if (next_timeout != ~0ull) {
        next_timeout = std::min((int)next_timeout, MAX_TIMEOUT);
      } else {
        next_timeout = MAX_TIMEOUT;
      }
      // 等待事件发生，即 tickle() 函数往管道写端写入数据或者
      rt = epoll_wait(epfd_, events, MAX_EVENTS, (int)next_timeout);
      if (rt < 0 && errno == EINTR) {
        continue;
      } else {
        break;
      }
    } while (true);

    // 收集所有已超时的定时器，执行回调函数
    std::vector<std::function<void()>> expired_callbacks;
    ListExpiredCallbacks(expired_callbacks);
    if (!expired_callbacks.empty()) {
      // 将超时事件加入调度队列
      Schedule(expired_callbacks.begin(), expired_callbacks.end());
      expired_callbacks.clear();
    }

    // 遍历所有发生的事件，根据epoll_event的私有指针找到对应的FdContext，进行事件处理
    for (int i = 0; i < rt; ++i) {
      epoll_event& event = events[i];
      if (event.data.fd == tickle_fds_[0]) {
        uint8_t dummy[256];
        while (read(tickle_fds_[0], dummy, sizeof(dummy)) > 0)
          ;
        continue;
      }

      // 获得事件对应句柄内容(包括执行体)
      FdContext* fd_ctx = (FdContext*)event.data.ptr;

      FdContext::MutexType::Locker lock(fd_ctx->mutex);

      /**
       * EPOLLERR: 出错，比如写读端已经关闭的pipe
       * EPOLLHUP: 套接字对端关闭
       * 出现这两种事件，应该同时触发fd的读和写事件，否则有可能出现注册的事件永远执行不到的情况
       */
      if (event.events & (EPOLLERR | EPOLLHUP)) {
        event.events |= (EPOLLIN | EPOLLOUT) & fd_ctx->events;
      }

      int real_events = NONE;
      if (event.events & EPOLLIN) {
        real_events |= READ;
      }
      if (event.events & EPOLLOUT) {
        real_events |= WRITE;
      }

      if ((fd_ctx->events & real_events) == NONE) {
        continue;
      }

      // 剔除已经发生的事件，将剩下的事件重新加入epoll_wait
      int left_events = (fd_ctx->events & ~real_events);
      int op          = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
      event.events    = EPOLLET | left_events;

      int rt2 = epoll_ctl(epfd_, op, fd_ctx->fd, &event);
      if (rt2) {
        LOG_ERROR(g_logger) << "epoll_ctl(" << epfd_ << ", " << op << "," << fd_ctx->fd << "," << event.events
                            << "):" << rt2 << " (" << errno << ") (" << strerror(errno) << ")";
        continue;
      }

      // 处理已经发生的事件，也就是让调度器调度指定的函数或协程
      if (real_events & READ) {
        fd_ctx->TriggerEvent(READ);
        --pending_event_cnt_;
      }
      if (real_events & WRITE) {
        fd_ctx->TriggerEvent(WRITE);
        --pending_event_cnt_;
      }
    }

    /**
     * 一旦处理完所有的事件，idle协程yield，这样可以让调度协程(Scheduler::run)重新检查是否有新任务要调度
     * 上面triggerEvent实际也只是把对应的fiber重新加入调度，要执行的话还要等idle协程退出
     */
    Fiber::ptr cur     = Fiber::GetRunningFiber();
    auto       raw_ptr = cur.get();
    cur.reset();

    raw_ptr->Yield();
  }
}

void IOManager::OnTimerInsertedAtFront() { Tickle(); }

}  // namespace gudov
