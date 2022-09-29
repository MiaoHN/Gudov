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

static ConfigVar<int>::ptr g_tcpConnectTimeout =
    Config::Lookup("tcp.connect.timeout", 5000, "tcp connect timeout");

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
  // 此处将系统函数挂载到 nameF 上
  HOOK_FUN(XX)
#undef XX
}

static uint64_t s_connectTimeout = -1;

struct _HookIniter {
  _HookIniter() {
    hookInit();
    s_connectTimeout = g_tcpConnectTimeout->getValue();

    g_tcpConnectTimeout->addListener(
        [](const int& oldValue, const int& newValue) {
          GUDOV_LOG_INFO(g_logger) << "tcp connect timeout changed from "
                                   << oldValue << " to " << newValue;
          s_connectTimeout = newValue;
        });
  }
};

// 使用结构体在初始化时完成 hook
static _HookIniter s_hookIniter;

bool isHookEnable() { return t_hookEnable; }

void setHookEnable(bool flag) { t_hookEnable = flag; }

}  // namespace gudov

struct TimerInfo {
  int cancelled = 0;
};

/**
 * @brief 通用的 IO 处理函数
 *
 * @tparam OriginFun 原始函数的函数指针
 * @tparam Args 原始函数所带参数
 * @param fd 待操作句柄
 * @param fun 原始的函数
 * @param hookFunName 要 hook 的函数名
 * @param event 处理的事件
 * @param timeoutSo 超时事件
 * @param args 函数的参数
 * @return ssize_t
 */
template <typename OriginFun, typename... Args>
static ssize_t doIO(int fd, OriginFun fun, const std::string& hookFunName,
                    uint32_t event, int timeoutSo, Args&&... args) {
  if (!gudov::isHookEnable()) {
    // 未启用 hook 时直接调用原有函数
    return fun(fd, std::forward<Args>(args)...);
  }

  // 尝试在 FdManager 中获得该 fd 句柄
  gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(fd);
  if (!ctx) {
    // 没找到则直接调用
    return fun(fd, std::forward<Args>(args)...);
  }

  // 如果句柄已关闭则发生错误
  if (ctx->isClose()) {
    errno = EBADF;
    return -1;
  }

  // 1. 只处理 socket
  // 2. 如果已经在用户态设置过非阻塞(fcntl or ioctl)，也不继续处理
  if (!ctx->isSocket() || ctx->getUserNonblock()) {
    return fun(fd, std::forward<Args>(args)...);
  }

  // 设置超时时间
  uint64_t                   timeout = ctx->getTimeout(timeoutSo);
  std::shared_ptr<TimerInfo> tinfo(new TimerInfo);

retry:
  // 尝试执行原始函数
  ssize_t n = fun(fd, std::forward<Args>(args)...);
  while (n == -1 && errno == EINTR) {
    // 发生中断，重试
    n = fun(fd, std::forward<Args>(args)...);
  }

  if (n == -1 && errno == EAGAIN) {
    // EAGAIN 表示暂无数据课操作，可稍后再试
    // 需要异步操作

    // 获取 IOManager 和定时器
    gudov::IOManager*        iom = gudov::IOManager::GetThis();
    gudov::Timer::ptr        timer;
    std::weak_ptr<TimerInfo> winfo(tinfo);

    if (timeout != (uint64_t)-1) {
      // 设置了超时
      timer = iom->addConditionTimer(
          timeout,
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

// 在这里声明函数指针变量

#define XX(name) name##Fun name##F = nullptr;
HOOK_FUN(XX)
#undef XX

unsigned int sleep(unsigned int seconds) {
  if (!gudov::isHookEnable()) {
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
  if (!gudov::isHookEnable()) {
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
  if (!gudov::isHookEnable()) {
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
  if (!gudov::isHookEnable()) {
    return socketF(domain, type, protocol);
  }
  int fd = socketF(domain, type, protocol);
  if (fd == -1) {
    return -1;
  }
  gudov::FdMgr::getInstance()->get(fd, true);
  return fd;
}

int connectWithTimeout(int fd, const struct sockaddr* addr, socklen_t addrlen,
                       uint64_t timeoutMs) {
  if (!gudov::isHookEnable()) {
    return connectF(fd, addr, addrlen);
  }
  gudov::FdContext::ptr ctx = gudov::FdMgr::getInstance()->get(fd);
  if (!ctx || ctx->isClose()) {
    errno = EBADF;
    return -1;
  }
  if (!ctx->isSocket()) {
    return connectF(fd, addr, addrlen);
  }

  if (ctx->getUserNonblock()) {
    return connectF(fd, addr, addrlen);
  }

  int n = connectF(fd, addr, addrlen);
  if (n == 0) {
    return 0;
  } else if (n != -1 || errno != EINPROGRESS) {
    return n;
  }

  gudov::IOManager*          iom = gudov::IOManager::GetThis();
  gudov::Timer::ptr          timer;
  std::shared_ptr<TimerInfo> tinfo(new TimerInfo);
  std::weak_ptr<TimerInfo>   winfo(tinfo);

  if (timeoutMs != (uint64_t)-1) {
    timer = iom->addConditionTimer(
        timeoutMs,
        [winfo, fd, iom]() {
          auto t = winfo.lock();
          if (!t || t->cancelled) {
            return;
          }
          t->cancelled = ETIMEDOUT;
          iom->cancelEvent(fd, gudov::IOManager::Event::WRITE);
        },
        winfo);
  }

  int rt = iom->addEvent(fd, gudov::IOManager::WRITE);
  if (rt == 0) {
    gudov::Fiber::YieldToHold();
    if (timer) {
      timer->cancel();
    }
    if (tinfo->cancelled) {
      errno = tinfo->cancelled;
      return -1;
    }
  } else {
    if (timer) {
      timer->cancel();
    }
    GUDOV_LOG_ERROR(g_logger) << "connect addEvent(" << fd << ", WRITE) error";
  }

  int       error = 0;
  socklen_t len   = sizeof(int);
  if (-1 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len)) {
    return -1;
  }
  if (!error) {
    return 0;
  } else {
    errno = error;
    return -1;
  }
}

int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
  return connectWithTimeout(sockfd, addr, addrlen, gudov::s_connectTimeout);
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
  if (!gudov::isHookEnable()) {
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
  if (!gudov::isHookEnable()) {
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
