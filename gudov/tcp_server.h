#pragma once

#include <functional>
#include <memory>

#include "address.h"
#include "iomanager.h"
#include "noncopyable.h"
#include "socket.h"

namespace gudov {

class TcpServer : public std::enable_shared_from_this<TcpServer>, NonCopyable {
 public:
  using ptr = std::shared_ptr<TcpServer>;

  TcpServer(IOManager* worker = IOManager::GetThis(), IOManager* accept_worker = IOManager::GetThis());
  virtual ~TcpServer();

  virtual bool Bind(Address::ptr addr);
  virtual bool Bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails);
  virtual bool Start();
  virtual void Stop();

  uint64_t    GetRecvTimeout() const { return recv_timeout_; }
  std::string GetName() const { return name_; }
  void        SetRecvTimeout(uint64_t v) { recv_timeout_ = v; }
  void        SetName(const std::string& v) { name_ = v; }

  bool IsStop() const { return is_stop_; }

  virtual std::string ToString(const std::string& prefix = "");

 protected:
  virtual void HandleClient(Socket::ptr client);
  virtual void StartAccept(Socket::ptr sock);

 private:
  std::vector<Socket::ptr> socks_;

  IOManager*  io_worker_;
  IOManager*  accept_worker_;
  uint64_t    recv_timeout_;
  std::string name_;

  std::string type_;

  bool is_stop_;
};

}  // namespace gudov
