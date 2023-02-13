#include "socket.h"

#include <limits.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "fdmanager.h"
#include "hook.h"
#include "iomanager.h"
#include "log.h"
#include "macro.h"

namespace gudov {

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

Socket::ptr Socket::CreateTCP(Address::ptr address) {
  Socket::ptr sock(new Socket(address->getFamily(), TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDP(Address::ptr address) {
  Socket::ptr sock(new Socket(address->getFamily(), UDP, 0));
  return sock;
}

Socket::ptr Socket::CreateTCPSocket() {
  Socket::ptr sock(new Socket(IPv4, TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDPSocket() {
  Socket::ptr sock(new Socket(IPv4, UDP, 0));
  return sock;
}

Socket::ptr Socket::CreateTCPSocket6() {
  Socket::ptr sock(new Socket(IPv6, TCP, 0));
  return sock;
}

Socket::ptr Socket::CreateUDPSocket6() {
  Socket::ptr sock(new Socket(IPv6, UDP, 0));
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
    : _sock(-1),
      _family(family),
      _type(type),
      _protocol(protocol),
      _isConnected(false) {}

Socket::~Socket() { close(); }

int64_t Socket::getSendTimeout() {
  FdContext::ptr ctx = FdMgr::getInstance()->get(_sock);
  if (ctx) {
    return ctx->getTimeout(SO_SNDTIMEO);
  }
  return -1;
}

void Socket::setSendTimeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  setOption(SOL_SOCKET, SO_SNDTIMEO, tv);
}

int64_t Socket::getRecvTimeout() {
  FdContext::ptr ctx = FdMgr::getInstance()->get(_sock);
  if (ctx) {
    return ctx->getTimeout(SO_RCVTIMEO);
  }
  return -1;
}

void Socket::setRecvTimeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  setOption(SOL_SOCKET, SO_RCVTIMEO, tv);
}

bool Socket::getOption(int level, int option, void* result, size_t* len) {
  int rt = getsockopt(_sock, level, option, result, (socklen_t*)len);
  if (rt) {
    GUDOV_LOG_DEBUG(g_logger)
        << "getOption sock=" << _sock << " level=" << level
        << " option=" << option << " errno=" << errno
        << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::setOption(int level, int option, const void* value, size_t len) {
  if (setsockopt(_sock, level, option, value, (socklen_t)len)) {
    GUDOV_LOG_DEBUG(g_logger)
        << "setOption sock=" << _sock << " level=" << level
        << " option=" << option << " errno=" << errno
        << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

Socket::ptr Socket::accept() {
  Socket::ptr sock(new Socket(_family, _type, _protocol));
  int         newSock = ::accept(_sock, nullptr, nullptr);
  if (newSock == -1) {
    GUDOV_LOG_ERROR(g_logger) << "accept(" << _sock << ") errno=" << errno
                              << " errstr=" << strerror(errno);
    return nullptr;
  }
  if (sock->init(newSock)) {
    return sock;
  }
  return nullptr;
}

bool Socket::bind(const Address::ptr addr) {
  if (!isValid()) {
    newSock();
    if (GUDOV_UNLICKLY(!isValid())) {
      return false;
    }
  }

  if (GUDOV_UNLICKLY(addr->getFamily() != _family)) {
    GUDOV_LOG_ERROR(g_logger)
        << "bind sock.family(" << _family << ") addr.family("
        << addr->getFamily() << ") not equal, addr=" << addr->toString();
    return false;
  }

  if (::bind(_sock, addr->getAddr(), addr->getAddrLen())) {
    GUDOV_LOG_ERROR(g_logger)
        << "bind error errrno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  getLocalAddress();
  return true;
}

bool Socket::connect(const Address::ptr addr, uint64_t timeoutMs) {
  if (!isValid()) {
    newSock();
    if (GUDOV_UNLICKLY(!isValid())) {
      return false;
    }
  }

  if (GUDOV_UNLICKLY(addr->getFamily() != _family)) {
    GUDOV_LOG_ERROR(g_logger)
        << "connect sock.family(" << _family << ") addr.family("
        << addr->getFamily() << ") not equal, addr=" << addr->toString();
    return false;
  }

  if (timeoutMs == (uint64_t)-1) {
    if (::connect(_sock, addr->getAddr(), addr->getAddrLen())) {
      GUDOV_LOG_ERROR(g_logger)
          << "sock=" << _sock << " connect(" << addr->toString()
          << ") error errno=" << errno << " errstr=" << strerror(errno);
      close();
      return false;
    }
  } else {
    if (::connectWithTimeout(_sock, addr->getAddr(), addr->getAddrLen(),
                             timeoutMs)) {
      GUDOV_LOG_ERROR(g_logger)
          << "sock=" << _sock << " connect(" << addr->toString()
          << ") timeout=" << timeoutMs << " error errno=" << errno
          << " errstr=" << strerror(errno);
      close();
      return false;
    }
  }
  _isConnected = true;
  getRemoteAddress();
  getLocalAddress();
  return true;
}

bool Socket::listen(int backLog) {
  if (!isValid()) {
    GUDOV_LOG_ERROR(g_logger) << "listen error sock=-1";
    return false;
  }
  if (::listen(_sock, backLog)) {
    GUDOV_LOG_ERROR(g_logger)
        << "listen error errno=" << errno << " errstr=" << strerror(errno);
    return false;
  }
  return true;
}

bool Socket::close() {
  if (!_isConnected && _sock == -1) {
    return true;
  }
  _isConnected = false;
  if (_sock != -1) {
    ::close(_sock);
    _sock = -1;
  }
  return false;
}

int Socket::send(const void* buffer, size_t length, int flags) {
  if (isConnected()) {
    return ::send(_sock, buffer, length, flags);
  }
  return -1;
}

int Socket::send(const iovec* buffers, size_t length, int flags) {
  if (isConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov    = (iovec*)buffers;
    msg.msg_iovlen = length;
    return ::sendmsg(_sock, &msg, flags);
  }
  return -1;
}

int Socket::sendTo(const void* buffer, size_t length, const Address::ptr to,
                   int flags) {
  if (isConnected()) {
    return ::sendto(_sock, buffer, length, flags, to->getAddr(),
                    to->getAddrLen());
  }
  return -1;
}

int Socket::sendTo(const iovec* buffers, size_t length, const Address::ptr to,
                   int flags) {
  if (isConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov     = (iovec*)buffers;
    msg.msg_iovlen  = length;
    msg.msg_name    = to->getAddr();
    msg.msg_namelen = to->getAddrLen();
    return ::sendmsg(_sock, &msg, flags);
  }
  return -1;
}

int Socket::recv(void* buffer, size_t length, int flags) {
  if (isConnected()) {
    return ::recv(_sock, buffer, length, flags);
  }
  return -1;
}

int Socket::recv(iovec* buffers, size_t length, int flags) {
  if (isConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov    = (iovec*)buffers;
    msg.msg_iovlen = length;
    return ::recvmsg(_sock, &msg, flags);
  }
  return -1;
}

int Socket::recvFrom(void* buffer, size_t length, Address::ptr from,
                     int flags) {
  if (isConnected()) {
    socklen_t len = from->getAddrLen();
    return ::recvfrom(_sock, buffer, length, flags, from->getAddr(), &len);
  }
  return -1;
}

int Socket::recvFrom(iovec* buffers, size_t length, Address::ptr from,
                     int flags) {
  if (isConnected()) {
    msghdr msg;
    memset(&msg, 0, sizeof(msg));
    msg.msg_iov     = (iovec*)buffers;
    msg.msg_iovlen  = length;
    msg.msg_name    = from->getAddr();
    msg.msg_namelen = from->getAddrLen();
    return ::recvmsg(_sock, &msg, flags);
  }
  return -1;
}

Address::ptr Socket::getRemoteAddress() {
  if (_remoteAddress) {
    return _remoteAddress;
  }

  Address::ptr result;
  switch (_family) {
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
      result.reset(new UnknownAddress(_family));
      break;
  }
  socklen_t addrlen = result->getAddrLen();
  if (getpeername(_sock, result->getAddr(), &addrlen)) {
    GUDOV_LOG_ERROR(g_logger)
        << "getpeername error sock=" << _sock << " errno=" << errno
        << " errstr=" << strerror(errno);
    return Address::ptr(new UnknownAddress(_family));
  }
  if (_family == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
    addr->setAddrLen(addrlen);
  }
  _remoteAddress = result;
  return _remoteAddress;
}

Address::ptr Socket::getLocalAddress() {
  if (_localAddress) {
    return _localAddress;
  }

  Address::ptr result;
  switch (_family) {
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
      result.reset(new UnknownAddress(_family));
      break;
  }
  socklen_t addrlen = result->getAddrLen();
  if (getsockname(_sock, result->getAddr(), &addrlen)) {
    GUDOV_LOG_ERROR(g_logger)
        << "getsockname error sock=" << _sock << " errno=" << errno
        << " errstr=" << strerror(errno);
    return Address::ptr(new UnknownAddress(_family));
  }
  if (_family == AF_UNIX) {
    UnixAddress::ptr addr = std::dynamic_pointer_cast<UnixAddress>(result);
    addr->setAddrLen(addrlen);
  }
  _localAddress = result;
  return _localAddress;
}

bool Socket::isValid() const { return _sock != -1; }

int Socket::getError() {
  int    error = 0;
  size_t len   = sizeof(error);
  if (!getOption(SOL_SOCKET, SO_ERROR, &error, &len)) {
    return -1;
  }
  return error;
}

std::ostream& Socket::dump(std::ostream& os) const {
  os << "[Socket sock=" << _sock << " is_connected=" << _isConnected
     << " family=" << _family << " type=" << _type << " protocol=" << _protocol;
  if (_localAddress) {
    os << " local_address=" << _localAddress->toString();
  }
  if (_remoteAddress) {
    os << " remote_address=" << _remoteAddress->toString();
  }
  os << "]";
  return os;
}

bool Socket::cancelRead() {
  return IOManager::GetThis()->cancelEvent(_sock,
                                           gudov::IOManager::Event::READ);
}

bool Socket::cancelWrite() {
  return IOManager::GetThis()->cancelEvent(_sock,
                                           gudov::IOManager::Event::WRITE);
}

bool Socket::cancelAccept() {
  return IOManager::GetThis()->cancelEvent(_sock,
                                           gudov::IOManager::Event::READ);
}

bool Socket::cancelAll() { return IOManager::GetThis()->cancelAll(_sock); }

void Socket::initSock() {
  int val = 1;
  setOption(SOL_SOCKET, SO_REUSEADDR, val);
  if (_type == SOCK_STREAM) {
    setOption(IPPROTO_TCP, TCP_NODELAY, val);
  }
}

void Socket::newSock() {
  _sock = socket(_family, _type, _protocol);
  if (GUDOV_LICKLY(_sock != -1)) {
    initSock();
  } else {
    GUDOV_LOG_ERROR(g_logger)
        << "socket(" << _family << ", " << _type << ", " << _protocol
        << ") errno=" << errno << " errstr=" << strerror(errno);
  }
}

bool Socket::init(int sock) {
  FdContext::ptr ctx = FdMgr::getInstance()->get(sock);
  if (ctx && ctx->isSocket() && !ctx->isClose()) {
    _sock        = sock;
    _isConnected = true;
    initSock();
    getLocalAddress();
    getRemoteAddress();
    return true;
  }
  return false;
}

std::ostream& operator<<(std::ostream& os, const Socket& sock) {
  return sock.dump(os);
}

}  // namespace gudov
