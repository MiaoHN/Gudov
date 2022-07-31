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

namespace gudov {

class Semaphore {
 public:
  Semaphore(uint32_t count = 0);
  ~Semaphore();

  void wait();
  void notify();

 private:
  Semaphore(const Semaphore&) = delete;
  Semaphore(const Semaphore&&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

 private:
  sem_t semaphore_;
};

template <typename T>
class ScopedLockImpl {
 public:
  ScopedLockImpl(T& mutex) : mutex_(mutex) { lock(); }

  ~ScopedLockImpl() { unlock(); }

  void lock() {
    if (!locked_) {
      mutex_.lock();
      locked_ = true;
    }
  }

  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T& mutex_;
  bool locked_ = false;
};

template <typename T>
class ReadScopedLockImpl {
 public:
  ReadScopedLockImpl(T& mutex) : mutex_(mutex) { lock(); }

  ~ReadScopedLockImpl() { unlock(); }

  void lock() {
    if (!locked_) {
      mutex_.rdlock();
      locked_ = true;
    }
  }

  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T& mutex_;
  bool locked_ = false;
};

template <typename T>
class WriteScopedLockImpl {
 public:
  WriteScopedLockImpl(T& mutex) : mutex_(mutex) { lock(); }

  ~WriteScopedLockImpl() { unlock(); }

  void lock() {
    if (!locked_) {
      mutex_.wrlock();
      locked_ = true;
    }
  }

  void unlock() {
    if (locked_) {
      mutex_.unlock();
      locked_ = false;
    }
  }

 private:
  T& mutex_;
  bool locked_ = false;
};

class Mutex {
 public:
  using Lock = ScopedLockImpl<Mutex>;

  Mutex() { pthread_mutex_init(&mutex_, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&mutex_); }

  void lock() { pthread_mutex_lock(&mutex_); }
  void unlock() { pthread_mutex_unlock(&mutex_); }

 private:
  pthread_mutex_t mutex_;
};

class RWMutex {
 public:
  using ReadLock = ReadScopedLockImpl<RWMutex>;
  using WriteLock = WriteScopedLockImpl<RWMutex>;

  RWMutex() { pthread_rwlock_init(&lock_, nullptr); }
  ~RWMutex() { pthread_rwlock_destroy(&lock_); }

  void rdlock() { pthread_rwlock_rdlock(&lock_); }
  void wrlock() { pthread_rwlock_wrlock(&lock_); }
  void unlock() { pthread_rwlock_unlock(&lock_); }

 private:
  pthread_rwlock_t lock_;
};

class Spinlock {
 public:
  using Lock = ScopedLockImpl<Spinlock>;
  Spinlock() { pthread_spin_init(&mutex_, 0); }
  ~Spinlock() { pthread_spin_destroy(&mutex_); }

  void lock() { pthread_spin_lock(&mutex_); }
  void unlock() { pthread_spin_unlock(&mutex_); }

 private:
  pthread_spinlock_t mutex_;
};

class CASLock {
 public:
  typedef ScopedLockImpl<CASLock> Lock;
  CASLock() { mutex_.clear(); }
  ~CASLock() {}

  void lock() {
    while (std::atomic_flag_test_and_set_explicit(&mutex_,
                                                  std::memory_order_acquire))
      ;
  }

  void unlock() {
    std::atomic_flag_clear_explicit(&mutex_, std::memory_order_release);
  }

 private:
  volatile std::atomic_flag mutex_;
};

class Thread {
 public:
  using ptr = std::shared_ptr<Thread>;
  Thread(std::function<void()> cb, const std::string& name);
  ~Thread();

  pid_t getId() const { return id_; }
  const std::string& getName() const { return name_; }

  void join();

  static Thread* GetThis();
  static const std::string& GetName();
  static void SetName(const std::string& name);

 private:
  Thread(const Thread&) = delete;
  Thread(const Thread&&) = delete;
  Thread& operator=(const Thread&) = delete;

  static void* run(void* args);

 private:
  pid_t id_ = -1;
  pthread_t thread_ = 0;
  std::function<void()> cb_;
  std::string name_;

  Semaphore semaphore_;
};

}  // namespace gudov

#endif  // __GUDOV_THREAD_H__
