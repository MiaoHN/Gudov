#include <unistd.h>
#include <yaml-cpp/null.h>

#include "gudov/gudov.h"
#include "gudov/log.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

int          count = 0;
gudov::Mutex s_mutex;

void fun1() {
  LOG_INFO(g_logger) << "name: " << gudov::Thread::GetName()
                           << " this.name: "
                           << gudov::Thread::GetThis()->getName()
                           << " id: " << gudov::GetThreadId()
                           << " this.id: " << gudov::Thread::GetThis()->getId();

  for (int i = 0; i < 100000; ++i) {
    gudov::Mutex::Lock lock(s_mutex);
    ++count;
  }
}

void fun2() {
  while (true) {
    LOG_INFO(g_logger) << "xxxxxxxxxxxxxxxxxxxx";
  }
}

void fun3() {
  while (true) {
    LOG_INFO(g_logger) << "====================";
  }
}

int main(int argc, char *argv[]) {
  LOG_INFO(g_logger) << "thread test begin";
  YAML::Node root = YAML::LoadFile("conf/log2.yml");
  gudov::Config::LoadFromYaml(root);

  std::vector<gudov::Thread::ptr> threads;
  for (int i = 0; i < 1; ++i) {
    gudov::Thread::ptr thread1(
        new gudov::Thread(&fun2, "name_" + std::to_string(i * 2)));
    threads.push_back(thread1);
  }

  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->join();
  }

  LOG_INFO(g_logger) << "thread test end";
  LOG_INFO(g_logger) << "count=" << count;
  return 0;
}
