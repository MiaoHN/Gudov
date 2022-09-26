#include "iomanager.h"

#include <fcntl.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "log.h"
#include "macro.h"

namespace gudov {

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

IOManager::FdContext::EventContext& IOManager::FdContext::getContext(
    IOManager::Event event) {
  switch (event) {
    case IOManager::Event::READ:
      return read;
    case IOManager::Event::WRITE:
      return write;
    default:
      GUDOV_ASSERT2(false, "getContext");
  }
}

void IOManager::FdContext::resetContext(
    IOManager::FdContext::EventContext& ctx) {
  ctx.scheduler = nullptr;
  ctx.fiber.reset();
  ctx.cb = nullptr;
}

void IOManager::FdContext::triggerEvent(IOManager::Event event) {
  GUDOV_ASSERT(events & event);
  events = (Event)(events & ~event);

  EventContext& ctx = getContext(event);

  if (ctx.cb) {
    ctx.scheduler->schedule(&ctx.cb);
  } else {
    ctx.scheduler->schedule(&ctx.fiber);
  }
  ctx.scheduler = nullptr;
  return;
}

IOManager::IOManager(size_t threads, bool useCaller, const std::string& name)
    : Scheduler(threads, useCaller, name) {
  _epfd = epoll_create(5000);
  GUDOV_ASSERT(_epfd > 0);

  int rt = pipe(_tickleFds);
  GUDOV_ASSERT(!rt);

  epoll_event event;
  memset(&event, 0, sizeof(epoll_event));
  event.events  = EPOLLIN | EPOLLET;
  event.data.fd = _tickleFds[0];

  // 非阻塞方式
  rt = fcntl(_tickleFds[0], F_SETFL, O_NONBLOCK);
  GUDOV_ASSERT(!rt);

  // 如果管道可读，epoll_wait() 就会返回
  rt = epoll_ctl(_epfd, EPOLL_CTL_ADD, _tickleFds[0], &event);
  GUDOV_ASSERT(!rt);

  contextResize(32);

  start();
}

IOManager::~IOManager() {
  stop();
  close(_epfd);
  close(_tickleFds[0]);
  close(_tickleFds[1]);

  for (size_t i = 0; i < _fdContexts.size(); ++i) {
    if (_fdContexts[i]) {
      delete _fdContexts[i];
    }
  }
}

void IOManager::contextResize(size_t size) {
  _fdContexts.resize(size);

  for (size_t i = 0; i < _fdContexts.size(); ++i) {
    if (!_fdContexts[i]) {
      _fdContexts[i]     = new FdContext;
      _fdContexts[i]->fd = i;
    }
  }
}

int IOManager::addEvent(int fd, Event event, std::function<void()> cb) {
  FdContext* fdCtx = nullptr;
  // 得到对应 fd 下标的 context，如果越界就将 _fdContexts 扩容
  RWMutexType::ReadLock lock(_mutex);
  if ((int)_fdContexts.size() > fd) {
    fdCtx = _fdContexts[fd];
    lock.unlock();
  } else {
    lock.unlock();
    RWMutexType::WriteLock lock2(_mutex);
    contextResize(fd * 1.5);
    GUDOV_ASSERT((int)_fdContexts.size() > fd);
    fdCtx = _fdContexts[fd];
  }

  FdContext::MutexType::Lock lock2(fdCtx->mutex);
  if (fdCtx->events & event) {
    // 已经添加过该事件
    GUDOV_LOG_ERROR(g_logger)
        << "addEvent assert fd=" << fd << " event=" << event
        << " fdCtx.event=" << fdCtx->events;
    GUDOV_ASSERT(!(fdCtx->events & event));
  }

  // 判断是修改还是添加
  int op = fdCtx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;

  epoll_event epevent;
  epevent.events   = EPOLLET | fdCtx->events | event;
  epevent.data.ptr = fdCtx;

  int rt = epoll_ctl(_epfd, op, fd, &epevent);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "epoll_ctl(" << _epfd << ", " << op << "," << fd << ","
        << epevent.events << "):" << rt << " (" << errno << ") ("
        << strerror(errno) << ")";
    return -1;
  }

  ++_pendingEventCount;

  fdCtx->events                     = (Event)(fdCtx->events | event);
  FdContext::EventContext& eventCtx = fdCtx->getContext(event);

  // 确保该事件还未分配 scheduler, fiber 以及 callback 函数
  GUDOV_ASSERT(!eventCtx.scheduler && !eventCtx.fiber && !eventCtx.cb);

  // 为该事件分配调用资源
  eventCtx.scheduler = Scheduler::GetThis();
  if (cb) {
    eventCtx.cb.swap(cb);
  } else {
    eventCtx.fiber = Fiber::GetThis();
    GUDOV_ASSERT(eventCtx.fiber->getState() == Fiber::EXEC);
  }
  return 0;
}

bool IOManager::delEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(_mutex);
  if ((int)_fdContexts.size() <= fd) {
    // 该 fd 超出范围
    return false;
  }
  FdContext* fdCtx = _fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fdCtx->mutex);
  if (!(fdCtx->events & event)) {
    // 该 fd 不包含此事件
    return false;
  }

  Event newEvents = (Event)(fdCtx->events & ~event);
  int   op        = newEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;

  epoll_event epevent;
  epevent.events   = EPOLLET | newEvents;
  epevent.data.ptr = fdCtx;

  int rt = epoll_ctl(_epfd, op, fd, &epevent);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "epoll_ctl(" << _epfd << ", " << op << "," << fd << ","
        << epevent.events << "):" << rt << " (" << errno << ") ("
        << strerror(errno) << ")";
    return false;
  }

  --_pendingEventCount;

  // 重置该 fdContext
  fdCtx->events                      = newEvents;
  FdContext::EventContext& event_ctx = fdCtx->getContext(event);

  fdCtx->resetContext(event_ctx);
  return true;
}

bool IOManager::cancelEvent(int fd, Event event) {
  RWMutexType::ReadLock lock(_mutex);
  if ((int)_fdContexts.size() <= fd) {
    return false;
  }
  FdContext* fdCtx = _fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fdCtx->mutex);
  if (!(fdCtx->events & event)) {
    return false;
  }

  Event       newEvents = (Event)(fdCtx->events & ~event);
  int         op        = newEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events   = EPOLLET | newEvents;
  epevent.data.ptr = fdCtx;

  int rt = epoll_ctl(_epfd, op, fd, &epevent);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "epoll_ctl(" << _epfd << ", " << op << "," << fd << ","
        << epevent.events << "):" << rt << " (" << errno << ") ("
        << strerror(errno) << ")";
    return false;
  }

  // 删除前触发一次
  fdCtx->triggerEvent(event);
  --_pendingEventCount;
  return true;
}

bool IOManager::cancelAll(int fd) {
  RWMutexType::ReadLock lock(_mutex);
  if ((int)_fdContexts.size() <= fd) {
    return false;
  }
  FdContext* fdCtx = _fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fdCtx->mutex);
  if (!fdCtx->events) {
    return false;
  }

  int         op = EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events   = 0;
  epevent.data.ptr = fdCtx;

  int rt = epoll_ctl(_epfd, op, fd, &epevent);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "epoll_ctl(" << _epfd << ", " << op << "," << fd << ","
        << epevent.events << "):" << rt << " (" << errno << ") ("
        << strerror(errno) << ")";
    return false;
  }

  if (fdCtx->events & READ) {
    fdCtx->triggerEvent(READ);
    --_pendingEventCount;
  }
  if (fdCtx->events & WRITE) {
    fdCtx->triggerEvent(WRITE);
    --_pendingEventCount;
  }

  GUDOV_ASSERT(fdCtx->events == 0);
  return true;
}

IOManager* IOManager::GetThis() {
  return dynamic_cast<IOManager*>(Scheduler::GetThis());
}

void IOManager::tickle() {
  if (hasIdleThreads()) {
    return;
  }
  int rt = write(_tickleFds[1], "T", 1);
  GUDOV_ASSERT(rt == 1);
}

bool IOManager::stopping(uint64_t& timeout) {
  timeout = getNextTimer();
  return timeout == ~0ull && _pendingEventCount == 0 && Scheduler::stopping();
}

bool IOManager::stopping() {
  uint64_t timeout = 0;
  return stopping(timeout);
}

void IOManager::idle() {
  static const uint64_t maxEvents = 64;

  epoll_event* events = new epoll_event[maxEvents]();

  // 函数结束时删除 event
  std::shared_ptr<epoll_event> sharedEvents(
      events, [](epoll_event* ptr) { delete[] ptr; });

  while (true) {
    uint64_t nextTimeout = 0;
    if (stopping(nextTimeout)) {
      GUDOV_LOG_INFO(g_logger) << "name=" << getName() << " idle stopping exit";
      break;
    }

    int rt = 0;
    do {
      static const int MAX_TIMEOUT = 3000;
      if (nextTimeout != ~0ull) {
        nextTimeout =
            (int)nextTimeout > MAX_TIMEOUT ? MAX_TIMEOUT : nextTimeout;
      } else {
        nextTimeout = MAX_TIMEOUT;
      }
      // 等待事件发生
      rt = epoll_wait(_epfd, events, maxEvents, (int)nextTimeout);
      if (rt < 0 && errno == EINTR) {
      } else {
        break;
      }
    } while (true);

    std::vector<std::function<void()>> cbs;
    listExpiredCb(cbs);
    if (!cbs.empty()) {
      schedule(cbs.begin(), cbs.end());
      cbs.clear();
    }

    for (int i = 0; i < rt; ++i) {
      epoll_event& event = events[i];
      if (event.data.fd == _tickleFds[0]) {
        // _ticklefd[0] 用于通知事件产生这里只需读完剩余数据
        uint8_t dummy;
        while (read(_tickleFds[0], &dummy, 1) == 1)
          ;
        continue;
      }

      FdContext* fdCtx = (FdContext*)event.data.ptr;

      FdContext::MutexType::Lock lock(fdCtx->mutex);

      // 出错或者套接字对端关闭，此时需要同时打开读写事件，否则可能注册事件无法执行
      if (event.events & (EPOLLERR | EPOLLHUP)) {
        event.events |= EPOLLIN | EPOLLOUT;
      }
      int realEvents = NONE;
      if (event.events & EPOLLIN) {
        realEvents |= READ;
      }
      if (event.events & EPOLLOUT) {
        realEvents |= WRITE;
      }

      if ((fdCtx->events & realEvents) == NONE) {
        continue;
      }

      // 提取未处理事件
      int leftEvents = (fdCtx->events & ~realEvents);
      int op         = leftEvents ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
      event.events   = EPOLLET | leftEvents;

      int rt2 = epoll_ctl(_epfd, op, fdCtx->fd, &event);
      if (rt2) {
        GUDOV_LOG_ERROR(g_logger)
            << "epoll_ctl(" << _epfd << ", " << op << "," << fdCtx->fd << ","
            << event.events << "):" << rt2 << " (" << errno << ") ("
            << strerror(errno) << ")";
        continue;
      }

      if (realEvents & READ) {
        // 触发读事件
        fdCtx->triggerEvent(READ);
        --_pendingEventCount;
      }
      if (realEvents & WRITE) {
        // 触发写事件
        fdCtx->triggerEvent(WRITE);
        --_pendingEventCount;
      }
    }

    Fiber::ptr cur     = Fiber::GetThis();
    auto       raw_ptr = cur.get();
    cur.reset();

    // 当前协程退出，scheduler 继续执行 run 进行调度
    raw_ptr->swapOut();
  }
}

void IOManager::onTimerInsertedAtFront() { tickle(); }

}  // namespace gudov
