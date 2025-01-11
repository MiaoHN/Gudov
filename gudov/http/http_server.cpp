#include "http_server.h"

#include "gudov/log.h"

namespace gudov {

namespace http {

static gudov::Logger::ptr g_logger = LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, IOManager* worker, IOManager* accept_worker)
    : TcpServer(worker, accept_worker), is_keep_alive_(keepalive) {
  dispatch_.reset(new ServletDispatch);

  type_ = "http";
}

void HttpServer::HandleClient(Socket::ptr client) {
  LOG_DEBUG(g_logger) << "HandleClient " << *client;
  HttpSession::ptr session(new HttpSession(client));
  do {
    auto req = session->RecvRequest();
    if (!req) {
      LOG_DEBUG(g_logger) << "recv http request fail, errno=" << errno << " errstr=" << strerror(errno)
                          << " cliet:" << *client << " keep_alive=" << is_keep_alive_;
      break;
    }

    HttpResponse::ptr rsp(new HttpResponse(req->GetVersion(), req->IsClose() || !is_keep_alive_));
    rsp->SetHeader("Server", GetName());
    dispatch_->Handle(req, rsp, session);
    session->SendResponse(rsp);

    if (!is_keep_alive_ || req->IsClose()) {
      break;
    }
  } while (true);
}

}  // namespace http

}  // namespace gudov