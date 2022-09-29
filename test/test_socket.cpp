#include "gudov/gudov.h"
#include "gudov/iomanager.h"
#include "gudov/socket.h"

static gudov::Logger::ptr g_looger = GUDOV_LOG_ROOT();

void testSocket() {
  // std::vector<gudov::Address::ptr> addrs;
  // gudov::Address::Lookup(addrs, "www.baidu.com", AF_INET);
  // gudov::IPAddress::ptr addr;
  // for(auto& i : addrs) {
  //    GUDOV_LOG_INFO(g_looger) << i->toString();
  //    addr = std::dynamic_pointer_cast<gudov::IPAddress>(i);
  //    if(addr) {
  //        break;
  //    }
  //}
  gudov::IPAddress::ptr addr =
      gudov::Address::LookupAnyIPAddress("www.baidu.com");
  if (addr) {
    GUDOV_LOG_INFO(g_looger) << "get address: " << addr->toString();
  } else {
    GUDOV_LOG_ERROR(g_looger) << "get address fail";
    return;
  }

  gudov::Socket::ptr sock = gudov::Socket::CreateTCP(addr);
  addr->setPort(80);
  GUDOV_LOG_INFO(g_looger) << "addr=" << addr->toString();
  if (!sock->connect(addr)) {
    GUDOV_LOG_ERROR(g_looger) << "connect " << addr->toString() << " fail";
    return;
  } else {
    GUDOV_LOG_INFO(g_looger) << "connect " << addr->toString() << " connected";
  }

  const char buff[] = "GET / HTTP/1.0\r\n\r\n";
  int        rt     = sock->send(buff, sizeof(buff));
  if (rt <= 0) {
    GUDOV_LOG_INFO(g_looger) << "send fail rt=" << rt;
    return;
  }

  std::string buffs;
  buffs.resize(4096 * 3);
  rt = sock->recv(&buffs[0], buffs.size());

  if (rt <= 0) {
    GUDOV_LOG_INFO(g_looger) << "recv fail rt=" << rt;
    return;
  }

  buffs.resize(rt);
  GUDOV_LOG_INFO(g_looger) << buffs;
}

int main(int argc, char** argv) {
  gudov::IOManager iom;
  iom.schedule(&testSocket);
  return 0;
}