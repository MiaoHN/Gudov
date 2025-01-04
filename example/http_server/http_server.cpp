#include "gudov/http/http_server.h"

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <string>

#include "gudov/config.h"
#include "gudov/log.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

bool exist_file(const std::string& filename) {
  std::ifstream f(filename);
  return !f.bad();
}

std::string read_file(const std::string& filename) {
  std::ifstream f(filename);
  return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void run() {
  if (exist_file("conf/http_server.yml")) {
    YAML::Node root = YAML::LoadFile("conf/http_server.yml");
    gudov::Config::LoadFromYaml(root);
    LOG_INFO(g_logger) << "Successfully load config file 'conf/http_server.yml'";
  } else {
    LOG_INFO(g_logger) << "config file 'conf/http_server.yml' doesn't exists, "
                          "use default configurations";
  }
  gudov::Address::ptr addr = gudov::Address::LookupAnyIPAddress("0.0.0.0:8020");
  if (!addr) {
    LOG_ERROR(g_logger) << "get address error";
    return;
  }

  gudov::http::HttpServer::ptr http_server(new gudov::http::HttpServer(true));
  while (!http_server->bind(addr)) {
    LOG_ERROR(g_logger) << "bind " << *addr << " fail";
    sleep(1);
  }

  auto servlet_dispatcher = http_server->getServletDispatch();

  servlet_dispatcher->addServlet("/", [](gudov::http::HttpRequest::ptr req, gudov::http::HttpResponse::ptr rsp,
                                         gudov::http::HttpSession::ptr session) {
    rsp->setBody(read_file("html/index.html"));
    return 0;
  });

  servlet_dispatcher->addServlet(
      "/index.html",
      [](gudov::http::HttpRequest::ptr req, gudov::http::HttpResponse::ptr rsp, gudov::http::HttpSession::ptr session) {
        rsp->setBody(read_file("html/index.html"));
        return 0;
      });

  servlet_dispatcher->addGlobServlet("/*", [](gudov::http::HttpRequest::ptr req, gudov::http::HttpResponse::ptr rsp,
                                              gudov::http::HttpSession::ptr session) {
    rsp->setBody(read_file("html/404.html"));
    return 0;
  });

  http_server->start();
}

int main(int argc, char** argv) {
  gudov::IOManager iom(5);
  iom.schedule(run);
  return 0;
}