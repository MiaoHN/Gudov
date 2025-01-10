# Gudov

![Travis (.com)](https://img.shields.io/travis/com/miaohn/gudov) ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/miaohn/gudov/cmake)
 ![GitHub License](https://img.shields.io/github/license/miaohn/gudov)

学习 sylar 高性能服务器的代码，对应[视频地址](https://www.bilibili.com/video/av53602631)。该框架实现了 M 对 N 的线程-协程调度模块，借助 epoll 实现定时器，并通过 hook 系统调用实现了较高的性能。

## 第三方库

如果是 Ubuntu 系统，需要先下载以下依赖库，其他分发版请自行下载对应的库文件。

```bash
apt-get install libyaml-cpp-dev libboost-dev ragel
```

## 参考

- [sylar](https://github.com/sylar-yin/sylar)
- [从零开始重写sylar C++高性能分布式服务器框架](https://www.midlane.top/wiki/pages/viewpage.action?pageId=10060952)
