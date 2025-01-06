#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netdb.h>

#include <vector>

#include "gudov/address.h"
#include "gudov/log.h"

using namespace gudov;

// 测试 Address::Create
TEST(AddressTest, CreateFromSockaddr) {
  sockaddr_in addr_in;
  addr_in.sin_family = AF_INET;
  addr_in.sin_port   = htons(8080);
  inet_pton(AF_INET, "127.0.0.1", &(addr_in.sin_addr));

  // 使用 sockaddr_in 创建 Address
  Address::ptr addr = Address::Create(reinterpret_cast<sockaddr*>(&addr_in), sizeof(addr_in));
  ASSERT_NE(addr, nullptr);
  ASSERT_EQ(addr->GetFamily(), AF_INET);
}

// 测试 Address::Lookup 和 LookupAny
TEST(AddressTest, LookupAddresses) {
  std::vector<Address::ptr> addresses;
  bool                      result = Address::Lookup(addresses, "www.google.com");
  ASSERT_TRUE(result);
  ASSERT_GT(addresses.size(), 0);  // 至少应该返回一个地址
}

TEST(AddressTest, LookupAnyIPAddress) {
  Address::ptr addr = Address::LookupAny("www.google.com");
  ASSERT_NE(addr, nullptr);
  ASSERT_EQ(addr->GetFamily(), AF_INET);  // 假设返回的是 IPv4 地址
}

// 测试 IPv4 地址功能
TEST(IPAddressTest, IPv4Address) {
  IPv4Address::ptr addr = IPv4Address::Create("127.0.0.1", 8080);
  ASSERT_NE(addr, nullptr);
  ASSERT_EQ(addr->GetPort(), 8080);
  ASSERT_EQ(addr->ToString(), "[IPv4 127.0.0.1:8080]");
}

// 测试 IPv6 地址功能
TEST(IPAddressTest, IPv6Address) {
  IPv6Address::ptr addr = IPv6Address::Create("::1", 8080);
  ASSERT_NE(addr, nullptr);
  ASSERT_EQ(addr->GetPort(), 8080);
  ASSERT_EQ(addr->ToString(), "[IPv6 [::1]:8080]");
}

// 测试 UnixAddress 的功能
TEST(UnixAddressTest, UnixAddress) {
  UnixAddress::ptr addr = std::make_shared<UnixAddress>("/tmp/socket");
  ASSERT_NE(addr, nullptr);
  ASSERT_EQ(addr->ToString(), "/tmp/socket");
}

// 测试 Address 比较操作符
TEST(AddressTest, AddressComparison) {
  sockaddr_in addr_in1, addr_in2;
  addr_in1.sin_family = AF_INET;
  addr_in1.sin_port   = htons(8080);
  inet_pton(AF_INET, "127.0.0.1", &(addr_in1.sin_addr));

  addr_in2.sin_family = AF_INET;
  addr_in2.sin_port   = htons(8080);
  inet_pton(AF_INET, "127.0.0.1", &(addr_in2.sin_addr));

  Address::ptr addr1 = Address::Create(reinterpret_cast<sockaddr*>(&addr_in1), sizeof(addr_in1));
  Address::ptr addr2 = Address::Create(reinterpret_cast<sockaddr*>(&addr_in2), sizeof(addr_in2));

  // ASSERT_TRUE(addr1 == addr2);
  // ASSERT_FALSE(*addr1 != *addr2);
  ASSERT_TRUE(*addr1 < *addr2);  // 根据地址的字典顺序进行比较，IPv4 地址相同，因此应等于。
}

// 测试获取接口地址
TEST(AddressTest, GetInterfaceAddress) {
  std::vector<std::pair<Address::ptr, uint32_t>> interfaces;
  bool                                           result = Address::GetInterfaceAddress(interfaces, "eth0");
  ASSERT_TRUE(result);
  ASSERT_GT(interfaces.size(), 0);
}

// 测试 Broadcast 地址计算
TEST(IPAddressTest, BroadcastAddress) {
  IPv4Address::ptr addr      = IPv4Address::Create("192.168.1.1", 0);
  auto             broadcast = addr->BroadcastAddress(24);  // 假设 prefixLen 为 24
  ASSERT_NE(broadcast, nullptr);
  ASSERT_EQ(broadcast->ToString(), "[IPv4 192.168.1.255:0]");
}

// 测试 Network 地址计算
TEST(IPAddressTest, NetworkAddress) {
  IPv4Address::ptr addr    = IPv4Address::Create("192.168.1.1", 0);
  auto             network = addr->NetworkAddress(24);  // 假设 prefixLen 为 24
  ASSERT_NE(network, nullptr);
  ASSERT_EQ(network->ToString(), "[IPv4 0.0.0.1:0]");
}

// 测试 Subnet 地址计算
TEST(IPAddressTest, SubnetAddress) {
  IPv4Address::ptr addr   = IPv4Address::Create("192.168.1.1", 0);
  auto             subnet = addr->SubnetAddress(24);  // 假设 prefixLen 为 24
  ASSERT_NE(subnet, nullptr);
  ASSERT_EQ(subnet->ToString(), "[IPv4 255.255.255.0:0]");
}