#ifndef __GUDOV_MACRO_H__
#define __GUDOV_MACRO_H__

#include <cassert>
#include <cstring>

#include "util.h"

#define GUDOV_ASSERT(x)                              \
  if (!(x)) {                                        \
    GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())                \
        << "ASSERTION: " #x << "\nbacktrace:\n"      \
        << gudov::BacktraceToString(100, 2, "    "); \
    assert(x);                                       \
  }

#define GUDOV_ASSERT2(x, w)                          \
  if (!(x)) {                                        \
    GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())                \
        << "ASSERTION: " #x << "\n"                  \
        << w << "\nbacktrace:\n"                     \
        << gudov::BacktraceToString(100, 2, "    "); \
    assert(x);                                       \
  }

#endif  // __GUDOV_MACRO_H__
