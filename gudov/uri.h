#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "address.h"

namespace gudov {

/**
 * @brief URI 类
 * @details
 *
 * foo://user@gudov.com:8888/over/there?name=ferret#nose
 *   \_/    \______________/\_________/ \_________/ \__/
 *    |            |            |            |        |
 *  scheme     authority       path        query   fragment
 *
 */
class Uri {
 public:
  using ptr = std::shared_ptr<Uri>;

  static Uri::ptr Create(const std::string& uri);

  Uri();

  const std::string& GetScheme() const { return scheme_; }

  const std::string& GetUserinfo() const { return user_info_; }

  const std::string& GetHost() const { return host_; }

  const std::string& GetPath() const;

  const std::string& GetQuery() const { return query_; }

  const std::string& GetFragment() const { return fragment_; }

  int32_t GetPort() const;

  void SetScheme(const std::string& v) { scheme_ = v; }

  void SetUserinfo(const std::string& v) { user_info_ = v; }

  void SetHost(const std::string& v) { host_ = v; }

  void SetPath(const std::string& v) { path_ = v; }

  void SetQuery(const std::string& v) { query_ = v; }

  void SetFragment(const std::string& v) { fragment_ = v; }

  void SetPort(int32_t v) { port_ = v; }

  std::ostream& Dump(std::ostream& os) const;

  std::string ToString() const;

  Address::ptr CreateAddress() const;

 private:
  bool IsDefaultPort() const;

 private:
  std::string scheme_;
  std::string user_info_;
  std::string host_;
  std::string path_;
  std::string query_;
  std::string fragment_;
  int32_t     port_;
};

}  // namespace gudov
