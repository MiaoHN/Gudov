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
  events            = (Event)(events & ~event);
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

  rt = fcntl(_tickleFds[0], F_SETFL, O_NONBLOCK);
  GUDOV_ASSERT(!rt);

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
  FdContext*            fdCtx = nullptr;
  RWMutexType::ReadLock lock(_mutex);
  if ((int)_fdContexts.size() > fd) {
    fdCtx = _fdContexts[fd];
    lock.unlock();
  } else {
    lock.unlock();
    RWMutexType::WriteLock lock2(_mutex);
    contextResize(fd * 1.5);
    fdCtx = _fdContexts[fd];
  }

  FdContext::MutexType::Lock lock2(fdCtx->mutex);
  if (fdCtx->events & event) {
    GUDOV_LOG_ERROR(g_logger)
        << "addEvent assert fd=" << fd << " event=" << event
        << " fdCtx.event=" << fdCtx->events;
    GUDOV_ASSERT(!(fdCtx->events & event));
  }

  int         op = fdCtx->events ? EPOLL_CTL_MOD : EPOLL_CTL_ADD;
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
  GUDOV_ASSERT(!eventCtx.scheduler && !eventCtx.fiber && !eventCtx.cb);

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
    return false;
  }
  FdContext* fdCtx = _fdContexts[fd];
  lock.unlock();

  FdContext::MutexType::Lock lock2(fdCtx->mutex);
  if (!(fdCtx->events & event)) {
    return false;
  }

  Event       new_events = (Event)(fdCtx->events & ~event);
  int         op         = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events   = EPOLLET | new_events;
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
  fdCtx->events                      = new_events;
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

  Event       new_events = (Event)(fdCtx->events & ~event);
  int         op         = new_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
  epoll_event epevent;
  epevent.events   = EPOLLET | new_events;
  epevent.data.ptr = fdCtx;

  int rt = epoll_ctl(_epfd, op, fd, &epevent);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "epoll_ctl(" << _epfd << ", " << op << "," << fd << ","
        << epevent.events << "):" << rt << " (" << errno << ") ("
        << strerror(errno) << ")";
    return false;
  }

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
  epoll_event*                 events = new epoll_event[64]();
  std::shared_ptr<epoll_event> shared_events(
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
      rt = epoll_wait(_epfd, events, 64, (int)nextTimeout);
      if (rt < 0 && errno == EINTR) {
      } else {
        break;
      }
    } while (true);

    std::vector<std::function<void()> > cbs;
    listExpiredCb(cbs);
    if (!cbs.empty()) {
      schedule(cbs.begin(), cbs.end());
      cbs.clear();
    }

    for (int i = 0; i < rt; ++i) {
      epoll_event& event = events[i];
      if (event.data.fd == _tickleFds[0]) {
        uint8_t dummy;
        while (read(_tickleFds[0], &dummy, 1) == 1)
          ;
        continue;
      }

      FdContext*                 fdCtx = (FdContext*)event.data.ptr;
      FdContext::MutexType::Lock lock(fdCtx->mutex);
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

      int left_events = (fdCtx->events & ~realEvents);
      int op          = left_events ? EPOLL_CTL_MOD : EPOLL_CTL_DEL;
      event.events    = EPOLLET | left_events;

      int rt2 = epoll_ctl(_epfd, op, fdCtx->fd, &event);
      if (rt2) {
        GUDOV_LOG_ERROR(g_logger)
            << "epoll_ctl(" << _epfd << ", " << op << "," << fdCtx->fd << ","
            << event.events << "):" << rt2 << " (" << errno << ") ("
            << strerror(errno) << ")";
        continue;
      }

      if (realEvents & READ) {
        fdCtx->triggerEvent(READ);
        --_pendingEventCount;
      }
      if (realEvents & WRITE) {
        fdCtx->triggerEvent(WRITE);
        --_pendingEventCount;
      }
    }

    Fiber::ptr cur     = Fiber::GetThis();
    auto       raw_ptr = cur.get();
    cur.reset();

    raw_ptr->swapOut();
  }
}

void IOManager::onTimerInsertedAtFront() { tickle(); }

}  // namespace gudov
