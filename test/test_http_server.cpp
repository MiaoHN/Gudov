#include "gudov/http/http_server.h"
#include "gudov/log.h"

static gudov::Logger::ptr g_logger = LOG_ROOT();

gudov ::IOManager::ptr worker;

void run() {
  g_logger->SetLevel(gudov::LogLevel::INFO);
  gudov::http::HttpServer::ptr server(new gudov::http::HttpServer(true));
  gudov::Address::ptr          addr = gudov::Address::LookupAnyIPAddress("0.0.0.0:8020");
  while (!server->Bind(addr)) {
    sleep(2);
  }
  auto sd = server->GetServletDispatch();
  sd->addServlet("/gudov/xx", [](gudov::http::HttpRequest::ptr req, gudov::http::HttpResponse::ptr rsp,
                                 gudov::http::HttpSession::ptr session) {
    rsp->SetBody(req->ToString());
    return 0;
  });

  sd->addGlobServlet("/gudov/*", [](gudov::http::HttpRequest::ptr req, gudov::http::HttpResponse::ptr rsp,
                                    gudov::http::HttpSession::ptr session) {
    rsp->SetBody("Glob:\r\n" + req->ToString());
    return 0;
  });

  sd->addGlobServlet("/gudovx/*", [](gudov::http::HttpRequest::ptr req, gudov::http::HttpResponse::ptr rsp,
                                     gudov::http::HttpSession::ptr session) {
    rsp->SetBody(
        "<html>"
        "<head><title>404 Not Found</title></head>"
        "<body>"
        "<center><h1>404 Not Found</h1></center>"
        "<hr><center>nginx/1.16.0</center>"
        "</body>"
        "</html>"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->"
        "<!-- a padding to disable MSIE and Chrome friendly error page -->");
    return 0;
  });
  server->Start();

  while (!server->IsStop()) {
    sleep(2);
  }

  server->Stop();
}

int main(int argc, char** argv) {
  gudov::IOManager iom(1, true, "main");
  worker.reset(new gudov::IOManager(3, false, "worker"));
  iom.Schedule(run);
  return 0;
}