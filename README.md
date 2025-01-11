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

- Command

```bash
ab -c 10000 -t 10 "http://127.0.0.1:8888/"
```

- Result

```txt
Server Software:        gudov/1.0.0
Server Hostname:        127.0.0.1
Server Port:            8888

Document Path:          /
Document Length:        267 bytes

Concurrency Level:      10000
Time taken for tests:   7.052 seconds
Complete requests:      50000
Failed requests:        0
Total transferred:      17350000 bytes
HTML transferred:       13350000 bytes
Requests per second:    7089.97 [#/sec] (mean)
Time per request:       1410.443 [ms] (mean)
Time per request:       0.141 [ms] (mean, across all concurrent requests)
Transfer rate:          2402.56 [Kbytes/sec] received

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0  561 200.2    610     914
Processing:   142  685 214.9    721    1068
Waiting:      118  511 177.1    536     880
Total:        737 1245 234.8   1292    1614

Percentage of the requests served within a certain time (ms)
  50%   1292
  66%   1360
  75%   1393
  80%   1480
  90%   1568
  95%   1589
  98%   1594
  99%   1595
 100%   1614 (longest request)
```

## References

- [sylar](https://github.com/sylar-yin/sylar)
- [从零开始重写sylar C++高性能分布式服务器框架](https://www.midlane.top/wiki/pages/viewpage.action?pageId=10060952)
