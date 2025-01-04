#include "gudov/address.h"
#include "gudov/log.h"

gudov::Logger::ptr g_logger = LOG_ROOT();

void test() {
  std::vector<gudov::Address::ptr> addrs;

  LOG_INFO(g_logger) << "begin";
  bool v = gudov::Address::Lookup(addrs, "www.miaohn.top");
  LOG_INFO(g_logger) << "end";
  if (!v) {
    LOG_ERROR(g_logger) << "lookup fail";
    return;
  }

  for (size_t i = 0; i < addrs.size(); ++i) {
    LOG_INFO(g_logger) << i << " - " << addrs[i]->toString();
  }
}

void testIface() {
  std::multimap<std::string, std::pair<gudov::Address::ptr, uint32_t> > results;

  bool v = gudov::Address::GetInterfaceAddress(results);
  if (!v) {
    LOG_ERROR(g_logger) << "GetInterfaceAddresses fail";
    return;
  }

  for (auto& i : results) {
    LOG_INFO(g_logger) << i.first << " - " << i.second.first->toString() << " - " << i.second.second;
  }
}

void testIpv4() {
  // auto addr = gudov::IPAddress::Create("www.gudov.top");
  auto addr = gudov::IPAddress::Create("127.0.0.8");
  if (addr) {
    LOG_INFO(g_logger) << addr->toString();
  }
}

int main(int argc, char** argv) {
  testIpv4();
  testIface();
  test();
  return 0;
}