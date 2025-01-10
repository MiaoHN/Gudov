#include "servlet.h"

#include <fnmatch.h>

namespace gudov {

namespace http {

FunctionServlet::FunctionServlet(callback callback) : Servlet("FunctionServlet"), callback_(callback) {}

int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
  return callback_(request, response, session);
}

ServletDispatch::ServletDispatch() : Servlet("ServletDispatch") { default_.reset(new NotFoundServlet()); }

int32_t ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
  auto slt = GetMatchedServlet(request->GetPath());
  if (slt) {
    slt->handle(request, response, session);
  }
  return 0;
}

void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt) {
  RWMutexType::WriteLock lock(mutex_);
  datas_[uri] = slt;
}

void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback callback) {
  RWMutexType::WriteLock lock(mutex_);
  datas_[uri].reset(new FunctionServlet(callback));
}

void ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr slt) {
  RWMutexType::WriteLock lock(mutex_);
  for (auto it = globs_.begin(); it != globs_.end(); ++it) {
    if (it->first == uri) {
      globs_.erase(it);
      break;
    }
  }
  globs_.push_back(std::make_pair(uri, slt));
}

void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback callback) {
  return addGlobServlet(uri, FunctionServlet::ptr(new FunctionServlet(callback)));
}

void ServletDispatch::DelServlet(const std::string& uri) {
  RWMutexType::WriteLock lock(mutex_);
  datas_.erase(uri);
}

void ServletDispatch::DelGlobServlet(const std::string& uri) {
  RWMutexType::WriteLock lock(mutex_);
  for (auto it = globs_.begin(); it != globs_.end(); ++it) {
    if (it->first == uri) {
      globs_.erase(it);
      break;
    }
  }
}

Servlet::ptr ServletDispatch::GetServlet(const std::string& uri) {
  RWMutexType::ReadLock lock(mutex_);
  auto                  it = datas_.find(uri);
  return it == datas_.end() ? nullptr : it->second;
}

Servlet::ptr ServletDispatch::GetGlobServlet(const std::string& uri) {
  RWMutexType::ReadLock lock(mutex_);
  for (auto it = globs_.begin(); it != globs_.end(); ++it) {
    if (it->first == uri) {
      return it->second;
    }
  }
  return nullptr;
}

Servlet::ptr ServletDispatch::GetMatchedServlet(const std::string& uri) {
  RWMutexType::ReadLock lock(mutex_);
  auto                  mit = datas_.find(uri);
  if (mit != datas_.end()) {
    return mit->second;
  }
  for (auto it = globs_.begin(); it != globs_.end(); ++it) {
    if (!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
      return it->second;
    }
  }
  return default_;
}

NotFoundServlet::NotFoundServlet() : Servlet("NotFoundServlet") {}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
  static const std::string& RSP_BODY =
      "<html><head><title>404 Not Found"
      "</title></head><body><center><h1>404 Not Found</h1></center>"
      "<hr><center>gudov/1.0.0</center></body></html>";

  response->SetStatus(HttpStatus::NOT_FOUND);
  response->SetHeader("Server", "gudov/1.0.0");
  response->SetHeader("Content-Type", "text/html");
  response->SetBody(RSP_BODY);
  return 0;
}

}  // namespace http
}  // namespace gudov