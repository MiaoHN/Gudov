#pragma once

#include "gudov/tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace gudov {

namespace http {

class HttpServer : public TcpServer {
 public:
  using ptr = std::shared_ptr<HttpServer>;

  HttpServer(bool keepalive = false, IOManager* worker = IOManager::GetThis(),
             IOManager* accept_worker = IOManager::GetThis());

  ServletDispatch::ptr getServletDispatch() const { return m_dispatch; }
  void                 setServletDispatch(ServletDispatch::ptr v) { m_dispatch = v; }

 protected:
  virtual void handleClient(Socket::ptr client) override;

 private:
  bool                 m_is_keep_alive;
  ServletDispatch::ptr m_dispatch;
};

}  // namespace http

}  // namespace gudov
