#include "gudov/address.h"
#include "gudov/bytearray.h"
#include "gudov/iomanager.h"
#include "gudov/log.h"
#include "gudov/tcp_server.h"

static gudov::Logger::ptr g_logger = LOG_ROOT();

class EchoServer : public gudov::TcpServer {
 public:
  EchoServer(int type);
  void handleClient(gudov::Socket::ptr client) override;

 private:
  int m_type = 0;
};

EchoServer::EchoServer(int type) : m_type(type) {}

void EchoServer::handleClient(gudov::Socket::ptr client) {
  LOG_INFO(g_logger) << "handleClient " << *client;
  gudov::ByteArray::ptr ba(new gudov::ByteArray);
  while (true) {
    ba->clear();
    std::vector<iovec> iovs;
    ba->getWriteBuffers(iovs, 1024);

    int rt = client->recv(&iovs[0], iovs.size());
    if (rt == 0) {
      LOG_INFO(g_logger) << "client close: " << *client;
      break;
    } else if (rt < 0) {
      LOG_INFO(g_logger) << "client error rt=" << rt << " errno=" << errno << " errstr=" << strerror(errno);
      break;
    }
    ba->setPosition(ba->getPosition() + rt);
    ba->setPosition(0);
    if (m_type == 1) {
      // text
      std::cout << ba->toString();
    } else {
      std::cout << ba->toHexString();
    }
    std::cout.flush();
  }
}

int type = 1;

void run() {
  LOG_INFO(g_logger) << "server type=" << type;
  EchoServer::ptr es(new EchoServer(type));
  auto            addr = gudov::Address::LookupAny("0.0.0.0:8020");
  while (!es->bind(addr)) {
    sleep(2);
  }
  es->start();
}

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    LOG_INFO(g_logger) << "used as[" << argv[0] << " -t] or [" << argv[0] << " -b]";
    return 0;
  }

  if (!strcmp(argv[1], "-b")) {
    type = 2;
  }

  gudov::IOManager iom(2);
  iom.Schedule(run);

  return 0;
}
