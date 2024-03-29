#ifndef __GUDOV_TCP_SERVER_H__
#define __GUDOV_TCP_SERVER_H__

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
  TcpServer(IOManager* woker        = IOManager::GetThis(),
            IOManager* accept_woker = IOManager::GetThis());
  virtual ~TcpServer();

  virtual bool bind(Address::ptr addr);
  virtual bool bind(const std::vector<Address::ptr>& addrs,
                    std::vector<Address::ptr>&       fails);
  virtual bool start();
  virtual void stop();

  uint64_t    getRecvTimeout() const { return m_recv_timeout; }
  std::string getName() const { return m_name; }
  void        setRecvTimeout(uint64_t v) { m_recv_timeout = v; }
  void        setName(const std::string& v) { m_name = v; }

  bool isStop() const { return m_is_stop; }

 protected:
  virtual void handleClient(Socket::ptr client);
  virtual void startAccept(Socket::ptr sock);

 private:
  std::vector<Socket::ptr> m_socks;

  IOManager*  m_worker;
  IOManager*  m_accept_worker;
  uint64_t    m_recv_timeout;
  std::string m_name;
  bool        m_is_stop;
};

}  // namespace gudov

#endif  // __GUDOV_TCP_SERVER_H__