#include "gudov/gudov.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void testFiber() { GUDOV_LOG_INFO(g_logger) << "test in fiber"; }

int main(int argc, char *argv[]) {
  GUDOV_LOG_INFO(g_logger) << "main";
  gudov::Scheduler sc;
  sc.schedule(&testFiber);
  sc.start();
  GUDOV_LOG_INFO(g_logger) << "schedule";
  sc.stop();
  GUDOV_LOG_INFO(g_logger) << "over";
  return 0;
}
