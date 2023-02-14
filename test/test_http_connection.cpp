#include <fstream>
#include <iostream>

#include "gudov/http/http_connection.h"
#include "gudov/iomanager.h"
#include "gudov/log.h"

static gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void test_pool() {
  gudov::http::HttpConnectionPool::ptr pool(new gudov::http::HttpConnectionPool(
      "www.baidu.com", "", 80, 10, 1000 * 30, 5));

  gudov::IOManager::GetThis()->addTimer(
      1000,
      [pool]() {
        auto r = pool->doGet("/", 300);
        GUDOV_LOG_INFO(g_logger) << r->toString();
      },
      true);
}

void run() {
  gudov::Address::ptr addr =
      gudov::Address::LookupAnyIPAddress("miaohn.github.io:80");
  if (!addr) {
    GUDOV_LOG_INFO(g_logger) << "get addr error";
    return;
  }

  gudov::Socket::ptr sock = gudov::Socket::CreateTCP(addr);
  bool               rt   = sock->connect(addr);
  if (!rt) {
    GUDOV_LOG_INFO(g_logger) << "connect " << *addr << " failed";
    return;
  }

  gudov::http::HttpConnection::ptr conn(new gudov::http::HttpConnection(sock));
  gudov::http::HttpRequest::ptr    req(new gudov::http::HttpRequest);
  req->setPath("/archives/");
  req->setHeader("host", "miaohn.github.io");
  GUDOV_LOG_INFO(g_logger) << "req:" << std::endl << *req;

  conn->sendRequest(req);
  auto rsp = conn->recvResponse();

  if (!rsp) {
    GUDOV_LOG_INFO(g_logger) << "recv response error";
    return;
  }
  GUDOV_LOG_INFO(g_logger) << "rsp:" << std::endl << *rsp;

  std::ofstream ofs("rsp.dat");
  ofs << *rsp;

  GUDOV_LOG_INFO(g_logger) << "=========================";

  auto r = gudov::http::HttpConnection::DoGet("http://www.baidu.com", 300);
  GUDOV_LOG_INFO(g_logger) << "result=" << r->result << " error=" << r->error
                           << " rsp="
                           << (r->response ? r->response->toString() : "");

  GUDOV_LOG_INFO(g_logger) << "=========================";
  test_pool();
}

int main(int argc, char** argv) {
  gudov::IOManager iom(2);
  iom.schedule(run);
  return 0;
}