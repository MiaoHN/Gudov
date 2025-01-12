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

static std::atomic<int> handled_start_count(0);
static std::atomic<int> handled_close_count(0);

void HttpServer::HandleClient(Socket::ptr client) {
  LOG_DEBUG(g_logger) << "HandleClient " << *client;

  HttpSession::ptr session = std::make_shared<HttpSession>(client);

  // if (handled_start_count % 1000 == 0) {
  //   LOG_INFO(g_logger) << "HttpServer::HandleClient::START handled_start_count=" << handled_start_count
  //                      << " handled_close_count=" << handled_close_count;
  // }
  handled_start_count++;

  do {
    auto req = session->RecvRequest();
    if (!req) {
      LOG_DEBUG(g_logger) << "recv http request fail, errno=" << errno << " errstr=" << strerror(errno)
                          << " cliet:" << *client << " keep_alive=" << is_keep_alive_;
      break;
    }

    HttpResponse::ptr rsp = std::make_shared<HttpResponse>(req->GetVersion(), req->IsClose() || !is_keep_alive_);

    rsp->SetHeader("Server", GetName());

    dispatch_->Handle(req, rsp, session);

    session->SendResponse(rsp);

    if (!is_keep_alive_ || req->IsClose()) {
      break;
    }
  } while (true);

  handled_close_count++;

  // if (handled_start_count % 1000 == 0) {
  //   LOG_INFO(g_logger) << "HttpServer::HandleClient::END handled_start_count=" << handled_start_count
  //                      << " handled_close_count=" << handled_close_count;
  // }

  session->Close();
}

}  // namespace http

}  // namespace gudov