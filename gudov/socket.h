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
#pragma once

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
  Socket::ptr Accept();

  bool Bind(const Address::ptr addr);
  bool Connect(const Address::ptr addr, uint64_t timeoutMs = -1);
  bool Listen(int backLog = SOMAXCONN);
  bool Close();

  int Send(const void* buffer, size_t length, int flags = 0);
  int Send(const iovec* buffers, size_t length, int flags = 0);
  int SendTo(const void* buffer, size_t length, const Address::ptr to, int flags = 0);
  int SendTo(const iovec* buffers, size_t length, const Address::ptr to, int flags = 0);
  int Recv(void* buffer, size_t length, int flags = 0);
  int Recv(iovec* buffers, size_t length, int flags = 0);
  int RecvFrom(void* buffer, size_t length, Address::ptr from, int flags = 0);
  int RecvFrom(iovec* buffers, size_t length, Address::ptr from, int flags = 0);

  int64_t GetSendTimeout();
  void    SetSendTimeout(int64_t v);

  int64_t GetRecvTimeout();
  void    SetRecvTimeout(int64_t v);

  bool GetOption(int level, int option, void* result, socklen_t* len);

  template <class T>
  bool GetOption(int level, int option, T& result) {
    socklen_t length = sizeof(T);
    return GetOption(level, option, &result, &length);
  }

  bool SetOption(int level, int option, const void* value, socklen_t len);

  template <class T>
  bool SetOption(int level, int option, T& value) {
    return SetOption(level, option, &value, sizeof(T));
  }

  /**
   * @brief 获取本地地址
   *
   * @return Address::ptr
   */
  Address::ptr GetRemoteAddress();

  /**
   * @brief 获取远端地址
   *
   * @return Address::ptr
   */
  Address::ptr GetLocalAddress();

  int GetFamily() const { return family_; }
  int GetType() const { return type_; }
  int GetProtocol() const { return protocol_; }

  bool IsConnected() const { return is_connected_; }
  bool IsValid() const;
  int  GetError();

  std::ostream& Dump(std::ostream& os) const;

  int GetSocket() const { return sock_; }

  bool CancelRead();
  bool CancelWrite();
  bool CancelAccept();
  bool CancelAll();

 private:
  /**
   * @brief 对 socket 进行初始化设置
   * @details 设置地址复用，并禁用 Nagle 算法，允许小包的发送
   *
   */
  void InitSock();

  /**
   * @brief 使用 socket() 函数新建 socket 并初始化
   *
   */
  void NewSock();

  /**
   * @brief 对裸 socket 进行初始化
   * @details 该函数在 accept() 中被调用
   *
   * @param sock 待初始化的 fd
   * @return true 初始化成功
   * @return false 初始化失败
   */
  bool Init(int sock);

 private:
  int  sock_;
  int  family_;
  int  type_;
  int  protocol_;
  bool is_connected_;

  Address::ptr local_address_;
  Address::ptr remote_address_;
};

std::ostream& operator<<(std::ostream& os, const Socket& addr);

}  // namespace gudov
