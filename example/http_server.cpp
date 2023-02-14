#include "gudov/http/http_server.h"

#include "gudov/log.h"

static gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void run() {
  g_logger->setLevel(gudov::LogLevel::INFO);
  gudov::http::HttpServer::ptr server(new gudov::http::HttpServer(true));
  gudov::Address::ptr addr = gudov::Address::LookupAnyIPAddress("0.0.0.0:8020");
  while (!server->bind(addr)) {
    sleep(2);
  }
  auto sd = server->getServletDispatch();
  sd->addServlet("/gudov/xx", [](gudov::http::HttpRequest::ptr  req,
                                 gudov::http::HttpResponse::ptr rsp,
                                 gudov::http::HttpSession::ptr  session) {
    rsp->setBody(req->toString());
    return 0;
  });

  sd->addGlobServlet("/gudov/*", [](gudov::http::HttpRequest::ptr  req,
                                    gudov::http::HttpResponse::ptr rsp,
                                    gudov::http::HttpSession::ptr  session) {
    rsp->setBody("Glob:\r\n" + req->toString());
    return 0;
  });

  sd->addGlobServlet("/gudovx/*", [](gudov::http::HttpRequest::ptr  req,
                                     gudov::http::HttpResponse::ptr rsp,
                                     gudov::http::HttpSession::ptr  session) {
    rsp->setBody(
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
  server->start();
}

int main(int argc, char** argv) {
  gudov::IOManager iom(4);
  iom.schedule(run);
  return 0;
}