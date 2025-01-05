#include "tcp_server.h"

#include "config.h"
#include "log.h"

namespace gudov {

static ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
    Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

static Logger::ptr g_logger = LOG_NAME("system");

TcpServer::TcpServer(IOManager* woker, IOManager* accept_woker)
    : m_worker(woker),
      m_accept_worker(accept_woker),
      m_recv_timeout(g_tcp_server_read_timeout->GetValue()),
      name_("gudov/1.0.0"),
      m_is_stop(true) {}

TcpServer::~TcpServer() {
  for (auto& i : m_socks) {
    i->close();
  }
  m_socks.clear();
}

bool TcpServer::bind(Address::ptr addr) {
  std::vector<Address::ptr> addrs;
  std::vector<Address::ptr> fails;
  addrs.push_back(addr);
  return bind(addrs, fails);
}

bool TcpServer::bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails) {
  for (auto& addr : addrs) {
    Socket::ptr sock = Socket::CreateTCP(addr);
    if (!sock->bind(addr)) {
      LOG_ERROR(g_logger) << "bind fail errno=" << errno << " errstr=" << strerror(errno) << " addr=["
                          << addr->toString() << "]";
      fails.push_back(addr);
      continue;
    }
    if (!sock->listen()) {
      LOG_ERROR(g_logger) << "listen fail errno=" << errno << " errstr=" << strerror(errno) << " addr=["
                          << addr->toString() << "]";
      fails.push_back(addr);
      continue;
    }
    m_socks.push_back(sock);
  }

  if (!fails.empty()) {
    m_socks.clear();
    return false;
  }

  for (auto& i : m_socks) {
    LOG_DEBUG(g_logger) << "server bind success: " << *i;
  }
  return true;
}

void TcpServer::startAccept(Socket::ptr sock) {
  while (!m_is_stop) {
    Socket::ptr client = sock->accept();
    if (client) {
      client->setRecvTimeout(m_recv_timeout);
      m_worker->Schedule(std::bind(&TcpServer::handleClient, shared_from_this(), client));
    } else {
      LOG_ERROR(g_logger) << "accept errno=" << errno << " errstr=" << strerror(errno);
    }
  }
}

bool TcpServer::start() {
  if (!m_is_stop) {
    return true;
  }
  m_is_stop = false;
  for (auto& sock : m_socks) {
    m_accept_worker->Schedule(std::bind(&TcpServer::startAccept, shared_from_this(), sock));
  }
  return true;
}

void TcpServer::stop() {
  m_is_stop = true;
  auto self = shared_from_this();
  m_accept_worker->Schedule([this, self]() {
    for (auto& sock : m_socks) {
      sock->cancelAll();
      sock->close();
    }
    m_socks.clear();
  });
}

void TcpServer::handleClient(Socket::ptr client) { LOG_INFO(g_logger) << "handleClient: " << *client; }

}  // namespace gudov
