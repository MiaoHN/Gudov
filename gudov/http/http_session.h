#pragma once

#include "gudov/socket_stream.h"
#include "http.h"

namespace gudov {

namespace http {

class HttpSession : public SocketStream {
 public:
  using ptr = std::shared_ptr<HttpSession>;

  HttpSession(Socket::ptr sock, bool owner = true);

  HttpRequest::ptr RecvRequest();

  int SendResponse(HttpResponse::ptr rsp);
};

}  // namespace http

}  // namespace gudov
