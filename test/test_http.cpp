#include "gudov/http/http.h"
#include "gudov/log.h"

void test_request() {
  gudov::http::HttpRequest::ptr req(new gudov::http::HttpRequest);
  req->setHeader("host", "www.baidu.com");
  req->setBody("hello gudov");
  req->dump(std::cout) << std::endl;
}

void test_response() {
  gudov::http::HttpResponse::ptr rsp(new gudov::http::HttpResponse);
  rsp->setHeader("X-X", "gudov");
  rsp->setBody("hello gudov");
  rsp->setStatus((gudov::http::HttpStatus)400);
  rsp->setClose(false);

  rsp->dump(std::cout) << std::endl;
}

int main(int argc, char** argv) {
  test_request();
  test_response();
  return 0;
}