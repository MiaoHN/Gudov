#ifndef __GUDOV_SINGLETON_H__
#define __GUDOV_SINGLETON_H__

#include <memory>

namespace gudov {

template <class T, class X = void, int N = 0>
class Singleton {
 public:
  static T* getInstance() {
    static T v;
    return &v;
  }
};

template <class T, class X = void, int N = 0>
class SingletonPtr {
 public:
  static std::shared_ptr<T> getInstance() {
    static std::shared_ptr<T> v(new T);
    return v;
  }
};

}  // namespace gudov

#endif  // __GUDOV_SINGLETON_H__
