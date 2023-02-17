#include "thread.h"

#include <semaphore.h>

#include "log.h"
#include "util.h"

namespace gudov {

// 当前运行的线程
static thread_local Thread* t_thread = nullptr;
// 当前运行的线程的名称
static thread_local std::string t_threadName = "UNKNOWN";

static Logger::ptr g_logger = LOG_NAME("system");

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
    t_thread->m_name = name;
  }
  t_threadName = name;
}

Thread::Thread(std::function<void()> callback, const std::string& name)
    : m_name(name), m_callback(callback) {
  if (name.empty()) {
    m_name = "UNKNOWN";
  }
  int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
  if (rt) {
    LOG_ERROR(g_logger)
        << "pthread_create thread fail, rt=" << rt << " name=" << name;
    throw std::logic_error("pthread_create error");
  }

  // 等待线程创建完毕 (运行到 notify()) 后执行函数体
  _semaphore.wait();
}

Thread::~Thread() {
  if (m_thread) {
    pthread_detach(m_thread);
  }
}

void Thread::join() {
  if (m_thread) {
    int rt = pthread_join(m_thread, nullptr);
    if (rt) {
      LOG_ERROR(g_logger)
          << "pthread_join thread fail, rt=" << rt << " name=" << m_name;
      throw std::logic_error("pthread_join error");
    }
    m_thread = 0;
  }
}

void* Thread::run(void* arg) {
  Thread* thread = (Thread*)arg;
  t_thread       = thread;
  t_threadName   = thread->m_name;
  thread->m_id    = GetThreadId();

  // 设置线程名
  pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

  std::function<void()> callback;
  callback.swap(thread->m_callback);

  // 线程创建成功，准备执行
  thread->_semaphore.notify();

  callback();
  return 0;
}

}  // namespace gudov
