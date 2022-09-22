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
  Semaphore(const Semaphore&)  = delete;
  Semaphore(const Semaphore&&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

 private:
  sem_t _semaphore;
};

template <typename T>
class ScopedLockImpl {
 public:
  ScopedLockImpl(T& mutex) : _mutex(mutex) { lock(); }

  ~ScopedLockImpl() { unlock(); }

  void lock() {
    if (!_locked) {
      _mutex.lock();
      _locked = true;
    }
  }

  void unlock() {
    if (_locked) {
      _mutex.unlock();
      _locked = false;
    }
  }

 private:
  T&   _mutex;
  bool _locked = false;
};

template <typename T>
class ReadScopedLockImpl {
 public:
  ReadScopedLockImpl(T& mutex) : _mutex(mutex) { lock(); }

  ~ReadScopedLockImpl() { unlock(); }

  void lock() {
    if (!_locked) {
      _mutex.rdlock();
      _locked = true;
    }
  }

  void unlock() {
    if (_locked) {
      _mutex.unlock();
      _locked = false;
    }
  }

 private:
  T&   _mutex;
  bool _locked = false;
};

template <typename T>
class WriteScopedLockImpl {
 public:
  WriteScopedLockImpl(T& mutex) : _mutex(mutex) { lock(); }

  ~WriteScopedLockImpl() { unlock(); }

  void lock() {
    if (!_locked) {
      _mutex.wrlock();
      _locked = true;
    }
  }

  void unlock() {
    if (_locked) {
      _mutex.unlock();
      _locked = false;
    }
  }

 private:
  T&   _mutex;
  bool _locked = false;
};

class Mutex {
 public:
  using Lock = ScopedLockImpl<Mutex>;

  Mutex() { pthread_mutex_init(&_mutex, nullptr); }
  ~Mutex() { pthread_mutex_destroy(&_mutex); }

  void lock() { pthread_mutex_lock(&_mutex); }
  void unlock() { pthread_mutex_unlock(&_mutex); }

 private:
  pthread_mutex_t _mutex;
};

class RWMutex {
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

class Spinlock {
 public:
  using Lock = ScopedLockImpl<Spinlock>;
  Spinlock() { pthread_spin_init(&_mutex, 0); }
  ~Spinlock() { pthread_spin_destroy(&_mutex); }

  void lock() { pthread_spin_lock(&_mutex); }
  void unlock() { pthread_spin_unlock(&_mutex); }

 private:
  pthread_spinlock_t _mutex;
};

class CASLock {
 public:
  typedef ScopedLockImpl<CASLock> Lock;
  CASLock() { _mutex.clear(); }
  ~CASLock() {}

  void lock() {
    while (std::atomic_flag_test_and_set_explicit(&_mutex,
                                                  std::memory_order_acquire))
      ;
  }

  void unlock() {
    std::atomic_flag_clear_explicit(&_mutex, std::memory_order_release);
  }

 private:
  volatile std::atomic_flag _mutex;
};

class Thread {
 public:
  using ptr = std::shared_ptr<Thread>;
  Thread(std::function<void()> cb, const std::string& name);
  ~Thread();

  pid_t              getId() const { return _id; }
  const std::string& getName() const { return _name; }

  void join();

  static Thread*            GetThis();
  static const std::string& GetName();
  static void               SetName(const std::string& name);

 private:
  Thread(const Thread&)  = delete;
  Thread(const Thread&&) = delete;
  Thread& operator=(const Thread&) = delete;

  static void* run(void* args);

 private:
  pid_t                 _id     = -1;
  pthread_t             _thread = 0;
  std::function<void()> _cb;
  std::string           _name;

  Semaphore _semaphore;
};

}  // namespace gudov

#endif  // __GUDOV_THREAD_H__
