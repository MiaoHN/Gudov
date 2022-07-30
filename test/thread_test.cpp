#include <unistd.h>

#include <string>
#include <vector>

#include "gudov/gudov.h"
#include "gudov/log.h"
#include "gudov/util.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void fun1() {
  GUDOV_LOG_INFO(g_logger) << "name: " << gudov::Thread::GetName()
                           << " this.name: "
                           << gudov::Thread::GetThis()->getName()
                           << " id: " << gudov::GetThreadId()
                           << " this.id: " << gudov::Thread::GetThis()->getId();
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
  return 0;
}
