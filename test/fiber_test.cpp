#include "gudov/gudov.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void runInFiber() {
  GUDOV_LOG_INFO(g_logger) << "run_in_fiber begin";
  gudov::Fiber::YieldToHold();
  GUDOV_LOG_INFO(g_logger) << "run_in_fiber end";
  gudov::Fiber::YieldToHold();
}

void testFiber() {
  GUDOV_LOG_INFO(g_logger) << "main begin -1";
  {
    gudov::Fiber::GetThis();
    GUDOV_LOG_INFO(g_logger) << "main begin";
    gudov::Fiber::ptr fiber(new gudov::Fiber(runInFiber));
    fiber->swapIn();
    GUDOV_LOG_INFO(g_logger) << "main after swapIn";
    fiber->swapIn();
    GUDOV_LOG_INFO(g_logger) << "main after end";
    fiber->swapIn();
  }
  GUDOV_LOG_INFO(g_logger) << "main after end2";
}

int main(int argc, char *argv[]) {
  gudov::Thread::SetName("main");

  std::vector<gudov::Thread::ptr> threads;
  for (int i = 0; i < 3; ++i) {
    threads.push_back(gudov::Thread::ptr(
        new gudov::Thread(&testFiber, "name_" + std::to_string(i))));
  }

  for (auto i : threads) {
    i->join();
  }
  return 0;
}
