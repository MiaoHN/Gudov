#ifndef __GUDOV_SOCKET_H__
#define __GUDOV_SOCKET_H__

#include <memory>

#include "address.h"
#include "noncopyable.h"

namespace gudov {

class Socket : public std::enable_shared_from_this<Socket>, NonCopyable {
 public:
  using ptr     = std::shared_ptr<Socket>;
  using Weakptr = std::weak_ptr<Socket>;

  enum Type {
    TCP = SOCK_STREAM,
    UDP = SOCK_DGRAM,
  };

  enum Family {
    IPv4 = AF_INET,
    IPv6 = AF_INET6,
    UNIX = AF_UNIX,
  };

  static Socket::ptr CreateTCP(Address::ptr address);
  static Socket::ptr CreateUDP(Address::ptr address);

  static Socket::ptr CreateTCPSocket();
  static Socket::ptr CreateUDPSocket();

  static Socket::ptr CreateTCPSocket6();
  static Socket::ptr CreateUDPSocket6();

  static Socket::ptr CreateUnixTCPSocket();
  static Socket::ptr CreateUnixUDPSocket();

  Socket(int family, int type, int protocol = 0);
  ~Socket();

  int64_t getSendTimeout();
  void    setSendTimeout(int64_t v);

  int64_t getRecvTimeout();
  void    setRecvTimeout(int64_t v);

  bool getOption(int level, int option, void* result, size_t* len);

  template <class T>
  bool getOption(int level, int option, T& result) {
    size_t length = sizeof(T);
    return getOption(level, option, &result, &length);
  }

  bool setOption(int level, int option, const void* value, size_t len);

  template <class T>
  bool setOption(int level, int option, T& value) {
    return setOption(level, option, &value, sizeof(T));
  }

  Socket::ptr accept();

  bool bind(const Address::ptr addr);
  bool connect(const Address::ptr addr, uint64_t timeoutMs = -1);
  bool listen(int backLog = SOMAXCONN);
  bool close();

  int send(const void* buffer, size_t length, int flags = 0);
  int send(const iovec* buffers, size_t length, int flags = 0);
  int sendTo(const void* buffer, size_t length, const Address::ptr to,
             int flags = 0);
  int sendTo(const iovec* buffers, size_t length, const Address::ptr to,
             int flags = 0);

  int recv(void* buffer, size_t length, int flags = 0);
  int recv(iovec* buffers, size_t length, int flags = 0);
  int recvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
  int recvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

  Address::ptr getRemoteAddress();
  Address::ptr getLocalAddress();

  int getFamily() const { return _family; }
  int getType() const { return _type; }
  int getProtocol() const { return _protocol; }

  bool isConnected() const { return _isConnected; }
  bool isValid() const;
  int  getError();

  std::ostream& dump(std::ostream& os) const;

  int getSocket() const { return _sock; }

  bool cancelRead();
  bool cancelWrite();
  bool cancelAccept();
  bool cancelAll();

 private:
  void initSock();
  void newSock();
  bool init(int sock);

 private:
  int  _sock;
  int  _family;
  int  _type;
  int  _protocol;
  bool _isConnected;

  Address::ptr _localAddress;
  Address::ptr _remoteAddress;
};

std::ostream& operator<<(std::ostream& os, const Socket& addr);

}  // namespace gudov

#endif  // __GUDOV_SOCKET_H__