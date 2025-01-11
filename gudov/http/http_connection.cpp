#include "http_connection.h"

#include "gudov/bytearray.h"
#include "gudov/log.h"
#include "http_parser.h"

namespace gudov {

namespace http {

static Logger::ptr g_logger = LOG_NAME("system");

std::string HttpResult::ToString() const {
  std::stringstream ss;
  ss << "[HttpResult result=" << result << " error=" << error
     << " response=" << (response ? response->ToString() : "nullptr") << "]";
  return ss.str();
}

HttpConnection::HttpConnection(Socket::ptr sock, bool owner) : SocketStream(sock, owner) {}

HttpConnection::~HttpConnection() { LOG_DEBUG(g_logger) << "HttpConnection::~HttpConnection"; }

HttpResponse::ptr HttpConnection::RecvResponse() {
  HttpResponseParser::ptr parser(new HttpResponseParser);
  uint64_t                buff_size = HttpRequestParser::GetHttpRequestBufferSize();
  // uint64_t buff_size = 100;
  std::shared_ptr<char> buffer(new char[buff_size + 1], [](char* ptr) { delete[] ptr; });
  char*                 data   = buffer.get();
  int                   offset = 0;
  do {
    int len = read(data + offset, buff_size - offset);
    if (len <= 0) {
      Close();
      return nullptr;
    }
    len += offset;
    data[len]     = '\0';
    size_t nparse = parser->execute(data, len, false);
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
  auto& client_parser = parser->GetParser();
  if (client_parser.chunked) {
    std::string body;
    int         len = offset;
    do {
      do {
        int rt = read(data + len, buff_size - len);
        if (rt <= 0) {
          Close();
          return nullptr;
        }
        len += rt;
        data[len]     = '\0';
        size_t nparse = parser->execute(data, len, true);
        if (parser->HasError()) {
          Close();
          return nullptr;
        }
        len -= nparse;
        if (len == (int)buff_size) {
          Close();
          return nullptr;
        }
      } while (!parser->IsFinished());
      len -= 2;

      LOG_INFO(g_logger) << "content_len=" << client_parser.content_len;
      if (client_parser.content_len <= len) {
        body.append(data, client_parser.content_len);
        memmove(data, data + client_parser.content_len, len - client_parser.content_len);
        len -= client_parser.content_len;
      } else {
        body.append(data, len);
        int left = client_parser.content_len - len;
        while (left > 0) {
          int rt = read(data, left > (int)buff_size ? (int)buff_size : left);
          if (rt <= 0) {
            Close();
            return nullptr;
          }
          body.append(data, rt);
          left -= rt;
        }
        len = 0;
      }
    } while (!client_parser.chunks_done);
    parser->GetData()->SetBody(body);
  } else {
    int64_t length = parser->GetContentLength();
    if (length > 0) {
      std::string body;
      body.resize(length);

      int len = 0;
      if (length >= offset) {
        memcpy(&body[0], data, offset);
        len = offset;
      } else {
        memcpy(&body[0], data, length);
        len = length;
      }
      length -= offset;
      if (length > 0) {
        if (ReadFixSize(&body[len], length) <= 0) {
          Close();
          return nullptr;
        }
      }
      parser->GetData()->SetBody(body);
    }
  }
  return parser->GetData();
}

int HttpConnection::SendRequest(HttpRequest::ptr rsp) {
  std::stringstream ss;
  ss << *rsp;
  std::string data = ss.str();
  return WriteFixSize(data.c_str(), data.size());
}

HttpResult::ptr HttpConnection::DoGet(const std::string& url, uint64_t timeout_ms,
                                      const std::map<std::string, std::string>& headers, const std::string& body) {
  Uri::ptr uri = Uri::Create(url);
  if (!uri) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr, "invalid url: " + url);
  }
  return DoGet(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoGet(Uri::ptr uri, uint64_t timeout_ms,
                                      const std::map<std::string, std::string>& headers, const std::string& body) {
  return DoRequest(HttpMethod::GET, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(const std::string& url, uint64_t timeout_ms,
                                       const std::map<std::string, std::string>& headers, const std::string& body) {
  Uri::ptr uri = Uri::Create(url);
  if (!uri) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr, "invalid url: " + url);
  }
  return DoPost(uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoPost(Uri::ptr uri, uint64_t timeout_ms,
                                       const std::map<std::string, std::string>& headers, const std::string& body) {
  return DoRequest(HttpMethod::POST, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers, const std::string& body) {
  Uri::ptr uri = Uri::Create(url);
  if (!uri) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_URL, nullptr, "invalid url: " + url);
  }
  return DoRequest(method, uri, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnection::DoRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers, const std::string& body) {
  HttpRequest::ptr req = std::make_shared<HttpRequest>();
  req->SetPath(uri->GetPath());
  req->SetQuery(uri->GetQuery());
  req->SetFragment(uri->GetFragment());
  req->SetMethod(method);
  bool has_host = false;
  for (auto& i : headers) {
    if (strcasecmp(i.first.c_str(), "connection") == 0) {
      if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
        req->SetClose(false);
      }
      continue;
    }

    if (!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
      has_host = !i.second.empty();
    }

    req->SetHeader(i.first, i.second);
  }
  if (!has_host) {
    req->SetHeader("Host", uri->GetHost());
  }
  req->SetBody(body);
  return DoRequest(req, uri, timeout_ms);
}

HttpResult::ptr HttpConnection::DoRequest(HttpRequest::ptr req, Uri::ptr uri, uint64_t timeout_ms) {
  Address::ptr addr = uri->CreateAddress();
  if (!addr) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::INVALID_HOST, nullptr,
                                        "invalid host: " + uri->GetHost());
  }
  Socket::ptr sock = Socket::CreateTCP(addr);
  if (!sock) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::CREATE_SOCKET_ERROR, nullptr,
                                        "create socket fail: " + addr->ToString() + " errno=" + std::to_string(errno) +
                                            " errstr=" + std::string(strerror(errno)));
  }
  if (!sock->Connect(addr)) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::CONNECT_FAIL, nullptr,
                                        "connect fail: " + addr->ToString());
  }
  sock->SetRecvTimeout(timeout_ms);
  HttpConnection::ptr conn = std::make_shared<HttpConnection>(sock);
  int                 rt   = conn->SendRequest(req);
  if (rt == 0) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER, nullptr,
                                        "send request closed by peer: " + addr->ToString());
  }
  if (rt < 0) {
    return std::make_shared<HttpResult>(
        (int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr,
        "send request socket error errno=" + std::to_string(errno) + " errstr=" + std::string(strerror(errno)));
  }
  auto rsp = conn->RecvResponse();
  if (!rsp) {
    return std::make_shared<HttpResult>(
        (int)HttpResult::Error::TIMEOUT, nullptr,
        "recv response timeout: " + addr->ToString() + " timeout_ms:" + std::to_string(timeout_ms));
  }
  return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
}

HttpConnectionPool::HttpConnectionPool(const std::string& host, const std::string& vhost, uint32_t port,
                                       uint32_t max_size, uint32_t max_alive_time, uint32_t max_request)
    : host_(host),
      vhost_(vhost),
      port_(port),
      max_size_(max_size),
      max_alive_time_(max_alive_time),
      max_request_(max_request) {}

HttpConnection::ptr HttpConnectionPool::GetConnection() {
  uint64_t                     now_ms = GetCurrentMS();
  std::vector<HttpConnection*> invalid_conns;
  HttpConnection*              ptr = nullptr;
  MutexType::Locker            lock(mutex_);
  while (!conns_.empty()) {
    auto conn = *conns_.begin();
    conns_.pop_front();
    if (!conn->IsConnected()) {
      invalid_conns.push_back(conn);
      continue;
    }
    if ((conn->create_time_ + max_alive_time_) > now_ms) {
      invalid_conns.push_back(conn);
      continue;
    }
    ptr = conn;
    break;
  }
  lock.Unlock();
  for (auto i : invalid_conns) {
    delete i;
  }
  total_ -= invalid_conns.size();

  if (!ptr) {
    IPAddress::ptr addr = Address::LookupAnyIPAddress(host_);
    if (!addr) {
      LOG_ERROR(g_logger) << "get addr fail: " << host_;
      return nullptr;
    }
    addr->SetPort(port_);
    Socket::ptr sock = Socket::CreateTCP(addr);
    if (!sock) {
      LOG_ERROR(g_logger) << "create sock fail: " << *addr;
      return nullptr;
    }
    if (!sock->Connect(addr)) {
      LOG_ERROR(g_logger) << "sock connect fail: " << *addr;
      return nullptr;
    }

    ptr = new HttpConnection(sock);
    ++total_;
  }
  return HttpConnection::ptr(ptr, std::bind(&HttpConnectionPool::ReleasePtr, std::placeholders::_1, this));
}

void HttpConnectionPool::ReleasePtr(HttpConnection* ptr, HttpConnectionPool* pool) {
  ++ptr->request_;
  if (!ptr->IsConnected() || ((ptr->create_time_ + pool->max_alive_time_) >= GetCurrentMS()) ||
      (ptr->request_ >= pool->max_request_)) {
    delete ptr;
    --pool->total_;
    return;
  }
  MutexType::Locker lock(pool->mutex_);
  pool->conns_.push_back(ptr);
}

HttpResult::ptr HttpConnectionPool::doGet(const std::string& url, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers, const std::string& body) {
  return doRequest(HttpMethod::GET, url, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doGet(Uri::ptr uri, uint64_t timeout_ms,
                                          const std::map<std::string, std::string>& headers, const std::string& body) {
  std::stringstream ss;
  ss << uri->GetPath() << (uri->GetQuery().empty() ? "" : "?") << uri->GetQuery()
     << (uri->GetFragment().empty() ? "" : "#") << uri->GetFragment();
  return doGet(ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(const std::string& url, uint64_t timeout_ms,
                                           const std::map<std::string, std::string>& headers, const std::string& body) {
  return doRequest(HttpMethod::POST, url, timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doPost(Uri::ptr uri, uint64_t timeout_ms,
                                           const std::map<std::string, std::string>& headers, const std::string& body) {
  std::stringstream ss;
  ss << uri->GetPath() << (uri->GetQuery().empty() ? "" : "?") << uri->GetQuery()
     << (uri->GetFragment().empty() ? "" : "#") << uri->GetFragment();
  return doPost(ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, const std::string& url, uint64_t timeout_ms,
                                              const std::map<std::string, std::string>& headers,
                                              const std::string&                        body) {
  HttpRequest::ptr req = std::make_shared<HttpRequest>();
  req->SetPath(url);
  req->SetMethod(method);
  req->SetClose(false);
  bool has_host = false;
  for (auto& i : headers) {
    if (strcasecmp(i.first.c_str(), "connection") == 0) {
      if (strcasecmp(i.second.c_str(), "keep-alive") == 0) {
        req->SetClose(false);
      }
      continue;
    }

    if (!has_host && strcasecmp(i.first.c_str(), "host") == 0) {
      has_host = !i.second.empty();
    }

    req->SetHeader(i.first, i.second);
  }
  if (!has_host) {
    if (vhost_.empty()) {
      req->SetHeader("Host", host_);
    } else {
      req->SetHeader("Host", vhost_);
    }
  }
  req->SetBody(body);
  return doRequest(req, timeout_ms);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpMethod method, Uri::ptr uri, uint64_t timeout_ms,
                                              const std::map<std::string, std::string>& headers,
                                              const std::string&                        body) {
  std::stringstream ss;
  ss << uri->GetPath() << (uri->GetQuery().empty() ? "" : "?") << uri->GetQuery()
     << (uri->GetFragment().empty() ? "" : "#") << uri->GetFragment();
  return doRequest(method, ss.str(), timeout_ms, headers, body);
}

HttpResult::ptr HttpConnectionPool::doRequest(HttpRequest::ptr req, uint64_t timeout_ms) {
  auto conn = GetConnection();
  if (!conn) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_GET_CONNECTION, nullptr,
                                        "pool host:" + host_ + " port:" + std::to_string(port_));
  }
  auto sock = conn->GetSocket();
  if (!sock) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::POOL_INVALID_CONNECTION, nullptr,
                                        "pool host:" + host_ + " port:" + std::to_string(port_));
  }
  sock->SetRecvTimeout(timeout_ms);
  int rt = conn->SendRequest(req);
  if (rt == 0) {
    return std::make_shared<HttpResult>((int)HttpResult::Error::SEND_CLOSE_BY_PEER, nullptr,
                                        "send request closed by peer: " + sock->GetRemoteAddress()->ToString());
  }
  if (rt < 0) {
    return std::make_shared<HttpResult>(
        (int)HttpResult::Error::SEND_SOCKET_ERROR, nullptr,
        "send request socket error errno=" + std::to_string(errno) + " errstr=" + std::string(strerror(errno)));
  }
  auto rsp = conn->RecvResponse();
  if (!rsp) {
    return std::make_shared<HttpResult>(
        (int)HttpResult::Error::TIMEOUT, nullptr,
        "recv response timeout: " + sock->GetRemoteAddress()->ToString() + " timeout_ms:" + std::to_string(timeout_ms));
  }
  return std::make_shared<HttpResult>((int)HttpResult::Error::OK, rsp, "ok");
}

}  // namespace http

}  // namespace gudov
