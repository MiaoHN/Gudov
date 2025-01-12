#include "gudov/gudov.h"
#include "gudov/http/http.h"

using gudov::Address;
using gudov::Config;
using gudov::EnvMgr;
using gudov::FSUtil;
using gudov::IOManager;
using gudov::Logger;
using gudov::http::HttpRequest;
using gudov::http::HttpResponse;
using gudov::http::HttpServer;
using gudov::http::HttpSession;

Logger::ptr g_logger = LOG_ROOT();

void run() {
  HttpServer::ptr server(new HttpServer(true));
  Address::ptr    addr = Address::LookupAnyIPAddress("0.0.0.0:8888");

  while (!server->Bind(addr)) {
    LOG_ERROR(g_logger) << "Bind " << *addr << " fail";
    sleep(2);
  }

  auto sd = server->GetServletDispatch();

  sd->AddServlet("/", [](HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session) {
    rsp->SetBody(FSUtil::ReadFile("html/index.html"));
    return 0;
  });

  sd->AddServlet("/index.html", [](HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session) {
    rsp->SetBody(FSUtil::ReadFile("html/index.html"));
    return 0;
  });

  sd->AddGlobServlet("/*", [](HttpRequest::ptr req, HttpResponse::ptr rsp, HttpSession::ptr session) {
    LOG_INFO(g_logger) << "404: " << req->ToString();
    rsp->SetBody(FSUtil::ReadFile("html/404.html"));
    return 0;
  });

  server->Start();
}

int main(int argc, char** argv) {
  EnvMgr::GetInstance()->Init(argc, argv);
  Config::LoadFromConfDir(EnvMgr::GetInstance()->GetConfigPath());

  IOManager iom(1, true, "http_server");

  iom.Schedule(run);

  return 0;
}