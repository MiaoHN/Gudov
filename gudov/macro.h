#pragma once

#include <cassert>
#include <cstring>

#include "util.h"

#if defined __GNUC__ || defined __llvm__
#define GUDOV_LICKLY(x)   __builtin_expect(!!(x), 1)
#define GUDOV_UNLICKLY(x) __builtin_expect(!!(x), 0)
#else
#define GUDOV_LICKLY(x)   (x)
#define GUDOV_UNLICKLY(x) (x)
#endif

#define GUDOV_ASSERT(x)                                                                                        \
  if (GUDOV_UNLICKLY(!(x))) {                                                                                  \
    LOG_ERROR(LOG_ROOT()) << "ASSERTION: " #x << "\nbacktrace:\n" << gudov::BacktraceToString(100, 2, "    "); \
    assert(x);                                                                                                 \
  }

#define GUDOV_ASSERT2(x, w)                                            \
  if (GUDOV_UNLICKLY(!(x))) {                                          \
    LOG_ERROR(LOG_ROOT()) << "ASSERTION: " #x << "\n"                  \
                          << w << "\nbacktrace:\n"                     \
                          << gudov::BacktraceToString(100, 2, "    "); \
    assert(x);                                                         \
  }
