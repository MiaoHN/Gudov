#include <gtest/gtest.h>

#include "gudov/gudov.h"
#include "gudov/iomanager.h"
#include "gudov/socket.h"

using namespace gudov;

class SocketTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 可选的测试环境初始化
  }

  void TearDown() override {
    // 可选的测试环境清理
  }
};

// 测试创建 TCP 和 UDP Socket
TEST_F(SocketTest, CreateSocket) {
  // 创建 IPv4 TCP Socket
  auto tcp_socket = Socket::CreateTCPSocket();
  ASSERT_NE(tcp_socket, nullptr);
  EXPECT_EQ(tcp_socket->GetFamily(), Socket::IPv4);
  EXPECT_EQ(tcp_socket->GetType(), Socket::TCP);

  // 创建 IPv4 UDP Socket
  auto udp_socket = Socket::CreateUDPSocket();
  ASSERT_NE(udp_socket, nullptr);
  EXPECT_EQ(udp_socket->GetFamily(), Socket::IPv4);
  EXPECT_EQ(udp_socket->GetType(), Socket::UDP);
}

// 测试 Socket 绑定地址
TEST_F(SocketTest, BindSocket) {
  auto socket = Socket::CreateTCPSocket();
  ASSERT_NE(socket, nullptr);

  // 绑定到本地地址 (127.0.0.1:12345)
  Address::ptr addr        = IPv4Address::Create("127.0.0.1", 12345);
  bool         bind_result = socket->Bind(addr);
  EXPECT_TRUE(bind_result);

  // 检查绑定后的本地地址
  auto local_addr = socket->GetLocalAddress();
  ASSERT_NE(local_addr, nullptr);
  EXPECT_EQ(local_addr->ToString(), addr->ToString());
}

// 测试 Socket 的 Listen 和 Accept
TEST_F(SocketTest, ListenAndAccept) {
  auto server_socket = Socket::CreateTCPSocket();
  ASSERT_NE(server_socket, nullptr);

  Address::ptr addr = IPv4Address::Create("127.0.0.1", 12345);
  ASSERT_TRUE(server_socket->Bind(addr));
  ASSERT_TRUE(server_socket->Listen());

  // 在新线程中模拟客户端连接
  std::thread client_thread([]() {
    auto client_socket = Socket::CreateTCPSocket();
    ASSERT_NE(client_socket, nullptr);

    Address::ptr server_addr = IPv4Address::Create("127.0.0.1", 12345);
    ASSERT_TRUE(client_socket->Connect(server_addr));
  });

  // 服务端接受客户端连接
  auto client_socket = server_socket->Accept();
  ASSERT_NE(client_socket, nullptr);
  EXPECT_TRUE(client_socket->IsConnected());

  client_thread.join();
}

// 测试数据发送与接收
TEST_F(SocketTest, SendAndReceive) {
  auto server_socket = Socket::CreateTCPSocket();
  ASSERT_NE(server_socket, nullptr);

  Address::ptr addr = IPv4Address::Create("127.0.0.1", 12345);
  ASSERT_TRUE(server_socket->Bind(addr));
  ASSERT_TRUE(server_socket->Listen());

  // 在新线程中模拟客户端发送数据
  std::thread client_thread([]() {
    auto client_socket = Socket::CreateTCPSocket();
    ASSERT_NE(client_socket, nullptr);

    Address::ptr server_addr = IPv4Address::Create("127.0.0.1", 12345);
    ASSERT_TRUE(client_socket->Connect(server_addr));

    const char* msg        = "Hello, Server!";
    int         bytes_sent = client_socket->Send(msg, strlen(msg), 0);
    EXPECT_EQ(bytes_sent, strlen(msg));
  });

  // 服务端接收数据
  auto client_socket = server_socket->Accept();
  ASSERT_NE(client_socket, nullptr);

  char buffer[1024]   = {0};
  int  bytes_received = client_socket->Recv(buffer, sizeof(buffer), 0);
  EXPECT_GT(bytes_received, 0);
  EXPECT_STREQ(buffer, "Hello, Server!");

  client_thread.join();
}

// 测试超时设置
TEST_F(SocketTest, DISABLED_TimeoutSettings) {
  auto socket = Socket::CreateTCPSocket();
  ASSERT_NE(socket, nullptr);

  // 设置发送超时
  socket->SetSendTimeout(2000);  // 2 秒
  EXPECT_EQ(socket->GetSendTimeout(), 2000);

  // 设置接收超时
  socket->SetRecvTimeout(3000);  // 3 秒
  EXPECT_EQ(socket->GetRecvTimeout(), 3000);
}
