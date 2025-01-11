#include "stream.h"

#include <algorithm>
#include <cstdint>

#include "config.h"
#include "log.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

static ConfigVar<int32_t>::ptr g_socket_buff_size =
    Config::Lookup("socket.buff_size", (int32_t)(1024 * 16), "socket buff size");

int Stream::ReadFixSize(void* buffer, size_t length) {
  size_t               offset  = 0;
  size_t               left    = length;
  static const int64_t MAX_LEN = g_socket_buff_size->GetValue();
  while (left > 0) {
    size_t len = read((char*)buffer + offset, std::min(left, (size_t)MAX_LEN));
    if (len <= 0) {
      LOG_ERROR(g_logger) << "ReadFixSize fail length=" << length << " len=" << len << " errno=" << errno
                          << " errstr=" << strerror(errno);
      return len;
    }
    offset += len;
    left -= len;
  }
  return length;
}

int Stream::ReadFixSize(ByteArray::ptr ba, size_t length) {
  size_t               left    = length;
  static const int64_t MAX_LEN = g_socket_buff_size->GetValue();
  while (left > 0) {
    size_t len = read(ba, std::min(left, (size_t)MAX_LEN));
    if (len <= 0) {
      LOG_ERROR(g_logger) << "ReadFixSize fail length=" << length << " len=" << len << " errno=" << errno
                          << " errstr=" << strerror(errno);
      return len;
    }
    left -= len;
  }
  return length;
}

int Stream::WriteFixSize(const void* buffer, size_t length) {
  size_t               offset  = 0;
  size_t               left    = length;
  static const int64_t MAX_LEN = g_socket_buff_size->GetValue();
  while (left > 0) {
    size_t len = write((const char*)buffer + offset, std::min(left, (size_t)MAX_LEN));
    if (len <= 0) {
      LOG_ERROR(g_logger) << "WriteFixSize fail length=" << length << " len=" << len << " errno=" << errno
                          << " errstr=" << strerror(errno);
      return len;
    }
    offset += len;
    left -= len;
  }
  return length;
}

int Stream::WriteFixSize(ByteArray::ptr ba, size_t length) {
  size_t               left    = length;
  static const int64_t MAX_LEN = g_socket_buff_size->GetValue();
  while (left > 0) {
    size_t len = write(ba, std::min(left, (size_t)MAX_LEN));
    if (len <= 0) {
      LOG_ERROR(g_logger) << "WriteFixSize fail length=" << length << " len=" << len << " errno=" << errno
                          << " errstr=" << strerror(errno);
      return len;
    }
    left -= len;
  }
  return length;
}

}  // namespace gudov
