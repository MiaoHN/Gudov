#pragma once

#include <pthread.h>
#include <semaphore.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "mutex.h"
#include "noncopyable.h"

namespace gudov {

class Thread : public NonCopyable {
 public:
  using ptr = std::shared_ptr<Thread>;
  Thread(std::function<void()> callback, const std::string& name);
  ~Thread();

  /**
   * @brief 获取线程的全局 ID
   *
   * @return pid_t
   */
  pid_t GetID() const { return id_; }

  const std::string& GetName() const { return name_; }

  /**
   * @brief 等待线程结束
   *
   */
  void Join();

  /**
   * @brief 获取当前运行的线程
   *
   * @return Thread*
   */
  static Thread* GetRunningThread();

  /**
   * @brief 获取当前运行的线程的名称
   *
   * @return Thread*
   */
  static const std::string& GetRunningThreadName();

  static void SetRunningThreadName(const std::string& name);

 private:
  static void* Run(void* args);

 private:
  pid_t       id_     = -1;  // 全局 ID，通过 syscall() 得到
  pthread_t   thread_ = 0;   // 进程内 ID，pthread_create() 创建时得到
  std::string name_;         // 线程名称

  std::function<void()> callback_;  // 线程内运行的函数

  Semaphore semaphore_;
};

}  // namespace gudov
