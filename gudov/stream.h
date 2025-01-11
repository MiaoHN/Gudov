#pragma once

#include <memory>

#include "bytearray.h"

namespace gudov {

class Stream {
 public:
  using ptr = std::shared_ptr<Stream>;

  virtual ~Stream() {}

  virtual int read(void* buffer, size_t length)      = 0;
  virtual int read(ByteArray::ptr ba, size_t length) = 0;
  virtual int ReadFixSize(void* buffer, size_t length);
  virtual int ReadFixSize(ByteArray::ptr ba, size_t length);
  virtual int write(const void* buffer, size_t length) = 0;
  virtual int write(ByteArray::ptr ba, size_t length)  = 0;
  virtual int WriteFixSize(const void* buffer, size_t length);
  virtual int WriteFixSize(ByteArray::ptr ba, size_t length);

  virtual void Close() = 0;
};

}  // namespace gudov
