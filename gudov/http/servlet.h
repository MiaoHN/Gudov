#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "gudov/thread.h"
#include "http.h"
#include "http_session.h"

namespace gudov {

namespace http {

class Servlet {
 public:
  using ptr = std::shared_ptr<Servlet>;

  Servlet(const std::string& name) : m_name(name) {}
  virtual ~Servlet() {}
  virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) = 0;

  const std::string& getName() const { return m_name; }

 protected:
  std::string m_name;
};

class FunctionServlet : public Servlet {
 public:
  using ptr = std::shared_ptr<FunctionServlet>;
  using callback =
      std::function<int32_t(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session)>;

  FunctionServlet(callback callback);

  virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

 private:
  callback m_callback;
};

class ServletDispatch : public Servlet {
 public:
  using ptr         = std::shared_ptr<ServletDispatch>;
  using RWMutexType = RWMutex;

  ServletDispatch();
  virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

  void addServlet(const std::string& uri, Servlet::ptr slt);
  void addServlet(const std::string& uri, FunctionServlet::callback callback);
  void addGlobServlet(const std::string& uri, Servlet::ptr slt);
  void addGlobServlet(const std::string& uri, FunctionServlet::callback callback);

  void delServlet(const std::string& uri);
  void delGlobServlet(const std::string& uri);

  Servlet::ptr getDefault() const { return m_default; }
  void         setDefault(Servlet::ptr v) { m_default = v; }

  Servlet::ptr getServlet(const std::string& uri);
  Servlet::ptr getGlobServlet(const std::string& uri);

  Servlet::ptr getMatchedServlet(const std::string& uri);

 private:
  RWMutexType m_mutex;
  // uri(/sylar/xxx) -> servlet
  std::unordered_map<std::string, Servlet::ptr> m_datas;
  // uri(/sylar/*) -> servlet
  std::vector<std::pair<std::string, Servlet::ptr>> m_globs;
  //默认servlet，所有路径都没匹配到时使用
  Servlet::ptr m_default;
};

class NotFoundServlet : public Servlet {
 public:
  using ptr = std::shared_ptr<NotFoundServlet>;
  NotFoundServlet();
  int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
};

}  // namespace http

}  // namespace gudov
