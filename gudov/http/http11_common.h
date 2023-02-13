#ifndef __GUDOV_HTTP11_COMMON_H__
#define __GUDOV_HTTP11_COMMON_H__

#include <sys/types.h>

typedef void (*element_cb)(void* data, const char* at, size_t length);
typedef void (*field_cb)(void* data, const char* field, size_t flen,
                         const char* value, size_t vlen);

#endif  // __GUDOV_HTTP11_COMMON_H__