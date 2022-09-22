#include "thread.h"

#include <semaphore.h>

#include "log.h"
#include "util.h"

namespace gudov {

static thread_local Thread*     t_thread     = nullptr;
static thread_local std::string t_threadName = "UNKNOWN";

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

Semaphore::Semaphore(uint32_t count) {
  if (sem_init(&_semaphore, 0, count)) {
    throw std::logic_error("sem_init error");
  }
}

Semaphore::~Semaphore() { sem_destroy(&_semaphore); }

void Semaphore::wait() {
  if (sem_wait(&_semaphore)) {
    throw std::logic_error("sem_wait error");
  }
}

void Semaphore::notify() {
  if (sem_post(&_semaphore)) {
    throw std::logic_error("sem_post error");
  }
}

Thread* Thread::GetThis() { return t_thread; }

const std::string& Thread::GetName() { return t_threadName; }

void Thread::SetName(const std::string& name) {
  if (t_thread) {
    t_thread->_name = name;
  }
  t_threadName = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    : _cb(cb), _name(name) {
  if (name.empty()) {
    _name = "UNKNOWN";
  }
  int rt = pthread_create(&_thread, nullptr, &Thread::run, this);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "pthread_create thread fail, rt=" << rt << " name=" << name;
    throw std::logic_error("pthread_create error");
  }
  _semaphore.wait();
}

Thread::~Thread() {
  if (_thread) {
    pthread_detach(_thread);
  }
}

void Thread::join() {
  if (_thread) {
    int rt = pthread_join(_thread, nullptr);
    if (rt) {
      GUDOV_LOG_ERROR(g_logger)
          << "pthread_join thread fail, rt=" << rt << " name=" << _name;
      throw std::logic_error("pthread_join error");
    }
    _thread = 0;
  }
}

void* Thread::run(void* arg) {
  Thread* thread = (Thread*)arg;
  t_thread       = thread;
  t_threadName   = thread->_name;
  thread->_id    = GetThreadId();
  pthread_setname_np(pthread_self(), thread->_name.substr(0, 15).c_str());

  std::function<void()> cb;
  cb.swap(thread->_cb);

  thread->_semaphore.notify();

  cb();
  return 0;
}

}  // namespace gudov
