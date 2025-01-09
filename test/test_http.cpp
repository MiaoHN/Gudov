#include "gudov/http/http.h"
#include "gudov/log.h"

void test_request() {
  gudov::http::HttpRequest::ptr req(new gudov::http::HttpRequest);
  req->SetHeader("host", "www.baidu.com");
  req->SetBody("hello gudov");
  req->Dump(std::cout) << std::endl;
}

void test_response() {
  gudov::http::HttpResponse::ptr rsp(new gudov::http::HttpResponse);
  rsp->SetHeader("X-X", "gudov");
  rsp->SetBody("hello gudov");
  rsp->SetStatus((gudov::http::HttpStatus)400);
  rsp->SetClose(false);

  rsp->Dump(std::cout) << std::endl;
}

int main(int argc, char** argv) {
  test_request();
  test_response();
  return 0;
}