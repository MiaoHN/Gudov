#ifndef __GUDOV_HTTP_CONNECTION_H__
#define __GUDOV_HTTP_CONNECTION_H__

#include "gudov/socket_stream.h"
#include "http.h"

namespace gudov {

namespace http {

class HttpConnection : public SocketStream {
 public:
  using ptr = std::shared_ptr<HttpConnection>;

  HttpConnection(Socket::ptr sock, bool owner = true);

  HttpResponse::ptr recvResponse();

  int sendRequest(HttpRequest::ptr req);
};

}  // namespace http

}  // namespace gudov

#endif  // __GUDOV_HTTP_CONNECTION_H__