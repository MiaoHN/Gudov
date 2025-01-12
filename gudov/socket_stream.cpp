#include "socket_stream.h"

#include <sys/socket.h>

namespace gudov {

SocketStream::SocketStream(Socket::ptr sock, bool owner) : sock_(sock), owner_(owner) {}

SocketStream::~SocketStream() {
  if (owner_) {
    Close();
  }
}

bool SocketStream::IsConnected() const { return sock_ && sock_->IsConnected(); }

int SocketStream::read(void* buffer, size_t length) { return sock_->Recv(buffer, length); }

int SocketStream::read(ByteArray::ptr ba, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  std::vector<iovec> iovs;
  ba->GetWriteBuffers(iovs, length);
  int rt = sock_->Recv(&iovs[0], iovs.size());
  if (rt > 0) {
    ba->SetPosition(ba->GetPosition() + rt);
  }
  return rt;
}

int SocketStream::write(const void* buffer, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  return sock_->Send(buffer, length, MSG_NOSIGNAL);
}

int SocketStream::write(ByteArray::ptr ba, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  std::vector<iovec> iovs;
  ba->GetReadBuffers(iovs, length);
  int rt = sock_->Send(&iovs[0], iovs.size());
  if (rt > 0) {
    ba->SetPosition(ba->GetPosition() + rt);
  }
  return rt;
}

void SocketStream::Close() {
  if (sock_) {
    sock_->Close();
  }
}

}  // namespace gudov
