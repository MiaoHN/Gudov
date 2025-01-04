/**
 * @file fdmanager.h
 * @author MiaoHN (582418227@qq.com)
 * @brief fd 管理器封装
 * @version 0.1
 * @date 2023-02-17
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

#include <memory>
#include <vector>

#include "singleton.h"
#include "thread.h"

namespace gudov {

/**
 * @brief 全局的 fd 信息封装
 * @attention IOManager 中的 FdContext 是用来处理事件的，和这个 FdContext 不一样
 *
 */
class FdContext : public std::enable_shared_from_this<FdContext> {
 public:
  using ptr = std::shared_ptr<FdContext>;

  FdContext(int fd);
  ~FdContext();

  bool init();
  bool isInit() const { return m_is_init; }
  bool isSocket() const { return m_is_socket; }
  bool isClose() const { return m_is_close; }
  bool close();

  void setUserNonblock(bool v) { m_user_nonblock = v; }
  bool getUserNonblock() const { return m_user_nonblock; }

  void setSysNonblock(bool v) { m_sys_nonblock = v; }
  bool getSysNonblock() const { return m_sys_nonblock; }

  void     setTimeout(int type, uint64_t v);
  uint64_t getTimeout(int type);

 private:
  bool m_is_init : 1;
  bool m_is_socket : 1;
  bool m_sys_nonblock : 1;
  bool m_user_nonblock : 1;
  bool m_is_close : 1;

  int m_fd;

  uint64_t m_recv_timeout;
  uint64_t m_send_timeout;
};

class FdManager {
 public:
  using RWMutexType = RWMutex;

  FdManager();
  ~FdManager();

  FdContext::ptr get(int fd, bool autoCreate = false);
  void           del(int fd);

 private:
  RWMutexType m_mutex;

  std::vector<FdContext::ptr> _datas;
};

using FdMgr = Singleton<FdManager>;

}  // namespace gudov
