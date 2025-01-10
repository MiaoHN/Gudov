#include "http.h"

namespace gudov {

namespace http {

HttpMethod StringToHttpMethod(const std::string& m) {
#define XX(num, name, string)            \
  if (strcmp(#string, m.c_str()) == 0) { \
    return HttpMethod::name;             \
  }
  HTTP_METHOD_MAP(XX);
#undef XX
  return HttpMethod::INVALID_METHOD;
}

HttpMethod CharsToHttpMethod(const char* m) {
#define XX(num, name, string)                      \
  if (strncmp(#string, m, strlen(#string)) == 0) { \
    return HttpMethod::name;                       \
  }
  HTTP_METHOD_MAP(XX);
#undef XX
  return HttpMethod::INVALID_METHOD;
}

static const char* s_method_string[] = {
#define XX(num, name, string) #string,
    HTTP_METHOD_MAP(XX)
#undef XX
};

const char* HttpMethodToString(const HttpMethod& m) {
  uint32_t idx = (uint32_t)m;
  if (idx >= (sizeof(s_method_string) / sizeof(s_method_string[0]))) {
    return "<unknown>";
  }
  return s_method_string[idx];
}

const char* HttpStatusToString(const HttpStatus& s) {
  switch (s) {
#define XX(code, name, msg) \
  case HttpStatus::name:    \
    return #msg;
    HTTP_STATUS_MAP(XX);
#undef XX
    default:
      return "<unknown>";
  }
}

bool CaseInsensitiveLess::operator()(const std::string& lhs, const std::string& rhs) const {
  return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
}

HttpRequest::HttpRequest(uint8_t version, bool close)
    : method_(HttpMethod::GET), version_(version), close_(close), path_("/") {}

std::string HttpRequest::GetHeader(const std::string& key, const std::string& def) const {
  auto it = headers_.find(key);
  return it == headers_.end() ? def : it->second;
}

std::string HttpRequest::GetParam(const std::string& key, const std::string& def) const {
  auto it = params_.find(key);
  return it == params_.end() ? def : it->second;
}

std::string HttpRequest::GetCookie(const std::string& key, const std::string& def) const {
  auto it = cookies_.find(key);
  return it == cookies_.end() ? def : it->second;
}

void HttpRequest::SetHeader(const std::string& key, const std::string& val) { headers_[key] = val; }

void HttpRequest::SetParam(const std::string& key, const std::string& val) { params_[key] = val; }

void HttpRequest::SetCookie(const std::string& key, const std::string& val) { cookies_[key] = val; }

void HttpRequest::DelHeader(const std::string& key) { headers_.erase(key); }

void HttpRequest::DelParam(const std::string& key) { params_.erase(key); }

void HttpRequest::DelCookie(const std::string& key) { cookies_.erase(key); }

bool HttpRequest::HasHeader(const std::string& key, std::string* val) {
  auto it = headers_.find(key);
  if (it == headers_.end()) {
    return false;
  }
  if (val) {
    *val = it->second;
  }
  return true;
}

bool HttpRequest::HasParam(const std::string& key, std::string* val) {
  auto it = params_.find(key);
  if (it == params_.end()) {
    return false;
  }
  if (val) {
    *val = it->second;
  }
  return true;
}

bool HttpRequest::HasCookie(const std::string& key, std::string* val) {
  auto it = cookies_.find(key);
  if (it == cookies_.end()) {
    return false;
  }
  if (val) {
    *val = it->second;
  }
  return true;
}

std::ostream& HttpRequest::Dump(std::ostream& os) const {
  // GET /uri HTTP/1.1
  // Host: wwww.baidu.com
  //
  os << HttpMethodToString(method_) << " " << path_ << (query_.empty() ? "" : "?") << query_
     << (fragment_.empty() ? "" : "#") << fragment_ << " HTTP/" << ((uint32_t)(version_ >> 4)) << "."
     << ((uint32_t)(version_ & 0x0F)) << "\r\n";
  os << "connection: " << (close_ ? "close" : "keep-alive") << "\r\n";
  for (auto& i : headers_) {
    if (strcasecmp(i.first.c_str(), "connection") == 0) {
      continue;
    }
    os << i.first << ":" << i.second << "\r\n";
  }

  if (!body_.empty()) {
    os << "content-length: " << body_.size() << "\r\n\r\n" << body_;
  } else {
    os << "\r\n";
  }
  return os;
}

std::string HttpRequest::ToString() const {
  std::stringstream ss;
  Dump(ss);
  return ss.str();
}

HttpResponse::HttpResponse(uint8_t version, bool close)
    : status_(HttpStatus::OK), version_(version), close_(close) {}

std::string HttpResponse::GetHeader(const std::string& key, const std::string& def) const {
  auto it = headers_.find(key);
  return it == headers_.end() ? def : it->second;
}

void HttpResponse::SetHeader(const std::string& key, const std::string& val) { headers_[key] = val; }

void HttpResponse::DelHeader(const std::string& key) { headers_.erase(key); }

std::string HttpResponse::ToString() const {
  std::stringstream ss;
  Dump(ss);
  return ss.str();
}

std::ostream& HttpResponse::Dump(std::ostream& os) const {
  os << "HTTP/" << ((uint32_t)(version_ >> 4)) << "." << ((uint32_t)(version_ & 0x0F)) << " " << (uint32_t)status_
     << " " << (reason_.empty() ? HttpStatusToString(status_) : reason_) << "\r\n";

  for (auto& i : headers_) {
    if (strcasecmp(i.first.c_str(), "connection") == 0) {
      continue;
    }
    os << i.first << ": " << i.second << "\r\n";
  }
  os << "connection: " << (close_ ? "close" : "keep-alive") << "\r\n";

  if (!body_.empty()) {
    os << "content-length: " << body_.size() << "\r\n\r\n" << body_;
  } else {
    os << "\r\n";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const HttpRequest& req) { return req.Dump(os); }

std::ostream& operator<<(std::ostream& os, const HttpResponse& rsp) { return rsp.Dump(os); }

}  // namespace http

}  // namespace gudov
