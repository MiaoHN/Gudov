#include "gudov/hook.h"
#include "gudov/iomanager.h"
#include "gudov/log.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void testSleep() {
  gudov::IOManager iom(1);

  iom.schedule([]() {
    sleep(2);
    GUDOV_LOG_INFO(g_logger) << "sleep 2";
  });

  iom.schedule([]() {
    sleep(3);
    GUDOV_LOG_INFO(g_logger) << "sleep 3";
  });

  GUDOV_LOG_INFO(g_logger) << "testSleep";
}

int main(int argc, char const *argv[]) {
  testSleep();
  return 0;
}
