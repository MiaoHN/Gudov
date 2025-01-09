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

  ServletDispatch::ptr GetServletDispatch() const { return dispatch_; }
  void                 SetServletDispatch(ServletDispatch::ptr v) { dispatch_ = v; }

 protected:
  virtual void HandleClient(Socket::ptr client) override;

 private:
  bool                 is_keep_alive_;
  ServletDispatch::ptr dispatch_;
};

}  // namespace http

}  // namespace gudov
