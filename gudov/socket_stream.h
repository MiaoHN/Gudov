#ifndef __GUDOV_SOCKET_STREAM_H__
#define __GUDOV_SOCKET_STREAM_H__

#include "socket.h"
#include "stream.h"

namespace gudov {

class SocketStream : public Stream {
 public:
  using ptr = std::shared_ptr<SocketStream>;

  SocketStream(Socket::ptr sock, bool owner = true);
  ~SocketStream();

  int  read(void* buffer, size_t length) override;
  int  read(ByteArray::ptr ba, size_t length) override;
  int  write(const void* buffer, size_t length) override;
  int  write(ByteArray::ptr ba, size_t length) override;
  void close() override;

  Socket::ptr getSocket() const { return m_socket; }
  bool        isConnected() const;

 protected:
  Socket::ptr m_socket;
  bool        m_owner;
};

}  // namespace gudov

#endif  // __GUDOV_SOCKET_STREAM_H__