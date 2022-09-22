#include "thread.h"

#include <semaphore.h>

#include "log.h"
#include "util.h"

namespace gudov {

static thread_local Thread*     t_thread     = nullptr;
static thread_local std::string t_threadName = "UNKNOWN";

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

Semaphore::Semaphore(uint32_t count) {
  if (sem_init(&semaphore_, 0, count)) {
    throw std::logic_error("sem_init error");
  }
}

Semaphore::~Semaphore() { sem_destroy(&semaphore_); }

void Semaphore::wait() {
  if (sem_wait(&semaphore_)) {
    throw std::logic_error("sem_wait error");
  }
}

void Semaphore::notify() {
  if (sem_post(&semaphore_)) {
    throw std::logic_error("sem_post error");
  }
}

Thread* Thread::GetThis() { return t_thread; }

const std::string& Thread::GetName() { return t_threadName; }

void Thread::SetName(const std::string& name) {
  if (t_thread) {
    t_thread->name_ = name;
  }
  t_threadName = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    : cb_(cb), name_(name) {
  if (name.empty()) {
    name_ = "UNKNOWN";
  }
  int rt = pthread_create(&thread_, nullptr, &Thread::run, this);
  if (rt) {
    GUDOV_LOG_ERROR(g_logger)
        << "pthread_create thread fail, rt=" << rt << " name=" << name;
    throw std::logic_error("pthread_create error");
  }
  semaphore_.wait();
}

Thread::~Thread() {
  if (thread_) {
    pthread_detach(thread_);
  }
}

void Thread::join() {
  if (thread_) {
    int rt = pthread_join(thread_, nullptr);
    if (rt) {
      GUDOV_LOG_ERROR(g_logger)
          << "pthread_join thread fail, rt=" << rt << " name=" << name_;
      throw std::logic_error("pthread_join error");
    }
    thread_ = 0;
  }
}

void* Thread::run(void* arg) {
  Thread* thread = (Thread*)arg;
  t_thread       = thread;
  t_threadName   = thread->name_;
  thread->id_    = GetThreadId();
  pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());

  std::function<void()> cb;
  cb.swap(thread->cb_);

  thread->semaphore_.notify();

  cb();
  return 0;
}

}  // namespace gudov
