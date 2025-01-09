#pragma once

#include "http.h"
#include "http11_parser.h"
#include "httpclient_parser.h"

namespace gudov {

namespace http {

class HttpRequestParser {
 public:
  using ptr = std::shared_ptr<HttpRequestParser>;
  HttpRequestParser();
  size_t execute(char* data, size_t len);
  int    IsFinished();
  int    hasError();

  HttpRequest::ptr GetData() const { return data_; }
  void             SetError(int v) { error_ = v; }

  uint64_t           GetContentLength();
  const http_parser& GetParser() const { return parser_; }

  static uint64_t GetHttpRequestBufferSize();
  static uint64_t GetHttpRequestMaxBodySize();

 private:
  http_parser      parser_;
  HttpRequest::ptr data_;

  /**
   * @brief error number
   * @details
   * 1000: invalid method
   * 1001: invalid version
   * 1002: invalid field
   *
   */
  int error_;
};

class HttpResponseParser {
 public:
  using ptr = std::shared_ptr<HttpResponseParser>;
  HttpResponseParser();
  size_t execute(char* data, size_t len, bool chunck);
  int    IsFinished();
  int    hasError();

  HttpResponse::ptr GetData() const { return data_; }
  void              SetError(int v) { error_ = v; }

  uint64_t GetContentLength();

  const httpclient_parser& GetParser() const { return parser_; }

 public:
  static uint64_t GetHttpResponseBufferSize();
  static uint64_t GetHttpResponseMaxBodySize();

 private:
  httpclient_parser parser_;
  HttpResponse::ptr data_;
  /**
   * @brief error number
   * @details
   * 1001: invalid version
   * 1002: invalid field
   *
   */
  int error_;
};

}  // namespace http

}  // namespace gudov
