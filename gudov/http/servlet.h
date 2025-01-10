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

  Servlet(const std::string& name) : name_(name) {}
  virtual ~Servlet() {}
  virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) = 0;

  const std::string& GetName() const { return name_; }

 protected:
  std::string name_;
};

class FunctionServlet : public Servlet {
 public:
  using ptr = std::shared_ptr<FunctionServlet>;
  using callback =
      std::function<int32_t(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session)>;

  FunctionServlet(callback callback);

  virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

 private:
  callback callback_;
};

class ServletDispatch : public Servlet {
 public:
  using ptr         = std::shared_ptr<ServletDispatch>;
  using RWMutexType = RWMutex;

  ServletDispatch();
  virtual int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;

  void AddServlet(const std::string& uri, Servlet::ptr slt);
  void AddServlet(const std::string& uri, FunctionServlet::callback callback);
  void AddGlobServlet(const std::string& uri, Servlet::ptr slt);
  void AddGlobServlet(const std::string& uri, FunctionServlet::callback callback);

  void DelServlet(const std::string& uri);
  void DelGlobServlet(const std::string& uri);

  Servlet::ptr GetDefault() const { return default_; }
  void         SetDefault(Servlet::ptr v) { default_ = v; }

  Servlet::ptr GetServlet(const std::string& uri);
  Servlet::ptr GetGlobServlet(const std::string& uri);

  Servlet::ptr GetMatchedServlet(const std::string& uri);

 private:
  RWMutexType mutex_;
  // uri(/sylar/xxx) -> servlet
  std::unordered_map<std::string, Servlet::ptr> datas_;
  // uri(/sylar/*) -> servlet
  std::vector<std::pair<std::string, Servlet::ptr>> globs_;
  //默认servlet，所有路径都没匹配到时使用
  Servlet::ptr default_;
};

class NotFoundServlet : public Servlet {
 public:
  using ptr = std::shared_ptr<NotFoundServlet>;
  NotFoundServlet();
  int32_t handle(HttpRequest::ptr request, HttpResponse::ptr response, HttpSession::ptr session) override;
};

}  // namespace http

}  // namespace gudov
