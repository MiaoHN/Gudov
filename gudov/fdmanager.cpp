#include "fdmanager.h"

#include "hook.h"
#include "sys/stat.h"

namespace gudov {

FdContext::FdContext(int fd)
    : _isInit(false),
      _isSocket(false),
      _sysNonblock(false),
      _userNonblock(false),
      _isClose(false),
      _fd(fd),
      _recvTimeout(-1),
      _sendTimeout(-1) {
  init();
}

FdContext::~FdContext() {}

bool FdContext::init() {
  if (_isInit) {
    return true;
  }

  _recvTimeout = -1;
  _sendTimeout = -1;

  struct stat fdStat;
  if (-1 == fstat(_fd, &fdStat)) {
    _isInit   = false;
    _isSocket = false;
  } else {
    _isInit   = true;
    _isSocket = S_ISSOCK(fdStat.st_mode);
  }

  if (_isSocket) {
    int flags = fcntlF(_fd, F_GETFL, 0);
    if (!(flags & O_NONBLOCK)) {
      fcntlF(_fd, F_SETFL, flags | O_NONBLOCK);
    }
    _sysNonblock = true;
  } else {
    _sysNonblock = false;
  }

  _userNonblock = false;
  _isClose      = false;
  return _isInit;
}

void FdContext::setTimeout(int type, uint64_t v) {
  if (type == SO_RCVTIMEO) {
    _recvTimeout = v;
  } else {
    _sendTimeout = v;
  }
}

uint64_t FdContext::getTimeout(int type) {
  if (type == SO_RCVTIMEO) {
    return _recvTimeout;
  } else {
    return _sendTimeout;
  }
}

FdManager::FdManager() { _datas.resize(64); }

FdManager::~FdManager() {}

FdContext::ptr FdManager::get(int fd, bool autoCreate) {
  RWMutexType::ReadLock lock(_mutex);
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

  RWMutexType::WriteLock lock2(_mutex);

  FdContext::ptr ctx(new FdContext(fd));
  _datas[fd] = ctx;

  return ctx;
}

void FdManager::del(int fd) {
  RWMutexType::WriteLock lock(_mutex);
  if (_datas.size() <= static_cast<size_t>(fd)) {
    return;
  }

  _datas[fd].reset();
}

}  // namespace gudov
