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
 * @brief Global fd context
 * @attention FdContext in `IOManager` is used to manage task, while `FdCtx` is used to manage fd
 *
 */
class FdCtx : public std::enable_shared_from_this<FdCtx> {
 public:
  using ptr = std::shared_ptr<FdCtx>;

  /**
   * @brief Construct a new Fd Ctx object
   */
  FdCtx(int fd);

  /**
   * @brief Destroy the Fd Ctx object
   */
  ~FdCtx();

  bool IsInit() const { return is_init_; }
  bool IsSocket() const { return is_socket_; }
  bool IsClose() const;

  void SetUserNonblock(bool v) { user_nonblock_ = v; }
  bool GetUserNonblock() const { return user_nonblock_; }

  void SetSysNonblock(bool v) { sys_nonblock_ = v; }
  bool GetSysNonblock() const { return sys_nonblock_; }

  void     SetTimeout(int type, uint64_t v);
  uint64_t GetTimeout(int type);

 private:
  /**
   * @brief Function: Initializes the `FdCtx` object. This function sets up various properties related to the file
   * descriptor represented by the `fd_` member variable of the `FdCtx` object.
   *
   * @return true if the initialization is successful or if the object has already been initialized.
   * @return false if the `fstat` call fails during the initialization process.
   */
  bool Init();

 private:
  bool is_init_ : 1;
  bool is_socket_ : 1;
  bool sys_nonblock_ : 1;
  bool user_nonblock_ : 1;

  int fd_;

  uint64_t recv_timeout_;
  uint64_t send_timeout_;
};

/// File descriptor manager
class FdManager {
 public:
  using RWMutexType = RWMutex;

  /// Construct a new `FdManager` object, default size is 64
  FdManager();

  /// Destruct the `FdManager` object
  ~FdManager();

  /**
   * @brief Get an pointer associated with the given file descriptor `fd`.
   *
   * @param fd An integer representing the file descriptor. This is the key used to retrieve the corresponding `FdCtx`
   * @param auto_create A boolean flag. If `true`, the function will create a new `FdCtx` object if one doesn't already
    * exist for the given `fd`. If `false`, it will not create a new object.
   * @return FdCtx::ptr If `fd` is negative or `auto_create` is `false` and no object exists for the `fd`, it returns
   `nullptr`.
   */
  FdCtx::ptr Get(int fd, bool auto_create = false);

  /**
   * @brief Delete the `FdCtx` associated with the given file descriptor `fd`.
   *
   * @param fd An integer representing the file descriptor.
   */
  void Del(int fd);

 private:
  /// protect `data_`
  RWMutexType mutex_;

  /// store all `FdCtx` objects
  std::vector<FdCtx::ptr> data_;
};

/// Global `FdManager` object
using FdMgr = Singleton<FdManager>;

}  // namespace gudov
