#include "gudov/address.h"
#include "gudov/log.h"

gudov::Logger::ptr g_logger = GUDOV_LOG_ROOT();

void test() {
  std::vector<gudov::Address::ptr> addrs;

  GUDOV_LOG_INFO(g_logger) << "begin";
  bool v = gudov::Address::Lookup(addrs, "www.miaohn.top");
  GUDOV_LOG_INFO(g_logger) << "end";
  if (!v) {
    GUDOV_LOG_ERROR(g_logger) << "lookup fail";
    return;
  }

  for (size_t i = 0; i < addrs.size(); ++i) {
    GUDOV_LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
  }
}

void testIface() {
  std::multimap<std::string, std::pair<gudov::Address::ptr, uint32_t> > results;

  bool v = gudov::Address::GetInterfaceAddress(results);
  if (!v) {
    GUDOV_LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
    return;
  }

  for (auto& i : results) {
    GUDOV_LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString()
                             << " - " << i.second.second;
  }
}

void testIpv4() {
  // auto addr = gudov::IPAddress::Create("www.gudov.top");
  auto addr = gudov::IPAddress::Create("127.0.0.8");
  if (addr) {
    GUDOV_LOG_INFO(g_logger) << addr->toString();
  }
}

int main(int argc, char** argv) {
  testIpv4();
  testIface();
  test();
  return 0;
}