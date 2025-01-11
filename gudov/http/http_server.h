#pragma once

#include "gudov/tcp_server.h"
#include "http_session.h"
#include "servlet.h"

namespace gudov {

namespace http {

class HttpServer : public TcpServer {
 public:
  using ptr = std::shared_ptr<HttpServer>;

  /**
   * @brief Construct a new Http Server object
   *
   * @param keepalive
   * @param worker
   * @param accept_worker
   */
  HttpServer(bool keepalive = false, IOManager* worker = IOManager::GetThis(),
             IOManager* accept_worker = IOManager::GetThis());

  /**
   * @brief Get the Servlet Dispatch object
   *
   * @return ServletDispatch::ptr
   */
  ServletDispatch::ptr GetServletDispatch() const { return dispatch_; }

  /**
   * @brief Set the Servlet Dispatch object
   *
   * @param v
   */
  void SetServletDispatch(ServletDispatch::ptr v) { dispatch_ = v; }

 protected:
  /**
   * @brief Handle Client
   *
   * @param client
   */
  virtual void HandleClient(Socket::ptr client) override;

 private:
  /**
   * @brief If keep alive
   *
   */
  bool is_keep_alive_;

  /**
   * @brief Servlet Dispatch
   *
   */
  ServletDispatch::ptr dispatch_;
};

}  // namespace http

}  // namespace gudov
