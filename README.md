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

### Environment

- OS: Ubuntu 22.04 jammy(on the Windows Subsystem for Linux)
- Kernel: x86_64 Linux 5.15.167.4-microsoft-standard-WSL2
- CPU: Intel Core i5-9300H @ 8x 2.4GHz
- RAM: 8400MiB / 15934MiB

### Apache Benchmark

```bash
$ ab -c 1000 -n 200000 "http://127.0.0.1:8888/"
...
Concurrency Level:      1000
Time taken for tests:   14.682 seconds
Complete requests:      200000
Failed requests:        0
Total transferred:      69400000 bytes
HTML transferred:       53400000 bytes
Requests per second:    13621.94 [#/sec] (mean)
Time per request:       73.411 [ms] (mean)
Time per request:       0.073 [ms] (mean, across all concurrent requests)
Transfer rate:          4616.03 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    3   4.3      2      73
Processing:    26   70  15.8     66     151
Waiting:        0   69  15.6     65     150
Total:         53   73  16.1     68     151

Percentage of the requests served within a certain time (ms)
  50%     68
  66%     73
  75%     76
  80%     79
  90%     96
  95%    110
  98%    126
  99%    134
 100%    151 (longest request)
```

## References

- [sylar](https://github.com/sylar-yin/sylar)
- [从零开始重写sylar C++高性能分布式服务器框架](https://www.midlane.top/wiki/pages/viewpage.action?pageId=10060952)
