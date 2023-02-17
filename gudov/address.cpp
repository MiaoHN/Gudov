#include "address.h"

#include <ifaddrs.h>
#include <netdb.h>

#include <cstddef>

#include "endian.h"
#include "log.h"

namespace gudov {

static Logger::ptr g_logger = GUDOV_LOG_NAME("system");

template <class T>
static T CreateMask(uint32_t bits) {
  return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template <class T>
static uint32_t CountBytes(T value) {
  uint32_t result = 0;
  for (; value; ++result) {
    value &= value - 1;
  }
  return result;
}

Address::ptr Address::Create(const sockaddr* addr, socklen_t addrlen) {
  if (addr == nullptr) {
    return nullptr;
  }

  Address::ptr result;
  switch (addr->sa_family) {
    case AF_INET:
      result.reset(new IPv4Address(*(const sockaddr_in*)addr));
      break;
    case AF_INET6:
      result.reset(new IPv6Address(*(const sockaddr_in6*)addr));
      break;
    default:
      result.reset(new UnknownAddress(*addr));
      break;
  }
  return result;
}

bool Address::Lookup(std::vector<Address::ptr>& result, const std::string& host,
                     int family, int type, int protocol) {
  addrinfo hints, *results, *next;
  hints.ai_flags     = 0;
  hints.ai_family    = family;
  hints.ai_socktype  = type;
  hints.ai_protocol  = protocol;
  hints.ai_addrlen   = 0;
  hints.ai_canonname = nullptr;
  hints.ai_addr      = nullptr;
  hints.ai_next      = nullptr;

  std::string node;
  const char* service = nullptr;

  //检查 IPv6 address serivce
  if (!host.empty() && host[0] == '[') {
    const char* endipv6 =
        (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
    if (endipv6) {
      if (*(endipv6 + 1) == '\0') {
        service = endipv6 + 2;
      }
      node = host.substr(1, endipv6 - host.c_str() - 1);
    }
  }

  //检查 node serivce
  if (node.empty()) {
    service = (const char*)memchr(host.c_str(), ':', host.size());
    if (service) {
      if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
        node = host.substr(0, service - host.c_str());
        ++service;
      }
    }
  }

  if (node.empty()) {
    node = host;
  }
  int error = getaddrinfo(node.c_str(), service, &hints, &results);
  if (error) {
    GUDOV_LOG_ERROR(g_logger)
        << "Address::Lookup getaddress(" << host << ", " << family << ", "
        << type << ") err=" << error << " errstr=" << strerror(errno);
    return false;
  }

  next = results;
  while (next) {
    result.push_back(Create(next->ai_addr, (socklen_t)next->ai_addrlen));
    next = next->ai_next;
  }

  freeaddrinfo(results);
  return true;
}

Address::ptr Address::LookupAny(const std::string& host, int family, int type,
                                int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    return result[0];
  }
  return nullptr;
}

std::shared_ptr<IPAddress> Address::LookupAnyIPAddress(const std::string& host,
                                                       int family, int type,
                                                       int protocol) {
  std::vector<Address::ptr> result;
  if (Lookup(result, host, family, type, protocol)) {
    for (auto& i : result) {
      GUDOV_LOG_DEBUG(g_logger) << i->toString();
    }
    for (auto& i : result) {
      IPAddress::ptr v = std::dynamic_pointer_cast<IPAddress>(i);
      if (v) {
        return v;
      }
    }
  }
  return nullptr;
}

bool Address::GetInterfaceAddress(
    std::multimap<std::string, std::pair<Address::ptr, uint32_t>>& result,
    int                                                            family) {
  struct ifaddrs *next, *results;
  if (getifaddrs(&results) != 0) {
    GUDOV_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses getifaddrs "
                                 " err="
                              << errno << " errstr=" << strerror(errno);
    return false;
  }

  try {
    for (next = results; next; next = next->ifa_next) {
      Address::ptr addr;
      uint32_t     prefixLen = ~0u;
      if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
        continue;
      }
      switch (next->ifa_addr->sa_family) {
        case AF_INET: {
          addr             = Create(next->ifa_addr, sizeof(sockaddr_in));
          uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
          prefixLen        = CountBytes(netmask);
        } break;
        case AF_INET6: {
          addr              = Create(next->ifa_addr, sizeof(sockaddr_in6));
          in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
          prefixLen         = 0;
          for (int i = 0; i < 16; ++i) {
            prefixLen += CountBytes(netmask.s6_addr[i]);
          }
        } break;
        default:
          break;
      }

      if (addr) {
        result.insert(
            std::make_pair(next->ifa_name, std::make_pair(addr, prefixLen)));
      }
    }
  } catch (...) {
    GUDOV_LOG_ERROR(g_logger) << "Address::GetInterfaceAddresses exception";
    freeifaddrs(results);
    return false;
  }
  freeifaddrs(results);
  return true;
}

bool Address::GetInterfaceAddress(
    std::vector<std::pair<Address::ptr, uint32_t>>& result,
    const std::string& iface, int family) {
  if (iface.empty() || iface == "*") {
    if (family == AF_INET || family == AF_UNSPEC) {
      result.push_back(std::make_pair(Address::ptr(new IPv4Address()), 0u));
    }
    if (family == AF_INET6 || family == AF_UNSPEC) {
      result.push_back(std::make_pair(Address::ptr(new IPv6Address()), 0u));
    }
    return true;
  }

  std::multimap<std::string, std::pair<Address::ptr, uint32_t>> results;

  if (!GetInterfaceAddress(results, family)) {
    return false;
  }

  auto its = results.equal_range(iface);
  for (; its.first != its.second; ++its.first) {
    result.push_back(its.first->second);
  }
  return true;
}

int Address::getFamily() const { return getAddr()->sa_family; }

std::string Address::toString() const {
  std::stringstream ss;
  insert(ss);
  return ss.str();
}

bool Address::operator<(const Address& rhs) const {
  socklen_t minLen = std::min(getAddrLen(), rhs.getAddrLen());
  int       result = memcmp(getAddr(), rhs.getAddr(), minLen);
  if (result < 0) {
    return true;
  } else if (result > 0) {
    return false;
  } else if (getAddrLen() < rhs.getAddrLen()) {
    return true;
  }
  return false;
}

bool Address::operator==(const Address& rhs) const {
  return getAddrLen() == rhs.getAddrLen() &&
         memcmp(getAddr(), rhs.getAddr(), getAddrLen()) == 0;
}

bool Address::operator!=(const Address& rhs) const { return !(*this == rhs); }

IPAddress::ptr IPAddress::Create(const char* address, uint16_t port) {
  addrinfo hints, *results;
  memset(&hints, 0, sizeof(addrinfo));

  hints.ai_flags  = AI_NUMERICHOST;
  hints.ai_family = AF_UNSPEC;

  int error = getaddrinfo(address, nullptr, &hints, &results);
  if (error) {
    GUDOV_LOG_ERROR(g_logger) << "IPAddress::Create(" << address << ", " << port
                              << ") error=" << error << " errno=" << errno
                              << " errstr=" << strerror(errno);
    return nullptr;
  }

  try {
    IPAddress::ptr result = std::dynamic_pointer_cast<IPAddress>(
        Address::Create(results->ai_addr, (socklen_t)results->ai_addrlen));
    if (result) {
      result->setPort(port);
    }
    freeaddrinfo(results);
    return result;
  } catch (...) {
    freeaddrinfo(results);
    return nullptr;
  }
}

IPv4Address::ptr IPv4Address::Create(const char* address, uint16_t port) {
  IPv4Address::ptr rt(new IPv4Address);
  rt->_addr.sin_port = ByteSwapOnLittleEndian(port);
  int result         = inet_pton(AF_INET, address, &rt->_addr.sin_addr);
  if (result <= 0) {
    GUDOV_LOG_ERROR(g_logger) << "IPv4Address::Create(" << address << ", "
                              << port << ") rt=" << result << " errno=" << errno
                              << " errstr=" << strerror(errno);
    return nullptr;
  }
  return rt;
}

IPv4Address::IPv4Address(const sockaddr_in& address) { _addr = address; }

IPv4Address::IPv4Address(uint32_t address, uint16_t port) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sin_family      = AF_INET;
  _addr.sin_port        = ByteSwapOnLittleEndian(port);
  _addr.sin_addr.s_addr = ByteSwapOnLittleEndian(address);
}

sockaddr* IPv4Address::getAddr() { return (sockaddr*)&_addr; }

const sockaddr* IPv4Address::getAddr() const { return (sockaddr*)&_addr; }

socklen_t IPv4Address::getAddrLen() const { return sizeof(_addr); }

std::ostream& IPv4Address::insert(std::ostream& os) const {
  uint32_t addr = ByteSwapOnLittleEndian(_addr.sin_addr.s_addr);
  os << "[IPv4 " << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "."
     << ((addr >> 8) & 0xff) << "." << (addr & 0xff) << ":"
     << ByteSwapOnLittleEndian(_addr.sin_port) << "]";
  return os;
}

IPAddress::ptr IPv4Address::broadcastAddress(uint32_t prefixLen) {
  if (prefixLen > 32) {
    return nullptr;
  }

  sockaddr_in broad(_addr);
  broad.sin_addr.s_addr |=
      ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefixLen));
  return IPv4Address::ptr(new IPv4Address(broad));
}

IPAddress::ptr IPv4Address::networkAddress(uint32_t prefixLen) {
  if (prefixLen > 32) {
    return nullptr;
  }

  sockaddr_in addr(_addr);
  addr.sin_addr.s_addr &=
      ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefixLen));
  return IPv4Address::ptr(new IPv4Address(addr));
}

IPAddress::ptr IPv4Address::subnetAddress(uint32_t prefixLen) {
  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr =
      ~ByteSwapOnLittleEndian(CreateMask<uint32_t>(prefixLen));
  return IPv4Address::ptr(new IPv4Address(addr));
}

uint32_t IPv4Address::getPort() const {
  return ByteSwapOnLittleEndian(_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
  _addr.sin_port = ByteSwapOnLittleEndian(v);
}

IPv6Address::ptr IPv6Address::Create(const char* address, uint16_t port) {
  IPv6Address::ptr rt(new IPv6Address);
  rt->_addr.sin6_port = ByteSwapOnLittleEndian(port);
  int result          = inet_pton(AF_INET6, address, &rt->_addr.sin6_addr);
  if (result <= 0) {
    GUDOV_LOG_ERROR(g_logger) << "IPv6Address::Create(" << address << ", "
                              << port << ") rt=" << result << " errno=" << errno
                              << " errstr=" << strerror(errno);
    return nullptr;
  }
  return rt;
}

IPv6Address::IPv6Address() {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address) { _addr = address; }

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sin6_family = AF_INET6;
  _addr.sin6_port   = ByteSwapOnLittleEndian(port);
  memcpy(&_addr.sin6_addr.s6_addr, address, 16);
}

sockaddr* IPv6Address::getAddr() { return (sockaddr*)&_addr; }

const sockaddr* IPv6Address::getAddr() const { return (sockaddr*)&_addr; }

socklen_t IPv6Address::getAddrLen() const { return sizeof(_addr); }

std::ostream& IPv6Address::insert(std::ostream& os) const {
  os << "[IPv6 [";
  uint16_t* addr      = (uint16_t*)_addr.sin6_addr.s6_addr;
  bool      usedZeros = false;
  for (size_t i = 0; i < 8; ++i) {
    if (addr[i] == 0 && !usedZeros) {
      continue;
    }
    if (i && addr[i - 1] == 0 && !usedZeros) {
      os << ":";
      usedZeros = true;
    }
    if (i) {
      os << ":";
    }
    os << std::hex << (int)ByteSwapOnLittleEndian(addr[i]) << std::dec;
  }
  if (!usedZeros && addr[7] == 0) {
    os << "::";
  }

  os << "]:" << ByteSwapOnLittleEndian(_addr.sin6_port);
  os << "]";
  return os;
}

IPAddress::ptr IPv6Address::broadcastAddress(uint32_t prefixLen) {
  sockaddr_in6 addr(_addr);
  addr.sin6_addr.s6_addr[prefixLen / 8] |= CreateMask<uint8_t>(prefixLen % 8);
  for (int i = prefixLen / 8 + 1; i < 16; ++i) {
    addr.sin6_addr.s6_addr[i] = 0xff;
  }
  return IPv6Address::ptr(new IPv6Address(addr));
}

IPAddress::ptr IPv6Address::networkAddress(uint32_t prefixLen) {
  sockaddr_in6 addr(_addr);
  addr.sin6_addr.s6_addr[prefixLen / 8] &= CreateMask<uint8_t>(prefixLen % 8);
  for (int i = prefixLen / 8 + 1; i < 16; ++i) {
    addr.sin6_addr.s6_addr[i] = 0x00;
  }
  return IPv6Address::ptr(new IPv6Address(addr));
}

IPAddress::ptr IPv6Address::subnetAddress(uint32_t prefixLen) {
  sockaddr_in6 addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin6_family                      = AF_INET6;
  addr.sin6_addr.s6_addr[prefixLen / 8] = ~CreateMask<uint8_t>(prefixLen % 8);

  for (uint32_t i = 0; i < prefixLen / 8; ++i) {
    addr.sin6_addr.s6_addr[i] = 0xff;
  }
  return IPv6Address::ptr(new IPv6Address(addr));
}

uint32_t IPv6Address::getPort() const {
  return ByteSwapOnLittleEndian(_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
  _addr.sin6_port = ByteSwapOnLittleEndian(v);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)0)->sun_path) - 1;

UnixAddress::UnixAddress() {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sun_family = AF_UNIX;
  _length          = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sun_family = AF_UNIX;
  _length          = path.size() + 1;

  if (!path.empty() && path[0] == '\0') {
    --_length;
  }

  if (_length > sizeof(_addr.sun_path)) {
    throw std::logic_error("path too long");
  }
  memcpy(_addr.sun_path, path.c_str(), _length);
  _length += offsetof(sockaddr_un, sun_path);
}

void UnixAddress::setAddrLen(uint32_t v) { _length = v; }

sockaddr* UnixAddress::getAddr() { return (sockaddr*)&_addr; }

const sockaddr* UnixAddress::getAddr() const { return (sockaddr*)&_addr; }

socklen_t UnixAddress::getAddrLen() const { return _length; }

std::ostream& UnixAddress::insert(std::ostream& os) const {
  if (_length > offsetof(sockaddr_un, sun_path) && _addr.sun_path[0] == '\0') {
    return os << "\\0"
              << std::string(_addr.sun_path + 1,
                             _length - offsetof(sockaddr_un, sun_path) - 1);
  }
  return os << _addr.sun_path;
}

UnknownAddress::UnknownAddress(int family) {
  memset(&_addr, 0, sizeof(_addr));
  _addr.sa_family = family;
}

UnknownAddress::UnknownAddress(const sockaddr& addr) { _addr = addr; }

sockaddr* UnknownAddress::getAddr() { return &_addr; }

const sockaddr* UnknownAddress::getAddr() const { return &_addr; }

socklen_t UnknownAddress::getAddrLen() const { return sizeof(_addr); }

std::ostream& UnknownAddress::insert(std::ostream& os) const {
  os << "[UnknownAddress family=" << _addr.sa_family << "]";
  return os;
}

std::ostream& operator<<(std::ostream& os, const Address& addr) {
  return addr.insert(os);
}

}  // namespace gudov
