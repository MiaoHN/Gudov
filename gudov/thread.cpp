#include "thread.h"

#include <semaphore.h>

#include "log.h"
#include "util.h"

namespace gudov {

// 当前运行的线程
static thread_local Thread* t_running_thread = nullptr;
// 当前运行的线程的名称
static thread_local std::string t_running_thread_name = "UNKNOWN";

static Logger::ptr g_logger = LOG_NAME("system");

Semaphore::Semaphore(uint32_t count) {
  if (sem_init(&semaphore_, 0, count)) {
    throw std::logic_error("sem_init error");
  }
}

Semaphore::~Semaphore() { sem_destroy(&semaphore_); }

void Semaphore::Wait() {
  if (sem_wait(&semaphore_)) {
    throw std::logic_error("sem_wait error");
  }
}

void Semaphore::Notify() {
  if (sem_post(&semaphore_)) {
    throw std::logic_error("sem_post error");
  }
}

Thread* Thread::GetRunningThread() { return t_running_thread; }

const std::string& Thread::GetRunningThreadName() { return t_running_thread_name; }

void Thread::SetRunningThreadName(const std::string& name) {
  if (t_running_thread) {
    t_running_thread->name_ = name;
  }
  t_running_thread_name = name;
}

Thread::Thread(std::function<void()> callback, const std::string& name) : name_(name), callback_(callback) {
  if (name.empty()) {
    name_ = "UNKNOWN";
  }
  int rt = pthread_create(&thread_, nullptr, &Thread::Run, this);
  if (rt) {
    LOG_ERROR(g_logger) << "pthread_create thread fail, rt=" << rt << " name=" << name;
    throw std::logic_error("pthread_create error");
  }

  // 等待线程创建完毕 (运行到 notify()) 后执行函数体
  semaphore_.Wait();
}

Thread::~Thread() {
  if (thread_) {
    pthread_detach(thread_);
  }
}

void Thread::Join() {
  if (thread_) {
    int rt = pthread_join(thread_, nullptr);
    if (rt) {
      LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt << " name=" << name_;
      throw std::logic_error("pthread_join error");
    }
    thread_ = 0;
  }
}

void* Thread::Run(void* arg) {
  Thread* thread        = (Thread*)arg;
  t_running_thread      = thread;
  t_running_thread_name = thread->name_;
  thread->id_           = GetThreadId();

  // 设置线程名
  pthread_setname_np(pthread_self(), thread->name_.substr(0, 15).c_str());

  std::function<void()> callback;
  callback.swap(thread->callback_);

  // 线程创建成功，准备执行
  thread->semaphore_.Notify();

  callback();
  return 0;
}

}  // namespace gudov
