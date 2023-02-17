/**
 * @file socket.h
 * @author MiaoHN (582418227@qq.com)
 * @brief socket 封装
 * @version 0.1
 * @date 2023-02-17
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef __GUDOV_SOCKET_H__
#define __GUDOV_SOCKET_H__

#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

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

  int64_t getSendTimeout();
  void    setSendTimeout(int64_t v);

  int64_t getRecvTimeout();
  void    setRecvTimeout(int64_t v);

  bool getOption(int level, int option, void* result, socklen_t* len);

  template <class T>
  bool getOption(int level, int option, T& result) {
    socklen_t length = sizeof(T);
    return getOption(level, option, &result, &length);
  }

  bool setOption(int level, int option, const void* value, socklen_t len);

  template <class T>
  bool setOption(int level, int option, T& value) {
    return setOption(level, option, &value, sizeof(T));
  }

  /**
   * @brief 获取本地地址
   *
   * @return Address::ptr
   */
  Address::ptr getRemoteAddress();

  /**
   * @brief 获取远端地址
   *
   * @return Address::ptr
   */
  Address::ptr getLocalAddress();

  int getFamily() const { return m_family; }
  int getType() const { return m_type; }
  int getProtocol() const { return m_protocol; }

  bool isConnected() const { return m_is_connected; }
  bool isValid() const;
  int  getError();

  std::ostream& dump(std::ostream& os) const;

  int getSocket() const { return m_sock; }

  bool cancelRead();
  bool cancelWrite();
  bool cancelAccept();
  bool cancelAll();

 private:
  /**
   * @brief 对 socket 进行初始化设置
   * @details 设置地址复用，并禁用 Nagle 算法，允许小包的发送
   *
   */
  void initSock();

  /**
   * @brief 使用 socket() 函数新建 socket 并初始化
   *
   */
  void newSock();

  /**
   * @brief 对裸 socket 进行初始化
   * @details 该函数在 accept() 中被调用
   *
   * @param sock 待初始化的 fd
   * @return true 初始化成功
   * @return false 初始化失败
   */
  bool init(int sock);

 private:
  int  m_sock;
  int  m_family;
  int  m_type;
  int  m_protocol;
  bool m_is_connected;

  Address::ptr m_local_address;
  Address::ptr m_remote_address;
};

std::ostream& operator<<(std::ostream& os, const Socket& addr);

}  // namespace gudov

#endif  // __GUDOV_SOCKET_H__