#pragma once

#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace gudov {

pid_t    GetThreadId();
uint32_t GetFiberId();

void BackTrace(std::vector<std::string>& bt, int size = 64, int skip = 1);

std::string BacktraceToString(int size = 64, int skip = 2, const std::string& prefix = "");

uint64_t GetCurrentMS();

uint64_t GetCurrentUS();

}  // namespace gudov
