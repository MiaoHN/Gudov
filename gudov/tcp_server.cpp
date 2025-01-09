#include "tcp_server.h"

#include "config.h"
#include "log.h"

namespace gudov {

static ConfigVar<uint64_t>::ptr g_tcp_server_read_timeout =
    Config::Lookup("tcp_server.read_timeout", (uint64_t)(60 * 1000 * 2), "tcp server read timeout");

static Logger::ptr g_logger = LOG_NAME("system");

TcpServer::TcpServer(IOManager* worker, IOManager* accept_worker)
    : io_worker_(worker),
      accept_worker_(accept_worker),
      recv_timeout_(g_tcp_server_read_timeout->GetValue()),
      name_("gudov/1.0.0"),
      is_stop_(true) {}

TcpServer::~TcpServer() {
  for (auto& i : socks_) {
    i->Close();
  }
  socks_.clear();
}

bool TcpServer::Bind(Address::ptr addr) {
  std::vector<Address::ptr> addrs;
  std::vector<Address::ptr> fails;
  addrs.push_back(addr);
  return Bind(addrs, fails);
}

bool TcpServer::Bind(const std::vector<Address::ptr>& addrs, std::vector<Address::ptr>& fails) {
  for (auto& addr : addrs) {
    Socket::ptr sock = Socket::CreateTCP(addr);
    if (!sock->Bind(addr)) {
      LOG_ERROR(g_logger) << "bind fail errno=" << errno << " errstr=" << strerror(errno) << " addr=["
                          << addr->ToString() << "]";
      fails.push_back(addr);
      continue;
    }
    if (!sock->Listen()) {
      LOG_ERROR(g_logger) << "listen fail errno=" << errno << " errstr=" << strerror(errno) << " addr=["
                          << addr->ToString() << "]";
      fails.push_back(addr);
      continue;
    }
    socks_.push_back(sock);
  }

  if (!fails.empty()) {
    socks_.clear();
    return false;
  }

  for (auto& i : socks_) {
    LOG_DEBUG(g_logger) << "server bind success: " << *i;
  }
  return true;
}

void TcpServer::StartAccept(Socket::ptr sock) {
  while (!is_stop_) {
    Socket::ptr client = sock->Accept();
    if (client) {
      client->SetRecvTimeout(recv_timeout_);
      io_worker_->Schedule(std::bind(&TcpServer::HandleClient, shared_from_this(), client));
    } else {
      LOG_ERROR(g_logger) << "accept errno=" << errno << " errstr=" << strerror(errno);
    }
  }
}

bool TcpServer::Start() {
  if (!is_stop_) {
    return true;
  }
  is_stop_ = false;
  for (auto& sock : socks_) {
    accept_worker_->Schedule(std::bind(&TcpServer::StartAccept, shared_from_this(), sock));
  }
  return true;
}

void TcpServer::Stop() {
  is_stop_  = true;
  auto self = shared_from_this();
  accept_worker_->Schedule([this, self]() {
    for (auto& sock : socks_) {
      sock->CancelAll();
      sock->Close();
    }
    socks_.clear();
  });
}

void TcpServer::HandleClient(Socket::ptr client) { LOG_INFO(g_logger) << "HandleClient: " << *client; }

std::string TcpServer::ToString(const std::string& prefix) {
  std::stringstream ss;
  ss << prefix << "[type=" << type_ << " name=" << name_ << " io_worker=" << (io_worker_ ? io_worker_->GetName() : "")
     << " accept=" << (accept_worker_ ? accept_worker_->GetName() : "") << " recv_timeout=" << recv_timeout_ << "]"
     << std::endl;
  std::string pfx = prefix.empty() ? "    " : prefix;
  for (auto& i : socks_) {
    ss << pfx << pfx << *i << std::endl;
  }
  return ss.str();
}

}  // namespace gudov
