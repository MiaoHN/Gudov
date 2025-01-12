#pragma once

#include "socket.h"
#include "stream.h"

namespace gudov {

class SocketStream : public Stream {
 public:
  using ptr = std::shared_ptr<SocketStream>;

  SocketStream(Socket::ptr sock, bool owner = true);
  virtual ~SocketStream();

  int  read(void* buffer, size_t length) override;
  int  read(ByteArray::ptr ba, size_t length) override;
  int  write(const void* buffer, size_t length) override;
  int  write(ByteArray::ptr ba, size_t length) override;
  void Close() override;

  Socket::ptr GetSocket() const { return sock_; }
  bool        IsConnected() const;

 protected:
  Socket::ptr sock_;
  bool        owner_;
};

}  // namespace gudov
