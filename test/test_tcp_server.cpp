#include "gudov/iomanager.h"
#include "gudov/log.h"
#include "gudov/tcp_server.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

void run() {
  auto addr = gudov::Address::LookupAny("0.0.0.0:8033");
  // auto addr2 = gudov::UnixAddress::ptr(new
  // gudov::UnixAddress("/tmp/unix_addr"));
  std::vector<gudov::Address::ptr> addrs;
  addrs.push_back(addr);
  // addrs.push_back(addr2);

  gudov::TcpServer::ptr            tcp_server(new gudov::TcpServer);
  std::vector<gudov::Address::ptr> fails;
  while (!tcp_server->bind(addrs, fails)) {
    sleep(2);
  }
  tcp_server->start();
}
int main(int argc, char** argv) {
  gudov::IOManager iom(2);
  iom.schedule(run);
  return 0;
}