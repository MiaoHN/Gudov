#include "gudov/iomanager.h"
#include "gudov/log.h"
#include "gudov/socket.h"

static gudov::Logger::ptr g_logger = LOG_ROOT();

void run() {
  gudov::IPAddress::ptr addr = gudov::Address::LookupAnyIPAddress("0.0.0.0:8050");
  gudov::Socket::ptr    sock = gudov::Socket::CreateUDP(addr);
  if (sock->Bind(addr)) {
    LOG_INFO(g_logger) << "udp Bind : " << *addr;
  } else {
    LOG_ERROR(g_logger) << "udp Bind : " << *addr << " fail";
    return;
  }
  while (true) {
    char                buff[1024];
    gudov::Address::ptr from(new gudov::IPv4Address);
    int                 len = sock->RecvFrom(buff, 1024, from);
    if (len > 0) {
      buff[len] = '\0';
      LOG_INFO(g_logger) << "recv: " << buff << " from: " << *from;
      len = sock->SendTo(buff, len, from);
      if (len < 0) {
        LOG_INFO(g_logger) << "send: " << buff << " to: " << *from << " error=" << len;
      }
    }
  }
}

int main(int argc, char const *argv[]) {
  gudov::IOManager iom(1);
  iom.Schedule(run);
  return 0;
}
