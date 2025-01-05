#pragma once

#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace gudov {

/**
 * @brief 是否启用 hook
 *
 * @return true
 * @return false
 */
bool IsHookEnable();

/**
 * @brief 开启或关闭 hook
 *
 * @param flag
 */
void SetHookEnable(bool flag);

}  // namespace gudov

extern "C" {

// ~ NOTE: 定义并声明函数指针，将这些函数指针指向系统原有的函数 ~

// sleep
typedef unsigned int (*sleepFun)(unsigned int seconds);
extern sleepFun sleepF;

typedef int (*usleepFun)(useconds_t usec);
extern usleepFun usleepF;

typedef int (*nanosleepFun)(const struct timespec *req, struct timespec *rem);
extern nanosleepFun nanosleepF;

// socket
typedef int (*socketFun)(int domain, int type, int protocol);
extern socketFun socketF;

typedef int (*connectFun)(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern connectFun connectF;

typedef int (*acceptFun)(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern acceptFun acceptF;

// read
typedef ssize_t (*readFun)(int fd, void *buf, size_t count);
extern readFun readF;

typedef ssize_t (*readvFun)(int fd, const struct iovec *iov, int iovcnt);
extern readvFun readvF;

typedef ssize_t (*recvFun)(int sockfd, void *buf, size_t len, int flags);
extern recvFun recvF;

typedef ssize_t (*recvfromFun)(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr,
                               socklen_t *addrlen);
extern recvfromFun recvfromF;

typedef ssize_t (*recvmsgFun)(int sockfd, struct msghdr *msg, int flags);
extern recvmsgFun recvmsgF;

// write
typedef ssize_t (*writeFun)(int fd, const void *buf, size_t count);
extern writeFun writeF;

typedef ssize_t (*writevFun)(int fd, const struct iovec *iov, int iovcnt);
extern writevFun writevF;

typedef ssize_t (*sendFun)(int s, const void *msg, size_t len, int flags);
extern sendFun sendF;

typedef ssize_t (*sendtoFun)(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
extern sendtoFun sendtoF;

typedef ssize_t (*sendmsgFun)(int s, const struct msghdr *msg, int flags);
extern sendmsgFun sendmsgF;

// close
typedef int (*closeFun)(int fd);
extern closeFun closeF;

// io control
typedef int (*fcntlFun)(int fd, int cmd, ...);
extern fcntlFun fcntlF;

typedef int (*ioctlFun)(int d, unsigned long request, ...);
extern ioctlFun ioctlF;

typedef int (*getsockoptFun)(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
extern getsockoptFun getsockoptF;

typedef int (*setsockoptFun)(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern setsockoptFun setsockoptF;

extern int connectWithTimeout(int fd, const struct sockaddr *addr, socklen_t addrlen, uint64_t timeoutMs);
}
