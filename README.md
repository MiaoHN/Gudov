# Gudov

![Travis (.com)](https://img.shields.io/travis/com/miaohn/gudov)![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/miaohn/gudov/cmake)
![GitHub License](https://img.shields.io/github/license/miaohn/gudov)

Learn the code of the sylar high-performance server. The corresponding [video link](https://www.bilibili.com/video/av53602631). This framework implements an M-to-N thread-coroutine scheduling module, realizes timers with the help of epoll, and achieves high performance through a hook system call.

## Third-party Libraries

If you are using the Ubuntu system, you need to download the following dependent libraries first. For other distributions, please download the corresponding library files by yourself.

```bash
apt-get install libyaml-cpp-dev libboost-dev ragel
```

## Benchmark

We use the `wrk` tool to test the performance of the server. The test environment is as follows:

## References

- [sylar](https://github.com/sylar-yin/sylar)
- [从零开始重写sylar C++高性能分布式服务器框架](https://www.midlane.top/wiki/pages/viewpage.action?pageId=10060952)
