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
class FdCtx : public std::enable_shared_from_this<FdCtx> {
 public:
  using ptr = std::shared_ptr<FdCtx>;

  FdCtx(int fd);
  ~FdCtx();

  bool Init();
  bool IsInit() const { return is_init_; }
  bool IsSocket() const { return is_socket_; }
  bool IsClose() const { return is_close_; }
  bool Close();

  void SetUserNonblock(bool v) { user_nonblock_ = v; }
  bool GetUserNonblock() const { return user_nonblock_; }

  void SetSysNonblock(bool v) { sys_nonblock_ = v; }
  bool GetSysNonblock() const { return sys_nonblock_; }

  void     SetTimeout(int type, uint64_t v);
  uint64_t GetTimeout(int type);

 private:
  bool is_init_ : 1;
  bool is_socket_ : 1;
  bool sys_nonblock_ : 1;
  bool user_nonblock_ : 1;
  bool is_close_ : 1;

  int fd_;

  uint64_t recv_timeout_;
  uint64_t send_timeout_;
};

class FdManager {
 public:
  using RWMutexType = RWMutex;

  FdManager();
  ~FdManager();

  FdCtx::ptr Get(int fd, bool autoCreate = false);
  void           Del(int fd);

 private:
  RWMutexType mutex_;

  std::vector<FdCtx::ptr> data_;
};

using FdMgr = Singleton<FdManager>;

}  // namespace gudov
