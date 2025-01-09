#include <gtest/gtest.h>

#include "gudov/iomanager.h"
#include "gudov/log.h"
#include "gudov/tcp_server.h"

using namespace std;
using namespace gudov;

gudov::Logger::ptr g_logger = LOG_ROOT();

class TcpServerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // 初始化 IOManager
    ioManager       = std::make_shared<IOManager>(1, true, "test_io_manager");
    acceptIoManager = std::make_shared<IOManager>(1, true, "test_accept_io_manager");

    // 创建 TcpServer 实例
    server = std::make_shared<TcpServer>(ioManager.get(), acceptIoManager.get());
  }

  void TearDown() override {
    // 清理资源
    server->Stop();
  }

  std::shared_ptr<IOManager> ioManager;
  std::shared_ptr<IOManager> acceptIoManager;
  std::shared_ptr<TcpServer> server;
};

// 测试 TcpServer 的 Bind 和 Start 功能
TEST_F(TcpServerTest, BindAndStart) {
  // 创建一个本地地址
  auto addr = Address::LookupAny("127.0.0.1:8080");

  // 绑定地址
  EXPECT_TRUE(server->Bind(addr));

  // 启动服务器
  EXPECT_TRUE(server->Start());

  // 检查服务器是否已启动
  EXPECT_FALSE(server->IsStop());
}

// 测试 TcpServer 的 Stop 功能
TEST_F(TcpServerTest, Stop) {
  auto addr = Address::LookupAny("127.0.0.1:8081");

  EXPECT_TRUE(server->Bind(addr));
  EXPECT_TRUE(server->Start());

  // 停止服务器
  server->Stop();

  // 检查服务器是否已停止
  EXPECT_TRUE(server->IsStop());
}

// 测试 TcpServer 的 HandleClient 功能
TEST_F(TcpServerTest, HandleClient) {
  auto addr = Address::LookupAny("127.0.0.1:8082");

  EXPECT_TRUE(server->Bind(addr));
  EXPECT_TRUE(server->Start());

  // 创建一个客户端 Socket
  auto clientSocket = Socket::CreateTCP(addr);

  // 连接到服务器
  EXPECT_TRUE(clientSocket->Connect(addr));

  // 模拟处理客户端连接
  // tcpServer->HandleClient(clientSocket);

  // 检查客户端连接是否被正确处理
  // 这里可以根据 HandleClient 的具体实现添加更多的断言
}

// 测试 TcpServer 的 StartAccept 功能
TEST_F(TcpServerTest, StartAccept) {
  auto addr = Address::LookupAny("127.0.0.1:8083");

  EXPECT_TRUE(server->Bind(addr));
  EXPECT_TRUE(server->Start());

  // 创建一个监听 Socket
  auto listenSocket = Socket::CreateTCP(addr);
  EXPECT_TRUE(listenSocket->Bind(addr));
  EXPECT_TRUE(listenSocket->Listen());

  // 检查是否开始接受连接
  // 这里可以根据 StartAccept 的具体实现添加更多的断言
}
