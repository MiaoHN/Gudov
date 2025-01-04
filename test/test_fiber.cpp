#include "gudov/gudov.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

void runInFiber() {
  LOG_INFO(g_logger) << "run_in_fiber begin";
  gudov::Fiber::Yield();
  LOG_INFO(g_logger) << "run_in_fiber end";
  gudov::Fiber::Yield();
}

void testFiber() {
  LOG_INFO(g_logger) << "main begin -1";
  {
    gudov::Fiber::GetThis();
    LOG_INFO(g_logger) << "main begin";
    gudov::Fiber::ptr fiber(new gudov::Fiber(runInFiber));
    fiber->swapIn();
    LOG_INFO(g_logger) << "main after swapIn";
    fiber->swapIn();
    LOG_INFO(g_logger) << "main after end";
    fiber->swapIn();
  }
  LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char *argv[]) {
  gudov::Thread::SetName("main");

  std::vector<gudov::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(gudov::Thread::ptr(new gudov::Thread(&testFiber, "name_" + std::to_string(i))));
  }

  for (auto i : threads) {
    i->join();
  }
  return 0;
}
