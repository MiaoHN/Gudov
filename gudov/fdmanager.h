#ifndef __GUDOV_FDMANAGER_H__
#define __GUDOV_FDMANAGER_H__

#include <memory>
#include <vector>

#include "singleton.h"
#include "thread.h"

namespace gudov {

class FdContext : public std::enable_shared_from_this<FdContext> {
 public:
  using ptr = std::shared_ptr<FdContext>;

  FdContext(int fd);
  ~FdContext();

  bool init();
  bool isInit() const { return _isInit; }
  bool isSocket() const { return _isSocket; }
  bool isClose() const { return _isClose; }
  bool close();

  void setUserNonblock(bool v) { _userNonblock = v; }
  bool getUserNonblock() const { return _userNonblock; }

  void setSysNonblock(bool v) { _sysNonblock = v; }
  bool getSysNonblock() const { return _sysNonblock; }

  void     setTimeout(int type, uint64_t v);
  uint64_t getTimeout(int type);

 private:
  bool _isInit : 1;
  bool _isSocket : 1;
  bool _sysNonblock : 1;
  bool _userNonblock : 1;
  bool _isClose : 1;

  int _fd;

  uint64_t _recvTimeout;
  uint64_t _sendTimeout;
};

class FdManager {
 public:
  using RWMutexType = RWMutex;

  FdManager();
  ~FdManager();

  FdContext::ptr get(int fd, bool autoCreate = false);
  void           del(int fd);

 private:
  RWMutexType _mutex;

  std::vector<FdContext::ptr> _datas;
};

using FdMgr = Singleton<FdManager>;

}  // namespace gudov

#endif  // __GUDOV_FDMANAGER_H__