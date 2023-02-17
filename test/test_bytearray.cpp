#include "gudov/bytearray.h"
#include "gudov/gudov.h"

static gudov::Logger::ptr g_logger = LOG_ROOT();

void test() {
#define XX(type, len, writeFun, readFun, baseLen)               \
  {                                                             \
    std::vector<type> vec;                                      \
    for (int i = 0; i < len; ++i) {                             \
      vec.push_back(rand());                                    \
    }                                                           \
    gudov::ByteArray::ptr ba(new gudov::ByteArray(baseLen));    \
    for (auto& i : vec) {                                       \
      ba->writeFun(i);                                          \
    }                                                           \
    ba->setPosition(0);                                         \
    for (size_t i = 0; i < vec.size(); ++i) {                   \
      type v = ba->readFun();                                   \
      GUDOV_ASSERT(v == vec[i]);                                \
    }                                                           \
    GUDOV_ASSERT(ba->getReadSize() == 0);                       \
    LOG_INFO(g_logger)                                    \
        << #writeFun "/" #readFun " (" #type " ) len=" << len   \
        << " baseLen=" << baseLen << " size=" << ba->getSize(); \
  }

  XX(int8_t, 100, writeFint8, readFint8, 1);
  XX(uint8_t, 100, writeFuint8, readFuint8, 1);
  XX(int16_t, 100, writeFint16, readFint16, 1);
  XX(uint16_t, 100, writeFuint16, readFuint16, 1);
  XX(int32_t, 100, writeFint32, readFint32, 1);
  XX(uint32_t, 100, writeFuint32, readFuint32, 1);
  XX(int64_t, 100, writeFint64, readFint64, 1);
  XX(uint64_t, 100, writeFuint64, readFuint64, 1);

  XX(int32_t, 100, writeInt32, readInt32, 1);
  XX(uint32_t, 100, writeUint32, readUint32, 1);
  XX(int64_t, 100, writeInt64, readInt64, 1);
  XX(uint64_t, 100, writeUint64, readUint64, 1);
#undef XX

#define XX(type, len, writeFun, readFun, baseLen)                              \
  {                                                                            \
    std::vector<type> vec;                                                     \
    for (int i = 0; i < len; ++i) {                                            \
      vec.push_back(rand());                                                   \
    }                                                                          \
    gudov::ByteArray::ptr ba(new gudov::ByteArray(baseLen));                   \
    for (auto& i : vec) {                                                      \
      ba->writeFun(i);                                                         \
    }                                                                          \
    ba->setPosition(0);                                                        \
    for (size_t i = 0; i < vec.size(); ++i) {                                  \
      type v = ba->readFun();                                                  \
      GUDOV_ASSERT(v == vec[i]);                                               \
    }                                                                          \
    GUDOV_ASSERT(ba->getReadSize() == 0);                                      \
    LOG_INFO(g_logger)                                                   \
        << #writeFun "/" #readFun " (" #type " ) len=" << len                  \
        << " baseLen=" << baseLen << " size=" << ba->getSize();                \
    ba->setPosition(0);                                                        \
    GUDOV_ASSERT(ba->writeToFile("/tmp/" #type "_" #len "-" #readFun ".dat")); \
    gudov::ByteArray::ptr ba2(new gudov::ByteArray(baseLen * 2));              \
    GUDOV_ASSERT(                                                              \
        ba2->readFromFile("/tmp/" #type "_" #len "-" #readFun ".dat"));        \
    ba2->setPosition(0);                                                       \
    GUDOV_ASSERT(ba->toString() == ba2->toString());                           \
    GUDOV_ASSERT(ba->getPosition() == 0);                                      \
    GUDOV_ASSERT(ba2->getPosition() == 0);                                     \
  }
  XX(int8_t, 100, writeFint8, readFint8, 1);
  XX(uint8_t, 100, writeFuint8, readFuint8, 1);
  XX(int16_t, 100, writeFint16, readFint16, 1);
  XX(uint16_t, 100, writeFuint16, readFuint16, 1);
  XX(int32_t, 100, writeFint32, readFint32, 1);
  XX(uint32_t, 100, writeFuint32, readFuint32, 1);
  XX(int64_t, 100, writeFint64, readFint64, 1);
  XX(uint64_t, 100, writeFuint64, readFuint64, 1);

  XX(int32_t, 100, writeInt32, readInt32, 1);
  XX(uint32_t, 100, writeUint32, readUint32, 1);
  XX(int64_t, 100, writeInt64, readInt64, 1);
  XX(uint64_t, 100, writeUint64, readUint64, 1);

#undef XX
}

int main(int argc, char const* argv[]) {
  test();
  return 0;
}
