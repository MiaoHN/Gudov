#include "socket.h"

#include <limits.h>

#include "fdmanager.h"
#include "hook.h"
#include "iomanager.h"
#include "log.h"
#include "macro.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

Socket::ptr Socket::CreateTCP(Address::ptr address) {
  Socket::ptr sock(new Socket(address->GetFamily(), TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDP(Address::ptr address) {
  Socket::ptr sock(new Socket(address->GetFamily(), UDP, 0));
  sock->NewSock();
  sock->is_connected_ = true;
  return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
  Socket::ptr sock(new Socket(IPv4, TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
  Socket::ptr sock(new Socket(IPv4, UDP, 0));
  sock->NewSock();
  sock->is_connected_ = true;
  return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
  Socket::ptr sock(new Socket(IPv6, TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
  Socket::ptr sock(new Socket(IPv6, UDP, 0));
  sock->NewSock();
  sock->is_connected_ = true;
  return sock;
}

Socket::ptr Socket::CreateUnixTCPSocket() {
  Socket::ptr sock(new Socket(UNIX, TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUnixUDPSocket() {
  Socket::ptr sock(new Socket(UNIX, UDP, 0));
  return sock;
}

Socket::Socket(int family, int type, int protocol)
    : sock_(-1), family_(family), type_(type), protocol_(protocol), is_connected_(false) {}

Socket::~Socket() { Close(); }

int64_t Socket::GetSendTimeout() {
  FdContext::ptr ctx = FdMgr::getInstance()->get(sock_);
  if (ctx) {
    return ctx->getTimeout(SO_SNDTIMEO);
  }
  return -1;
}

void Socket::SetSendTimeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  SetOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::GetRecvTimeout() {
  FdContext::ptr ctx = FdMgr::getInstance()->get(sock_);
  if (ctx) {
    return ctx->getTimeout(SO_RCVTIMEO);
  }
  return -1;
}

void Socket::SetRecvTimeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  SetOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::GetOption(int level, int option, void* result, socklen_t* len) {
  int rt = getsockopt(sock_, level, option, result, (socklen_t*)len);
  if (rt) {
    LOG_DEBUG(g_logger) << "GetOption sock=" << sock_ << " level=" << level << " option=" << option
                        << " errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::SetOption(int level, int option, const void* value, socklen_t len) {
  if (setsockopt(sock_, level, option, value, (socklen_t)len)) {
    LOG_DEBUG(g_logger) << "SetOption sock=" << sock_ << " level=" << level << " option=" << option
                        << " errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

Socket::ptr Socket::Accept() {
  Socket::ptr sock(new Socket(family_, type_, protocol_));
  int         new_sock = ::accept(sock_, nullptr, nullptr);
  if (new_sock == -1) {
    LOG_ERROR(g_logger) << "accept(" << sock_ << ") errno=" << errno << " errstr=" << strerror(errno);
    return nullptr;
  }
  if (sock->Init(new_sock)) {
    return sock;
  }
  return nullptr;
}

bool Socket::Bind(const Address::ptr addr) {
  if (!IsValid()) {
    NewSock();
    if (GUDOV_UNLICKLY(!IsValid())) {
      return false;
    }
  }

  if (GUDOV_UNLICKLY(addr->GetFamily() != family_)) {
    LOG_ERROR(g_logger) << "bind sock.family(" << family_ << ") addr.family(" << addr->GetFamily()
                        << ") not equal, addr=" << addr->ToString();
    return false;
  }

  if (::bind(sock_, addr->GetAddr(), addr->GetAddrLen())) {
    LOG_ERROR(g_logger) << "bind error errrno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  GetLocalAddress();
  return true;
}

bool Socket::Connect(const Address::ptr addr, uint64_t timeoutMs) {
  if (!IsValid()) {
    NewSock();
    if (GUDOV_UNLICKLY(!IsValid())) {
      return false;
    }
  }

  if (GUDOV_UNLICKLY(addr->GetFamily() != family_)) {
    LOG_ERROR(g_logger) << "connect sock.family(" << family_ << ") addr.family(" << addr->GetFamily()
                        << ") not equal, addr=" << addr->ToString();
    return false;
  }

  if (timeoutMs == (uint64_t)-1) {
    if (::connect(sock_, addr->GetAddr(), addr->GetAddrLen())) {
      LOG_ERROR(g_logger) << "sock=" << sock_ << " connect(" << addr->ToString() << ") error errno=" << errno
                          << " errstr=" << strerror(errno);
      Close();
      return false;
    }
  } else {
    if (::connectWithTimeout(sock_, addr->GetAddr(), addr->GetAddrLen(), timeoutMs)) {
      LOG_ERROR(g_logger) << "sock=" << sock_ << " connect(" << addr->ToString() << ") timeout=" << timeoutMs
                          << " error errno=" << errno << " errstr=" << strerror(errno);
      Close();
      return false;
    }
  }
  is_connected_ = true;
  GetRemoteAddress();
  GetLocalAddress();
  return true;
}

bool Socket::Listen(int backLog) {
  if (!IsValid()) {
    LOG_ERROR(g_logger) << "listen error sock=-1";
    return false;
  }
  if (::listen(sock_, backLog)) {
    LOG_ERROR(g_logger) << "listen error errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::Close() {
  if (!is_connected_ && sock_ == -1) {
    return true;
  }
  is_connected_ = false;
  if (sock_ != -1) {
    ::close(sock_);
    sock_ = -1;
  }
  return false;
}

int Socket::Send(const void* buffer, size_t length, int flags) {
  if (IsConnected()) {
    return ::send(sock_, buffer, length, flags);
  }
  return -1;
}

int Socket::Send(const iovec* buffers, size_t length, int flags) {
  if (IsConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov    = (iovec*)buffers;
    msg.msg_iovlen = length;
    return ::sendmsg(sock_, &msg, flags);
  }
  return -1;
}

int Socket::SendTo(const void* buffer, size_t length, const Address::ptr to, int flags) {
  if (IsConnected()) {
    return ::sendto(sock_, buffer, length, flags, to->GetAddr(), to->GetAddrLen());
  }
  return -1;
}

int Socket::SendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags) {
  if (IsConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov     = (iovec*)buffers;
    msg.msg_iovlen  = length;
    msg.msg_name    = to->GetAddr();
    msg.msg_namelen = to->GetAddrLen();
    return ::sendmsg(sock_, &msg, flags);
  }
  return -1;
}

int Socket::Recv(void* buffer, size_t length, int flags) {
  if (IsConnected()) {
    return ::recv(sock_, buffer, length, flags);
  }
  return -1;
}

int Socket::Recv(iovec* buffers, size_t length, int flags) {
  if (IsConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov    = (iovec*)buffers;
    msg.msg_iovlen = length;
    return ::recvmsg(sock_, &msg, flags);
  }
  return -1;
}

int Socket::RecvFrom(void* buffer, size_t length, Address::ptr from, int flags) {
  if (IsConnected()) {
    socklen_t len = from->GetAddrLen();
    return ::recvfrom(sock_, buffer, length, flags, from->GetAddr(), &len);
  }
  return -1;
}

int Socket::RecvFrom(iovec* buffers, size_t length, Address::ptr from, int flags) {
  if (IsConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov     = (iovec*)buffers;
    msg.msg_iovlen  = length;
    msg.msg_name    = from->GetAddr();
    msg.msg_namelen = from->GetAddrLen();
    return ::recvmsg(sock_, &msg, flags);
  }
  return -1;
}

Address::ptr Socket::GetRemoteAddress() {
  if (remote_address_) {
    return remote_address_;
  }

  Address::ptr result;
  switch (family_) {
    case AF_INET:
      result.reset(new IPv4Address());
      break;
    case AF_INET6:
      result.reset(new IPv6Address());
      break;
    case AF_UNIX:
      result.reset(new UnixAddress());
      break;
    default:
      result.reset(new UnknownAddress(family_));
      break;
  }
  socklen_t addrlen = result->GetAddrLen();
  if (getpeername(sock_, result->GetAddr(), &addrlen)) {
    LOG_ERROR(g_logger) << "getpeername error sock=" << sock_ << " errno=" << errno << " errstr=" << strerror(errno);
    return Address::ptr(new UnknownAddress(family_));
  }
  if (family_ == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
    addr->setAddrLen(addrlen);
  }
  remote_address_ = result;
  return remote_address_;
}

Address::ptr Socket::GetLocalAddress() {
  if (local_address_) {
    return local_address_;
  }

  Address::ptr result;
  switch (family_) {
    case AF_INET:
      result.reset(new IPv4Address());
      break;
    case AF_INET6:
      result.reset(new IPv6Address());
      break;
    case AF_UNIX:
      result.reset(new UnixAddress());
      break;
    default:
      result.reset(new UnknownAddress(family_));
      break;
  }
  socklen_t addrlen = result->GetAddrLen();
  if (getsockname(sock_, result->GetAddr(), &addrlen)) {
    LOG_ERROR(g_logger) << "getsockname error sock=" << sock_ << " errno=" << errno << " errstr=" << strerror(errno);
    return Address::ptr(new UnknownAddress(family_));
  }
  if (family_ == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
    addr->setAddrLen(addrlen);
  }
  local_address_ = result;
  return local_address_;
}

bool Socket::IsValid() const { return sock_ != -1; }

int Socket::GetError() {
  int       error = 0;
  socklen_t len   = sizeof(error);
  if (!GetOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
    error = errno;
  }
  return error;
}

std::ostream& Socket::Dump(std::ostream& os) const {
  os << "[Socket sock=" << sock_ << " is_connected=" << is_connected_ << " family=" << family_ << " type=" << type_
     << " protocol=" << protocol_;
  if (local_address_) {
    os << " local_address=" << local_address_->ToString();
  }
  if (remote_address_) {
    os << " remote_address=" << remote_address_->ToString();
  }
  os << "]";
  return os;
}

bool Socket::CancelRead() { return IOManager::GetThis()->CancelEvent(sock_, gudov::IOManager::Event::Read); }

bool Socket::CancelWrite() { return IOManager::GetThis()->CancelEvent(sock_, gudov::IOManager::Event::Write); }

bool Socket::CancelAccept() { return IOManager::GetThis()->CancelEvent(sock_, gudov::IOManager::Event::Read); }

bool Socket::CancelAll() { return IOManager::GetThis()->CancelAll(sock_); }

void Socket::InitSock() {
  int val = 1;
  SetOption(SOL_SOCKET, SO_REUSEADDR, val);
  if (type_ == SOCK_STREAM) {
    SetOption(IPPROTO_TCP, TCP_NODELAY, val);
  }
}

void Socket::NewSock() {
  sock_ = socket(family_, type_, protocol_);
  if (GUDOV_LICKLY(sock_ != -1)) {
    InitSock();
  } else {
    LOG_ERROR(g_logger) << "socket(" << family_ << ", " << type_ << ", " << protocol_ << ") errno=" << errno
                        << " errstr=" << strerror(errno);
  }
}

bool Socket::Init(int sock) {
  FdContext::ptr ctx = FdMgr::getInstance()->get(sock);
  if (ctx && ctx->isSocket() && !ctx->isClose()) {
    sock_         = sock;
    is_connected_ = true;
    InitSock();
    GetLocalAddress();
    GetRemoteAddress();
    return true;
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) { return sock.Dump(os); }

}  // namespace gudov
