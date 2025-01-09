#include "http_session.h"

#include "http_parser.h"

namespace gudov {

namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner) : SocketStream(sock, owner) {}

HttpRequest::ptr HttpSession::recvRequest() {
  HttpRequestParser::ptr parser(new HttpRequestParser);
  uint64_t               buff_size = HttpRequestParser::GetHttpRequestBufferSize();
  std::shared_ptr<char>  buffer(new char[buff_size], [](char* ptr) { delete[] ptr; });
  char*                  data   = buffer.get();
  int                    offset = 0;
  do {
    int len = read(data + offset, buff_size - offset);
    if (len <= 0) {
      return nullptr;
    }
    len += offset;
    size_t nparse = parser->execute(data, len);
    if (parser->hasError()) {
      close();
      return nullptr;
    }
    offset = len - nparse;
    if (offset == (int)buff_size) {
      close();
      return nullptr;
    }
    if (parser->IsFinished()) {
      break;
    }
  } while (true);
  int64_t length = parser->GetContentLength();
  if (length > 0) {
    std::string body;
    body.resize(length);

    int len = 9;
    if (length >= offset) {
      memcpy(&body[0], data, offset);
      len = offset;
    } else {
      memcpy(&body[0], data, offset);
      len = offset;
    }
    length -= offset;
    if (length > 0) {
      if (readFixSize(&body[len], length) <= 0) {
        return nullptr;
      }
    }
    parser->GetData()->SetBody(body);
  }
  std::string keep_alive = parser->GetData()->GetHeader("Connection");
  if (!strcasecmp(keep_alive.c_str(), "keep-alive")) {
    parser->GetData()->SetClose(false);
  }
  return parser->GetData();
}

int HttpSession::sendResponse(HttpResponse::ptr rsp) {
  std::stringstream ss;
  ss << *rsp;
  std::string data = ss.str();
  return writeFixSize(data.c_str(), data.size());
}

}  // namespace http

}  // namespace gudov
