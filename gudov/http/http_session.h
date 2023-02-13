#ifndef __GUDOV_HTTP_SESSION_H__
#define __GUDOV_HTTP_SESSION_H__

#include "gudov/socket_stream.h"
#include "http.h"

namespace gudov {

namespace http {

class HttpSession : public SocketStream {
 public:
  using ptr = std::shared_ptr<HttpSession>;

  HttpSession(Socket::ptr sock, bool owner = true);

  HttpRequest::ptr recvRequest();

  int sendResponse(HttpResponse::ptr rsp);
};

}  // namespace http

}  // namespace gudov

#endif  // __GUDOV_HTTP_SESSION_H__