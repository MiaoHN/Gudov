#ifndef __GUDOV_UTIL_H__
#define __GUDOV_UTIL_H__

#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>

namespace gudov {

pid_t GetThreadId();
uint32_t GetFiberId();
  
} // namespace gudov


#endif  // __GUDOV_UTIL_H__
