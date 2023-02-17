#ifndef __GUDOV_ADDRESS_H__
#define __GUDOV_ADDRESS_H__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gudov {

class IPAddress;

/**
 * @brief 地址抽象
 *
 */
class Address {
 public:
  using ptr = std::shared_ptr<Address>;

  /**
   * @brief 通过 `sockaddr` 创建一个 Address::ptr
   *
   * @param[in] addr 地址的指针
   * @param[in] addrlen 地址长度
   * @return Address::ptr
   */
  static Address::ptr Create(const sockaddr* addr, socklen_t addrlen);

  /**
   * @brief 根据主机名查询所有可用地址
   *
   * @param[out] result 记录可用地址的列表
   * @param[in] host 主机名
   * @param[in] family 地址族(IPv4/IPv6)
   * @param[in] type socket 类型
   * @param[in] protocol 协议类型
   * @return true 查询成功
   * @return false 查询失败
   */
  static bool Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family = AF_INET, int type = 0, int protocol = 0);

  /**
   * @brief 根据主机名得到任意一个可用地址
   *
   * @param[in] host 主机名
   * @param[in] family 地址族(IPv4/IPv6)
   * @param[in] type socket 类型
   * @param[in] protocol 协议类型
   * @return Address::ptr 可用地址
   */
  static Address::ptr LookupAny(const std::string& host, int family = AF_INET,
                                int type = 0, int protocol = 0);

  /**
   * @brief 根据 host 获取任意一个可用的 IP 地址
   *
   * @param[in] host 主机名
   * @param[in] family 地址族(IPv4/IPv6)
   * @param[in] type socket 类型
   * @param[in] protocol 协议类型
   * @return std::shared_ptr<IPAddress> 可用 IP 地址
   */
  static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                                                       int family   = AF_INET,
                                                       int type     = 0,
                                                       int protocol = 0);

  static bool GetInterfaceAddress(
      std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
      int family = AF_INET);
  static bool GetInterfaceAddress(
      std::vector<std::pair<Address::ptr, uint32_t>>& result,
      const std::string& iface, int family = AF_INET);

  virtual ~Address() {}

  /**
   * @brief 获取地址类型(IPv4/IPv6)
   *
   * @return int AF_INET/AF_INET6
   */
  int getFamily() const;

  virtual const sockaddr* getAddr() const    = 0;
  virtual sockaddr*       getAddr()          = 0;
  virtual socklen_t       getAddrLen() const = 0;

  virtual std::ostream& insert(std::ostream& os) const = 0;

  std::string toString() const;

  bool operator<(const Address& rhs) const;
  bool operator==(const Address& rhs) const;
  bool operator!=(const Address& rhs) const;
};

class IPAddress : public Address {
 public:
  using ptr = std::shared_ptr<IPAddress>;

  static IPAddress::ptr Create(const char* address, uint16_t port = 0);

  virtual IPAddress::ptr broadcastAddress(uint32_t prefixLen) = 0;
  virtual IPAddress::ptr networkAddress(uint32_t prefixLen)   = 0;
  virtual IPAddress::ptr subnetAddress(uint32_t prefixLen)    = 0;

  virtual uint32_t getPort() const     = 0;
  virtual void     setPort(uint16_t v) = 0;
};

class IPv4Address : public IPAddress {
 public:
  using ptr = std::shared_ptr<IPv4Address>;

  static IPv4Address::ptr Create(const char* address, uint16_t port = 0);

  IPv4Address(const sockaddr_in& address);
  IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

  const sockaddr* getAddr() const override;
  sockaddr*       getAddr() override;
  socklen_t       getAddrLen() const override;
  std::ostream&   insert(std::ostream& os) const override;

  IPAddress::ptr broadcastAddress(uint32_t prefixLen) override;
  IPAddress::ptr networkAddress(uint32_t prefixLen) override;
  IPAddress::ptr subnetAddress(uint32_t prefixLen) override;

  uint32_t getPort() const override;
  void     setPort(uint16_t v) override;

 private:
  sockaddr_in _addr;
};

class IPv6Address : public IPAddress {
 public:
  using ptr = std::shared_ptr<IPv6Address>;

  static IPv6Address::ptr Create(const char* address, uint16_t port = 0);

  IPv6Address();
  IPv6Address(const sockaddr_in6& address);
  IPv6Address(const uint8_t address[16], uint16_t port = 0);

  const sockaddr* getAddr() const override;
  sockaddr*       getAddr() override;
  socklen_t       getAddrLen() const override;
  std::ostream&   insert(std::ostream& os) const override;

  IPAddress::ptr broadcastAddress(uint32_t prefixLen) override;
  IPAddress::ptr networkAddress(uint32_t prefixLen) override;
  IPAddress::ptr subnetAddress(uint32_t prefixLen) override;

  uint32_t getPort() const override;
  void     setPort(uint16_t v) override;

 private:
  sockaddr_in6 _addr;
};

class UnixAddress : public Address {
 public:
  using ptr = std::shared_ptr<UnixAddress>;

  UnixAddress();
  UnixAddress(const std::string& path);

  const sockaddr* getAddr() const override;
  sockaddr*       getAddr() override;
  socklen_t       getAddrLen() const override;
  void            setAddrLen(uint32_t v);
  std::ostream&   insert(std::ostream& os) const override;

 private:
  sockaddr_un _addr;
  socklen_t   _length;
};

class UnknownAddress : public Address {
 public:
  using ptr = std::shared_ptr<UnknownAddress>;

  UnknownAddress(int family);
  UnknownAddress(const sockaddr& addr);

  const sockaddr* getAddr() const override;
  sockaddr*       getAddr() override;
  socklen_t       getAddrLen() const override;
  std::ostream&   insert(std::ostream& os) const override;

 private:
  sockaddr _addr;
};

std::ostream& operator<<(std::ostream& os, const Address& addr);

}  // namespace gudov

#endif  // __GUDOV_ADDRESS_H__
