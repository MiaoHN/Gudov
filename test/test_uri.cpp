#include <iostream>

#include "gudov/uri.h"

int main(int argc, char** argv) {
  gudov::Uri::ptr uri = gudov::Uri::Create("http://admin@www.baidu.top");
  std::cout << uri->toString() << std::endl;
  auto addr = uri->createAddress();
  std::cout << *addr << std::endl;
  return 0;
}