#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "gudov/hook.h"
#include "gudov/iomanager.h"
#include "gudov/log.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

void testSleep() {
  gudov::IOManager iom(1);

  iom.Schedule([]() {
    sleep(2);
    LOG_INFO(g_logger) << "sleep 2";
  });

  iom.Schedule([]() {
    sleep(3);
    LOG_INFO(g_logger) << "sleep 3";
  });

  LOG_INFO(g_logger) << "testSleep";
}

void testSock() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port   = htons(80);
  // www.baidu.com
  inet_pton(AF_INET, "36.152.44.96", &addr.sin_addr.s_addr);

  LOG_INFO(g_logger) << "begin connect";
  int rt = connect(sock, (const sockaddr *)&addr, sizeof(addr));
  LOG_INFO(g_logger) << "connect rt=" << rt << " errno=" << errno;

  if (rt) {
    return;
  }

  const char data[] = "GET / HTTP/1.0\r\n\r\n";

  rt = send(sock, data, sizeof(data), 0);

  LOG_INFO(g_logger) << "send rt=" << rt << " errno=" << errno;

  if (rt <= 0) {
    return;
  }

  std::string buff;
  buff.resize(4096 * 4);

  rt = recv(sock, &buff[0], buff.size(), 0);
  LOG_INFO(g_logger) << "recv rt=" << rt << " errno=" << errno;

  if (rt <= 0) {
    return;
  }

  buff.resize(rt);

  LOG_INFO(g_logger) << buff;
}

int main(int argc, char const *argv[]) {
  testSleep();
  // gudov::IOManager iom;
  // iom.schedule(testSock);
  return 0;
}
