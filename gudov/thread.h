#ifndef __GUDOV_THREAD_H__
#define __GUDOV_THREAD_H__

#include <pthread.h>

#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace gudov {

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
};

}  // namespace gudov

#endif  // __GUDOV_THREAD_H__
