#pragma once

#include <pthread.h>
#include <semaphore.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include "noncopyable.h"

namespace gudov {

/**
 * @brief 信号量
 *
 */
class Semaphore : public NonCopyable {
 public:
  Semaphore(uint32_t count = 0);
  ~Semaphore();

  /**
   * @brief 信号量不为 0 时减 1
   *
   */
  void Wait();

  /**
   * @brief 信号量加 1
   *
   */
  void Notify();

 private:
  sem_t semaphore_;
};

/**
 * @brief 通过 RAII 机制实现锁
 *
 * @tparam T
 */
template <typename T>
class ScopedLockImpl {
 public:
  ScopedLockImpl(T& mutex) : mutex_(mutex) { Lock(); }

  ~ScopedLockImpl() { Unlock(); }

  void Lock() {
    if (!locked_) {
      mutex_.Lock();
      locked_ = true;
    }
  }

  void Unlock() {
    if (locked_) {
      mutex_.Unlock();
      locked_ = false;
    }
  }

 private:
  T&   mutex_;
  bool locked_ = false;
};

/**
 * @brief 通过 RAII 机制实现的读锁
 *
 * @tparam T
 */
template <typename T>
class ReadScopedLockImpl {
 public:
  ReadScopedLockImpl(T& mutex) : mutex_(mutex) { Lock(); }

  ~ReadScopedLockImpl() { Unlock(); }

  void Lock() {
    if (!locked_) {
      mutex_.rdlock();
      locked_ = true;
    }
  }

  void Unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T&   mutex_;
  bool locked_ = false;
};

/**
 * @brief 通过 RAII 机制实现的写锁
 *
 * @tparam T
 */
template <typename T>
class WriteScopedLockImpl {
 public:
  WriteScopedLockImpl(T& mutex) : mutex_(mutex) { Lock(); }

  ~WriteScopedLockImpl() { Unlock(); }

  void Lock() {
    if (!locked_) {
      mutex_.wrlock();
      locked_ = true;
    }
  }

  void Unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T&   mutex_;
  bool locked_ = false;
};

class Mutex : public NonCopyable {
 public:
  using Locker = ScopedLockImpl<Mutex>;

  Mutex() { pthread_mutex_init(&mutex_, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&mutex_); }

  void Lock() { pthread_mutex_lock(&mutex_); }
  void Unlock() { pthread_mutex_unlock(&mutex_); }

 private:
  pthread_mutex_t mutex_;
};

/**
 * @brief 读写锁
 *
 */
class RWMutex : public NonCopyable {
 public:
  using ReadLock  = ReadScopedLockImpl<RWMutex>;
  using WriteLock = WriteScopedLockImpl<RWMutex>;

  RWMutex() { pthread_rwlock_init(&_lock, nullptr); }
  ~RWMutex() { pthread_rwlock_destroy(&_lock); }

  void rdlock() { pthread_rwlock_rdlock(&_lock); }
  void wrlock() { pthread_rwlock_wrlock(&_lock); }
  void unlock() { pthread_rwlock_unlock(&_lock); }

 private:
  pthread_rwlock_t _lock;
};

class Spinlock : public NonCopyable {
 public:
  using Locker = ScopedLockImpl<Spinlock>;

  Spinlock() { pthread_spin_init(&mutex_, 0); }
  ~Spinlock() { pthread_spin_destroy(&mutex_); }

  void Lock() { pthread_spin_lock(&mutex_); }
  void Unlock() { pthread_spin_unlock(&mutex_); }

 private:
  pthread_spinlock_t mutex_;
};

class CASLock : public NonCopyable {
 public:
  using Locker = ScopedLockImpl<CASLock>;

  CASLock() { mutex_.clear(); }
  ~CASLock() {}

  void Lock() {
    while (std::atomic_flag_test_and_set_explicit(&mutex_, std::memory_order_acquire))
      ;
  }

  void Unlock() { std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release); }

 private:
  volatile std::atomic_flag mutex_;
};

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
