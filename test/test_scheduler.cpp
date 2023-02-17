#include "gudov/gudov.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

void testFiber() {
  static int s_count = 5;
  LOG_INFO(g_logger) << "test in fiber s_count=" << s_count;

  sleep(1);

  if (--s_count >= 0) {
    gudov::Scheduler::GetThis()->schedule(&testFiber, gudov::GetThreadId());
  }
}

int main(int argc, char *argv[]) {
  LOG_INFO(g_logger) << "main";
  gudov::Scheduler sc(3, false, "test");
  sc.start();
  sleep(2);
  LOG_INFO(g_logger) << "schedule";
  sc.schedule(&testFiber);
  sc.stop();
  LOG_INFO(g_logger) << "over";
  return 0;
}
