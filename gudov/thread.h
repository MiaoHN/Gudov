#ifndef __GUDOV_THREAD_H__
#define __GUDOV_THREAD_H__

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
  void wait();

  /**
   * @brief 信号量加 1
   *
   */
  void notify();

 private:
  sem_t _semaphore;
};

/**
 * @brief 通过 RAII 机制实现锁
 *
 * @tparam T
 */
template <typename T>
class ScopedLockImpl {
 public:
  ScopedLockImpl(T& mutex) : m_mutex(mutex) { lock(); }

  ~ScopedLockImpl() { unlock(); }

  void lock() {
    if (!_locked) {
      m_mutex.lock();
      _locked = true;
    }
  }

  void unlock() {
    if (_locked) {
      m_mutex.unlock();
      _locked = false;
    }
  }

 private:
  T&   m_mutex;
  bool _locked = false;
};

/**
 * @brief 通过 RAII 机制实现的读锁
 *
 * @tparam T
 */
template <typename T>
class ReadScopedLockImpl {
 public:
  ReadScopedLockImpl(T& mutex) : m_mutex(mutex) { lock(); }

  ~ReadScopedLockImpl() { unlock(); }

  void lock() {
    if (!_locked) {
      m_mutex.rdlock();
      _locked = true;
    }
  }

  void unlock() {
    if (_locked) {
      m_mutex.unlock();
      _locked = false;
    }
  }

 private:
  T&   m_mutex;
  bool _locked = false;
};

/**
 * @brief 通过 RAII 机制实现的写锁
 *
 * @tparam T
 */
template <typename T>
class WriteScopedLockImpl {
 public:
  WriteScopedLockImpl(T& mutex) : m_mutex(mutex) { lock(); }

  ~WriteScopedLockImpl() { unlock(); }

  void lock() {
    if (!_locked) {
      m_mutex.wrlock();
      _locked = true;
    }
  }

  void unlock() {
    if (_locked) {
      m_mutex.unlock();
      _locked = false;
    }
  }

 private:
  T&   m_mutex;
  bool _locked = false;
};

class Mutex : public NonCopyable {
 public:
  using Lock = ScopedLockImpl<Mutex>;

  Mutex() { pthread_mutex_init(&m_mutex, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&m_mutex); }

  void lock() { pthread_mutex_lock(&m_mutex); }
  void unlock() { pthread_mutex_unlock(&m_mutex); }

 private:
  pthread_mutex_t m_mutex;
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
  using Lock = ScopedLockImpl<Spinlock>;
  Spinlock() { pthread_spin_init(&m_mutex, 0); }
  ~Spinlock() { pthread_spin_destroy(&m_mutex); }

  void lock() { pthread_spin_lock(&m_mutex); }
  void unlock() { pthread_spin_unlock(&m_mutex); }

 private:
  pthread_spinlock_t m_mutex;
};

class CASLock : public NonCopyable {
 public:
  typedef ScopedLockImpl<CASLock> Lock;
  CASLock() { m_mutex.clear(); }
  ~CASLock() {}

  void lock() {
    while (std::atomic_flag_test_and_set_explicit(&m_mutex,
                                                  std::memory_order_acquire))
      ;
  }

  void unlock() {
    std::atomic_flag_clear_explicit(&m_mutex, std::memory_order_release);
  }

 private:
  volatile std::atomic_flag m_mutex;
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
  pid_t getId() const { return _id; }

  const std::string& getName() const { return m_name; }

  /**
   * @brief 等待线程结束
   *
   */
  void join();

  /**
   * @brief 获取当前运行的线程
   *
   * @return Thread*
   */
  static Thread* GetThis();

  /**
   * @brief 获取当前运行的线程的名称
   *
   * @return Thread*
   */
  static const std::string& GetName();

  static void SetName(const std::string& name);

 private:
  static void* run(void* args);

 private:
  pid_t       _id     = -1;  // 全局 ID，通过 syscall() 得到
  pthread_t   m_thread = 0;   // 进程内 ID，pthread_create() 创建时得到
  std::string m_name;         // 线程名称

  std::function<void()> m_cb;  // 线程内运行的函数

  Semaphore _semaphore;
};

}  // namespace gudov

#endif  // __GUDOV_THREAD_H__
