#include <cassert>

#include "gudov/gudov.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void testAssert() {
  GUDOV_LOG_INFO(g_logger) << gudov::BacktraceToString(10);
  GUDOV_ASSERT2(0 == 1, "abcdef xx");
}

int main(int argc, char *argv[]) {
  testAssert();
  return 0;
}
