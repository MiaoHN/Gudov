#include <unistd.h>

#include "gudov/gudov.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

int count = 0;
gudov::RWMutex s_mutex;

void fun1() {
  GUDOV_LOG_INFO(g_logger) << "name: " << gudov::Thread::GetName()
                           << " this.name: "
                           << gudov::Thread::GetThis()->getName()
                           << " id: " << gudov::GetThreadId()
                           << " this.id: " << gudov::Thread::GetThis()->getId();

  for (int i = 0; i < 100000; ++i) {
    gudov::RWMutex::WriteLock lock(s_mutex);
    ++count;
  }
}

void fun2() {}

int main(int argc, char *argv[]) {
  GUDOV_LOG_INFO(g_logger) << "thread test begin";
  std::vector<gudov::Thread::ptr> threads;
  for (int i = 0; i < 5; ++i) {
    gudov::Thread::ptr thread(
        new gudov::Thread(&fun1, "name_" + std::to_string(i)));
    threads.push_back(thread);
  }

  for (int i = 0; i < 5; ++i) {
    threads[i]->join();
  }

  GUDOV_LOG_INFO(g_logger) << "thread test end";
  GUDOV_LOG_INFO(g_logger) << "count=" << count;
  return 0;
}
