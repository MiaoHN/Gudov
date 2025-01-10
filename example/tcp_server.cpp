#include "gudov/tcp_server.h"

#include "gudov/env.h"
#include "gudov/gudov.h"
#include "gudov/iomanager.h"
#include "gudov/log.h"
#include "gudov/macro.h"
#include "gudov/socket.h"

static gudov::Logger::ptr g_logger = LOG_ROOT();

/**
 * @brief 自定义TcpServer类，重载handleClient方法
 */
class MyTcpServer : public gudov::TcpServer {
 protected:
  virtual void HandleClient(gudov::Socket::ptr client) override;
};

void MyTcpServer::HandleClient(gudov::Socket::ptr client) {
  LOG_INFO(g_logger) << "new client: " << client->ToString();
  static std::string buf;
  buf.resize(4096);
  client->Recv(&buf[0], buf.length());  // 这里有读超时，由tcp_server.read_timeout配置项进行配置，默认120秒
  LOG_INFO(g_logger) << "recv: " << buf;
  client->Close();
}

void run() {
  gudov::TcpServer::ptr server(new MyTcpServer);  // 内部依赖shared_from_this()，所以必须以智能指针形式创建对象
  auto                  addr = gudov::Address::LookupAny("0.0.0.0:12345");
  GUDOV_ASSERT(addr);
  std::vector<gudov::Address::ptr> addrs;
  addrs.push_back(addr);

  std::vector<gudov::Address::ptr> fails;
  while (!server->Bind(addrs, fails)) {
    sleep(2);
  }

  LOG_INFO(g_logger) << "bind success, " << server->ToString();

  server->Start();
}

int main(int argc, char *argv[]) {
  gudov::EnvMgr::GetInstance()->Init(argc, argv);
  gudov::Config::LoadFromConfDir(gudov::EnvMgr::GetInstance()->GetConfigPath());

  gudov::IOManager iom(2);
  iom.Schedule(&run);

  return 0;
}