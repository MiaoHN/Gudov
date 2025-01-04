#include "http_server.h"

#include "gudov/log.h"

namespace gudov {

namespace http {

static gudov::Logger::ptr g_logger = LOG_NAME("system");

HttpServer::HttpServer(bool keepalive, IOManager* worker, IOManager* accept_worker)
    : TcpServer(worker, accept_worker), m_is_keep_alive(keepalive) {
  m_dispatch.reset(new ServletDispatch);
}

void HttpServer::handleClient(Socket::ptr client) {
  LOG_DEBUG(g_logger) << "handleClient " << *client;
  HttpSession::ptr session(new HttpSession(client));
  do {
    auto req = session->recvRequest();
    if (!req) {
      LOG_DEBUG(g_logger) << "recv http request fail, errno=" << errno << " errstr=" << strerror(errno)
                          << " cliet:" << *client << " keep_alive=" << m_is_keep_alive;
      break;
    }

    HttpResponse::ptr rsp(new HttpResponse(req->getVersion(), req->isClose() || !m_is_keep_alive));
    rsp->setHeader("Server", getName());
    m_dispatch->handle(req, rsp, session);
    session->sendResponse(rsp);

    if (!m_is_keep_alive || req->isClose()) {
      break;
    }
  } while (true);
  session->close();
}

}  // namespace http

}  // namespace gudov