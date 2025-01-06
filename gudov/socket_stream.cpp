#include "socket_stream.h"

namespace gudov {

SocketStream::SocketStream(Socket::ptr sock, bool owner) : m_socket(sock), m_owner(owner) {}

SocketStream::~SocketStream() {
  if (m_owner && m_socket) {
    m_socket->Close();
  }
}

bool SocketStream::IsConnected() const { return m_socket && m_socket->IsConnected(); }

int SocketStream::read(void* buffer, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  return m_socket->Recv(buffer, length);
}

int SocketStream::read(ByteArray::ptr ba, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  std::vector<iovec> iovs;
  ba->getWriteBuffers(iovs, length);
  int rt = m_socket->Recv(&iovs[0], iovs.size());
  if (rt > 0) {
    ba->setPosition(ba->getPosition() + rt);
  }
  return rt;
}

int SocketStream::write(const void* buffer, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  return m_socket->Send(buffer, length);
}

int SocketStream::write(ByteArray::ptr ba, size_t length) {
  if (!IsConnected()) {
    return -1;
  }
  std::vector<iovec> iovs;
  ba->getReadBuffers(iovs, length);
  int rt = m_socket->Send(&iovs[0], iovs.size());
  if (rt > 0) {
    ba->setPosition(ba->getPosition() + rt);
  }
  return rt;
}

void SocketStream::close() {
  if (m_socket) {
    m_socket->Close();
  }
}

}  // namespace gudov
