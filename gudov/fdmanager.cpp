#include "fdmanager.h"

#include "hook.h"
#include "sys/stat.h"

namespace gudov {

FdCtx::FdCtx(int fd)
    : is_init_(false),
      is_socket_(false),
      sys_nonblock_(false),
      user_nonblock_(false),
      is_close_(false),
      fd_(fd),
      recv_timeout_(-1),
      send_timeout_(-1) {
  Init();
}

FdCtx::~FdCtx() {}

bool FdCtx::Init() {
  if (is_init_) {
    return true;
  }

  recv_timeout_ = -1;
  send_timeout_ = -1;

  struct stat fdStat;
  if (-1 == fstat(fd_, &fdStat)) {
    is_init_   = false;
    is_socket_ = false;
  } else {
    is_init_   = true;
    is_socket_ = S_ISSOCK(fdStat.st_mode);
  }

  if (is_socket_) {
    int flags = fcntlF(fd_, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
      fcntlF(fd_, F_SETFL, flags | O_NONBLOCK);
    }
    sys_nonblock_ = true;
  } else {
    sys_nonblock_ = false;
  }

  user_nonblock_ = false;
  is_close_      = false;
  return is_init_;
}

void FdCtx::SetTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    recv_timeout_ = v;
  } else {
    send_timeout_ = v;
  }
}

uint64_t FdCtx::GetTimeout(int type) {
  if (type == SO_RCVTIMEO) {
    return recv_timeout_;
  } else {
    return send_timeout_;
  }
}

FdManager::FdManager() { data_.resize(64); }

FdManager::~FdManager() {}

FdCtx::ptr FdManager::Get(int fd, bool autoCreate) {
  if (fd < 0) {
    return nullptr;
  }
  RWMutexType::ReadLock lock(mutex_);
  if (data_.size() <= static_cast<size_t>(fd)) {
    if (autoCreate == false) {
      return nullptr;
    }
  } else {
    if (data_[fd] || !autoCreate) {
      return data_[fd];
    }
  }
  lock.Unlock();

  RWMutexType::WriteLock lock2(mutex_);

  FdCtx::ptr ctx(new FdCtx(fd));
  if (fd >= static_cast<int>(data_.size())) {
    data_.resize(fd * 1.5);
  }
  data_[fd] = ctx;

  return ctx;
}

void FdManager::Del(int fd) {
  RWMutexType::WriteLock lock(mutex_);
  if (data_.size() <= static_cast<size_t>(fd)) {
    return;
  }

  data_[fd].reset();
}

}  // namespace gudov
