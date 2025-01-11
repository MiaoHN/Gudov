#include "http_session.h"

#include "http_parser.h"

namespace gudov {

namespace http {

HttpSession::HttpSession(Socket::ptr sock, bool owner) : SocketStream(sock, owner) {}

HttpSession::~HttpSession() { Close(); }

HttpRequest::ptr HttpSession::RecvRequest() {
  HttpRequestParser::ptr parser(new HttpRequestParser);
  uint64_t               buff_size = HttpRequestParser::GetHttpRequestBufferSize();
  std::shared_ptr<char>  buffer(new char[buff_size], [](char* ptr) { delete[] ptr; });
  char*                  data   = buffer.get();
  int                    offset = 0;
  do {
    int len = read(data + offset, buff_size - offset);
    if (len <= 0) {
      Close();
      return nullptr;
    }
    len += offset;
    size_t nparse = parser->execute(data, len);
    if (parser->HasError()) {
      Close();
      return nullptr;
    }
    offset = len - nparse;
    if (offset == (int)buff_size) {
      Close();
      return nullptr;
    }
    if (parser->IsFinished()) {
      break;
    }
  } while (true);
  int64_t length = parser->GetContentLength();

  auto v = parser->GetData()->GetHeader("Expect");
  if (strcasecmp(v.c_str(), "100-continue") == 0) {
    static const std::string s_data = "HTTP/1.1 100 Continue\r\n\r\n";
    WriteFixSize(s_data.c_str(), s_data.size());
    parser->GetData()->DelHeader("Expect");
  }

  if (length > 0) {
    std::string body;
    body.resize(length);

    int len = 0;
    if (length >= offset) {
      memcpy(&body[0], data, offset);
      len = offset;
    } else {
      memcpy(&body[0], data, offset);
      len = offset;
    }
    length -= offset;
    if (length > 0) {
      if (ReadFixSize(&body[len], length) <= 0) {
        return nullptr;
      }
    }
    parser->GetData()->SetBody(body);
  }

  parser->GetData()->Init();
  return parser->GetData();
}

int HttpSession::SendResponse(HttpResponse::ptr rsp) {
  std::stringstream ss;
  ss << *rsp;
  std::string data = ss.str();
  return WriteFixSize(data.c_str(), data.size());
}

}  // namespace http

}  // namespace gudov
