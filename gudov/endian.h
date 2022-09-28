#ifndef __GUDOV_ENDIAN_H__
#define __GUDOV_ENDIAN_H__

#define GUDOV_LITTLE_ENDIAN 1
#define GUDOV_BIG_ENDIAN    2

#include <byteswap.h>

namespace gudov {

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type ByteSwap(
    T value) {
  return (T)bswap_64((uint64_t)value);
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type ByteSwap(
    T value) {
  return (T)bswap_32((uint32_t)value);
}

template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type ByteSwap(
    T value) {
  return (T)bswap_16((uint16_t)value);
}

#if BYTE_ORDER == BIG_ENDIAN
#define GUDOV_BYTE_ORDER GUDOV_BIG_ENDIAN
#else
#define GUDOV_BYTE_ORDER GUDOV_LITTLE_ENDIAN
#endif

#if GUDOV_BYTE_ORDER == GUDOV_BIG_ENDIAN

template <class T>
T ByteSwapOnLittleEndian(T t) {
  return t;
}

template <class T>
T ByteSwapOnBigEndian(T t) {
  return ByteSwap(t);
}

#else

template <class T>
T ByteSwapOnLittleEndian(T t) {
  return ByteSwap(t);
}

template <class T>
T ByteSwapOnBigEndian(T t) {
  return t;
}

#endif

}  // namespace gudov

#endif  // __GUDOV_ENDIAN_H__