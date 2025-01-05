#include "servlet.h"

#include <fnmatch.h>

namespace gudov {

namespace http {

FunctionServlet::FunctionServlet(callback callback) : Servlet("FunctionServlet"), m_callback(callback) {}

int32_t FunctionServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
  return m_callback(request, response, session);
}

ServletDispatch::ServletDispatch() : Servlet("ServletDispatch") { m_default.reset(new NotFoundServlet()); }

int32_t ServletDispatch::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
  auto slt = getMatchedServlet(request->getPath());
  if (slt) {
    slt->handle(request, response, session);
  }
  return 0;
}

void ServletDispatch::addServlet(const std::string& uri, Servlet::ptr slt) {
  RWMutexType::WriteLock lock(mutex_);
  m_datas[uri] = slt;
}

void ServletDispatch::addServlet(const std::string& uri, FunctionServlet::callback callback) {
  RWMutexType::WriteLock lock(mutex_);
  m_datas[uri].reset(new FunctionServlet(callback));
}

void ServletDispatch::addGlobServlet(const std::string& uri, Servlet::ptr slt) {
  RWMutexType::WriteLock lock(mutex_);
  for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
    if (it->first == uri) {
      m_globs.erase(it);
      break;
    }
  }
  m_globs.push_back(std::make_pair(uri, slt));
}

void ServletDispatch::addGlobServlet(const std::string& uri, FunctionServlet::callback callback) {
  return addGlobServlet(uri, FunctionServlet::ptr(new FunctionServlet(callback)));
}

void ServletDispatch::delServlet(const std::string& uri) {
  RWMutexType::WriteLock lock(mutex_);
  m_datas.erase(uri);
}

void ServletDispatch::delGlobServlet(const std::string& uri) {
  RWMutexType::WriteLock lock(mutex_);
  for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
    if (it->first == uri) {
      m_globs.erase(it);
      break;
    }
  }
}

Servlet::ptr ServletDispatch::getServlet(const std::string& uri) {
  RWMutexType::ReadLock lock(mutex_);
  auto                  it = m_datas.find(uri);
  return it == m_datas.end() ? nullptr : it->second;
}

Servlet::ptr ServletDispatch::getGlobServlet(const std::string& uri) {
  RWMutexType::ReadLock lock(mutex_);
  for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
    if (it->first == uri) {
      return it->second;
    }
  }
  return nullptr;
}

Servlet::ptr ServletDispatch::getMatchedServlet(const std::string& uri) {
  RWMutexType::ReadLock lock(mutex_);
  auto                  mit = m_datas.find(uri);
  if (mit != m_datas.end()) {
    return mit->second;
  }
  for (auto it = m_globs.begin(); it != m_globs.end(); ++it) {
    if (!fnmatch(it->first.c_str(), uri.c_str(), 0)) {
      return it->second;
    }
  }
  return m_default;
}

NotFoundServlet::NotFoundServlet() : Servlet("NotFoundServlet") {}

int32_t NotFoundServlet::handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) {
  static const std::string& RSP_BODY =
      "<html><head><title>404 Not Found"
      "</title></head><body><center><h1>404 Not Found</h1></center>"
      "<hr><center>gudov/1.0.0</center></body></html>";

  response->setStatus(HttpStatus::NOT_FOUND);
  response->setHeader("Server", "gudov/1.0.0");
  response->setHeader("Content-Type", "text/html");
  response->setBody(RSP_BODY);
  return 0;
}

}  // namespace http
}  // namespace gudov