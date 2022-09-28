#include "hook.h"

#include <dlfcn.h>
#include <stdarg.h>

#include <string>

#include "config.h"
#include "fdmanager.h"
#include "fiber.h"
#include "iomanager.h"
#include "log.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_NAME("system");

namespace gudov {

static thread_local bool t_hookEnable = false;

#define HOOK_FUN(XX) \
  XX(sleep)          \
  XX(usleep)         \
  XX(nanosleep)      \
  XX(socket)         \
  XX(connect)        \
  XX(accept)         \
  XX(read)           \
  XX(readv)          \
  XX(recv)           \
  XX(recvfrom)       \
  XX(recvmsg)        \
  XX(write)          \
  XX(writev)         \
  XX(send)           \
  XX(sendto)         \
  XX(sendmsg)        \
  XX(close)          \
  XX(fcntl)          \
  XX(ioctl)          \
  XX(getsockopt)     \
  XX(setsockopt)

void hookInit() {
  static bool isInit = false;

  if (isInit) {
    return;
  }

#define XX(name) name##F = (name##Fun)dlsym(RTLD_NEXT, #name);
  HOOK_FUN(XX);
#undef XX
}

struct _HookIniter {
  _HookIniter() { hookInit(); }
};

static _HookIniter s_hookIniter;

bool isHookEnable() { return t_hookEnable; }

void setHookEnable(bool flag) { t_hookEnable = flag; }

}  // namespace gudov

struct TimerInfo {
  int cancelled = 0;
};

template <typename OriginFun, typename... Args>
static ssize_t doIO(int fd, OriginFun fun, const std::string& hookFunName,
                    uint32_t event, int timeoutSo, Args&&... args) {
  if (gudov::t_hookEnable) {
    return fun(fd, std::forward<Args>(args)...);
  }

  gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(fd);
  if (!ctx) {
    return fun(fd, std::forward<Args>(args)...);
  }

  if (ctx->isClose()) {
    errno = EBADF;
    return -1;
  }

  if (!ctx->isSocket() || ctx->getUserNonblock()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  // 设置超时时间
  uint64_t                   to = ctx->getTimeout(timeoutSo);
  std::shared_ptr<TimerInfo> tinfo(new TimerInfo);

retry:
  ssize_t n = fun(fd, std::forward<Args>(args)...);
  while (n == -1 && errno == EINTR) {
    // 重试
    n = fun(fd, std::forward<Args>(args)...);
  }

  if (n == -1 && errno == EAGAIN) {
    // 需要异步操作
    gudov::IOManager*        iom = gudov::IOManager::GetThis();
    gudov::Timer::ptr        timer;
    std::weak_ptr<TimerInfo> winfo(tinfo);

    if (to != (uint64_t)-1) {
      // 设置了超时
      timer = iom->addConditionTimer(
          to,
          [winfo, fd, iom, event]() {
            auto t = winfo.lock();
            if (!t || t->cancelled) {
              return;
            }

            t->cancelled = ETIMEDOUT;
            iom->cancelEvent(fd, (gudov::IOManager::Event)(event));
          },
          winfo);
    }

    // 为 fd 添加一个协程并且 hold
    int rt = iom->addEvent(fd, (gudov::IOManager::Event)(event));
    if (rt) {
      GUDOV_LOG_ERROR(g_logger)
          << hookFunName << " addEvent(" << fd << ", " << event << ")";
      if (timer) {
        timer->cancel();
      }
    } else {
      gudov::Fiber::YieldToHold();
      if (timer) {
        timer->cancel();
      }
      if (tinfo->cancelled) {
        errno = tinfo->cancelled;
        return -1;
      }

      goto retry;
    }
  }

  return n;
}

extern "C" {
#define XX(name) name##Fun name##F = nullptr;
HOOK_FUN(XX)
#undef XX

unsigned int sleep(unsigned int seconds) {
  if (!gudov::t_hookEnable) {
    return sleepF(seconds);
  }

  gudov::Fiber::ptr fiber = gudov::Fiber::GetThis();
  gudov::IOManager* iom   = gudov::IOManager::GetThis();
  iom->addTimer(
      seconds * 1000,
      std::bind((void (gudov::Scheduler::*)(gudov::Fiber::ptr, int thread)) &
                    gudov::IOManager::schedule,
                iom, fiber, -1));
  gudov::Fiber::YieldToHold();
  return 0;
}

int usleep(useconds_t usec) {
  if (!gudov::t_hookEnable) {
    return usleepF(usec);
  }

  gudov::Fiber::ptr fiber = gudov::Fiber::GetThis();
  gudov::IOManager* iom   = gudov::IOManager::GetThis();
  iom->addTimer(
      usec / 1000,
      std::bind((void (gudov::Scheduler::*)(gudov::Fiber::ptr, int thread)) &
                    gudov::IOManager::schedule,
                iom, fiber, -1));
  gudov::Fiber::YieldToHold();
  return 0;
}

int nanosleep(const struct timespec* req, struct timespec* rem) {
  if (!gudov::t_hookEnable) {
    return nanosleepF(req, rem);
  }

  int timeoutMs = req->tv_sec * 1000 + req->tv_nsec / 1000000;

  gudov::Fiber::ptr fiber = gudov::Fiber::GetThis();
  gudov::IOManager* iom   = gudov::IOManager::GetThis();
  iom->addTimer(
      timeoutMs,
      std::bind((void (gudov::Scheduler::*)(gudov::Fiber::ptr, int thread)) &
                    gudov::IOManager::schedule,
                iom, fiber, -1));
  gudov::Fiber::YieldToHold();
  return 0;
}

int socket(int domain, int type, int protocol) {
  if (!gudov::t_hookEnable) {
    return socketF(domain, type, protocol);
  }
  int fd = socketF(domain, type, protocol);
  if (fd == -1) {
    return -1;
  }
  gudov::FdMgr::getInstance()->get(fd, true);
  return fd;
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  return connectF(sockfd, addr, addrlen);
}

int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
  int fd = doIO(sockfd, acceptF, "accept", gudov::IOManager::Event::READ,
                SO_RCVTIMEO, addr, addrlen);
  if (fd >= 0) {
    gudov::FdMgr::getInstance()->get(fd, true);
  }
  return fd;
}

ssize_t read(int fd, void* buf, size_t count) {
  return doIO(fd, readF, "read", gudov::IOManager::Event::READ, SO_RCVTIMEO,
              buf, count);
}

ssize_t readv(int fd, const struct iovec* iov, int iovcnt) {
  return doIO(fd, readvF, "readv", gudov::IOManager::Event::READ, SO_RCVTIMEO,
              iov, iovcnt);
}

ssize_t recv(int sockfd, void* buf, size_t len, int flags) {
  return doIO(sockfd, recvF, "recv", gudov::IOManager::Event::READ, SO_RCVTIMEO,
              buf, len, flags);
}

ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                 struct sockaddr* src_addr, socklen_t* addrlen) {
  return doIO(sockfd, recvfromF, "recvfrom", gudov::IOManager::Event::READ,
              SO_RCVTIMEO, buf, len, flags, src_addr, addrlen);
}

ssize_t recvmsg(int sockfd, struct msghdr* msg, int flags) {
  return doIO(sockfd, recvmsgF, "recvmsg", gudov::IOManager::Event::READ,
              SO_RCVTIMEO, msg, flags);
}

ssize_t write(int fd, const void* buf, size_t count) {
  return doIO(fd, writeF, "write", gudov::IOManager::Event::WRITE, SO_SNDTIMEO,
              buf, count);
}

ssize_t writev(int fd, const struct iovec* iov, int iovcnt) {
  return doIO(fd, writevF, "writev", gudov::IOManager::Event::WRITE,
              SO_SNDTIMEO, iov, iovcnt);
}

ssize_t send(int s, const void* msg, size_t len, int flags) {
  return doIO(s, sendF, "send", gudov::IOManager::Event::WRITE, SO_SNDTIMEO,
              msg, len, flags);
}

ssize_t sendto(int s, const void* msg, size_t len, int flags,
               const struct sockaddr* to, socklen_t tolen) {
  return doIO(s, sendtoF, "sendto", gudov::IOManager::Event::WRITE, SO_SNDTIMEO,
              msg, len, flags, to, tolen);
}

ssize_t sendmsg(int s, const struct msghdr* msg, int flags) {
  return doIO(s, sendmsgF, "sendmsg", gudov::IOManager::Event::WRITE,
              SO_SNDTIMEO, msg, flags);
}

int close(int fd) {
  if (!gudov::t_hookEnable) {
    return closeF(fd);
  }

  gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(fd);
  if (ctx) {
    auto iom = gudov::IOManager::GetThis();
    if (iom) {
      iom->cancelAll(fd);
    }
    gudov::FdMgr::getInstance()->del(fd);
  }
  return closeF(fd);
}

int fcntl(int fd, int cmd, ...) {
  va_list va;
  va_start(va, cmd);
  switch (cmd) {
    case F_SETFL: {
      int arg = va_arg(va, int);
      va_end(va);
      gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(fd);
      if (!ctx || ctx->isClose() || !ctx->isSocket()) {
        return fcntlF(fd, cmd, arg);
      }
      ctx->setUserNonblock(arg & O_NONBLOCK);
      if (ctx->getSysNonblock()) {
        arg |= O_NONBLOCK;
      } else {
        arg &= ~O_NONBLOCK;
      }
      return fcntlF(fd, cmd, arg);
    }
    case F_GETFL: {
      va_end(va);
      int                   arg = fcntlF(fd, cmd);
      gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(fd);
      if (!ctx || ctx->isClose() || !ctx->isSocket()) {
        return arg;
      }
      if (ctx->getUserNonblock()) {
        return arg | O_NONBLOCK;
      } else {
        return arg & ~O_NONBLOCK;
      }
    }
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
    case F_SETFD:
    case F_SETOWN:
    case F_SETSIG:
    case F_SETLEASE:
    case F_NOTIFY:
    case F_SETPIPE_SZ: {
      int arg = va_arg(va, int);
      va_end(va);
      return fcntlF(fd, cmd, arg);
    }
    case F_GETFD:
    case F_GETOWN:
    case F_GETSIG:
    case F_GETLEASE:
    case F_GETPIPE_SZ: {
      va_end(va);
      return fcntlF(fd, cmd);
    }
    case F_SETLK:
    case F_SETLKW:
    case F_GETLK: {
      struct flock* arg = va_arg(va, struct flock*);
      va_end(va);
      return fcntlF(fd, cmd, arg);
    }
    case F_GETOWN_EX:
    case F_SETOWN_EX: {
      struct f_owner_exlock* arg = va_arg(va, struct f_owner_exlock*);
      va_end(va);
      return fcntlF(fd, cmd, arg);
    }
    default: {
      va_end(va);
      return fcntlF(fd, cmd);
    }
  }
}

int ioctl(int d, unsigned long request, ...) {
  va_list va;
  va_start(va, request);
  void* arg = va_arg(va, void*);
  va_end(va);

  if (FIONBIO == request) {
    bool                  userNonblock = !!*(int*)arg;
    gudov::FdContext::ptr ctx          = gudov::FdMgr::getInstance()->get(d);
    if (!ctx || ctx->isClose() || !ctx->isSocket()) {
      return ioctlF(d, request, arg);
    }
    ctx->setUserNonblock(userNonblock);
  }
  return ioctlF(d, request, arg);
}

int getsockopt(int sockfd, int level, int optname, void* optval,
               socklen_t* optlen) {
  return getsockoptF(sockfd, level, optname, optval, optlen);
}

int setsockopt(int sockfd, int level, int optname, const void* optval,
               socklen_t optlen) {
  if (!gudov::t_hookEnable) {
    return setsockoptF(sockfd, level, optname, optval, optlen);
  }
  if (level == SOL_SOCKET) {
    if (optname == SO_RCVTIMEO || optname == SO_SNDTIMEO) {
      gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(sockfd);
      if (ctx) {
        const timeval* v = (const timeval*)optval;
        ctx->setTimeout(optname, v->tv_sec * 1000 + v->tv_usec / 1000);
      }
    }
  }
  return setsockoptF(sockfd, level, optname, optval, optlen);
}
}
