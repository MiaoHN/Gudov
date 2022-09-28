#ifndef __GUDOV_NONCOPYABLE_H__
#define __GUDOV_NONCOPYABLE_H__

namespace gudov {

class NonCopyable {
 public:
  NonCopyable()  = default;
  ~NonCopyable() = default;

  NonCopyable(const NonCopyable&) = delete;

  NonCopyable& operator=(const NonCopyable&) = delete;
};

}  // namespace gudov

#endif  // __GUDOV_NONCOPYABLE_H__