#include "gudov/http/http_server.h"

#include "gudov/log.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void run() {
  g_logger->setLevel(gudov::LogLevel::INFO);
  gudov::Address::ptr addr = gudov::Address::LookupAnyIPAddress("0.0.0.0:8020");
  if (!addr) {
    GUDOV_LOG_ERROR(g_logger) << "get address error";
    return;
  }

  gudov::http::HttpServer::ptr http_server(new gudov::http::HttpServer(true));
  while (!http_server->bind(addr)) {
    GUDOV_LOG_ERROR(g_logger) << "bind " << *addr << " fail";
    sleep(1);
  }

  http_server->start();
}

int main(int argc, char** argv) {
  gudov::IOManager iom(1);
  iom.schedule(run);
  return 0;
}