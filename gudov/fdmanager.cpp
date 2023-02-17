#include "fdmanager.h"

#include "hook.h"
#include "sys/stat.h"

namespace gudov {

FdContext::FdContext(int fd)
    : m_is_init(false),
      m_is_socket(false),
      m_sys_nonblock(false),
      m_user_nonblock(false),
      m_is_close(false),
      m_fd(fd),
      m_recv_timeout(-1),
      m_send_timeout(-1) {
  init();
}

FdContext::~FdContext() {}

bool FdContext::init() {
  if (m_is_init) {
    return true;
  }

  m_recv_timeout = -1;
  m_send_timeout = -1;

  struct stat fdStat;
  if (-1 == fstat(m_fd, &fdStat)) {
    m_is_init   = false;
    m_is_socket = false;
  } else {
    m_is_init   = true;
    m_is_socket = S_ISSOCK(fdStat.st_mode);
  }

  if (m_is_socket) {
    int flags = fcntlF(m_fd, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
      fcntlF(m_fd, F_SETFL, flags | O_NONBLOCK);
    }
    m_sys_nonblock = true;
  } else {
    m_sys_nonblock = false;
  }

  m_user_nonblock = false;
  m_is_close      = false;
  return m_is_init;
}

void FdContext::setTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    m_recv_timeout = v;
  } else {
    m_send_timeout = v;
  }
}

uint64_t FdContext::getTimeout(int type) {
  if (type == SO_RCVTIMEO) {
    return m_recv_timeout;
  } else {
    return m_send_timeout;
  }
}

FdManager::FdManager() { _datas.resize(64); }

FdManager::~FdManager() {}

FdContext::ptr FdManager::get(int fd, bool autoCreate) {
  RWMutexType::ReadLock lock(m_mutex);
  if (_datas.size() <= static_cast<size_t>(fd)) {
    if (autoCreate == false) {
      return nullptr;
    }
  } else {
    if (_datas[fd] || autoCreate) {
      return _datas[fd];
    }
  }
  lock.unlock();

  RWMutexType::WriteLock lock2(m_mutex);

  FdContext::ptr ctx(new FdContext(fd));
  _datas[fd] = ctx;

  return ctx;
}

void FdManager::del(int fd) {
  RWMutexType::WriteLock lock(m_mutex);
  if (_datas.size() <= static_cast<size_t>(fd)) {
    return;
  }

  _datas[fd].reset();
}

}  // namespace gudov
