#pragma once

#include <cxxabi.h>  // for abi::__cxa_demangle()
#include <pthread.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace gudov {

pid_t    GetThreadId();
uint32_t GetFiberId();

void BackTrace(std::vector<std::string> &bt, int size = 64, int skip = 1);

std::string BacktraceToString(int size = 64, int skip = 2, const std::string &prefix = "");

uint64_t GetCurrentMS();

uint64_t GetCurrentUS();

/**
 * @brief 文件系统操作类
 */
class FSUtil {
 public:
  /**
   * @brief 递归列举指定目录下所有指定后缀的常规文件，如果不指定后缀，则遍历所有文件，返回的文件名带路径
   * @param[out] files 文件列表
   * @param[in] path 路径
   * @param[in] subfix 后缀名，比如 ".yml"
   */
  static void ListAllFile(std::vector<std::string> &files, const std::string &path, const std::string &subfix);

  /**
   * @brief 创建路径，相当于mkdir -p
   * @param[in] dirname 路径名
   * @return 创建是否成功
   */
  static bool Mkdir(const std::string &dirname);

  /**
   * @brief 判断指定pid文件指定的pid是否正在运行，使用kill(pid, 0)的方式判断
   * @param[in] pidfile 保存进程号的文件
   * @return 是否正在运行
   */
  static bool IsRunningPidfile(const std::string &pidfile);

  /**
   * @brief 删除文件或路径
   * @param[in] path 文件名或路径名
   * @return 是否删除成功
   */
  static bool Rm(const std::string &path);

  /**
   * @brief 移动文件或路径，内部实现是先Rm(to)，再rename(from, to)，参考rename
   * @param[in] from 源
   * @param[in] to 目的地
   * @return 是否成功
   */
  static bool Mv(const std::string &from, const std::string &to);

  /**
   * @brief 返回绝对路径，参考realpath(3)
   * @details 路径中的符号链接会被解析成实际的路径，删除多余的'.' '..'和'/'
   * @param[in] path
   * @param[out] rpath
   * @return  是否成功
   */
  static bool Realpath(const std::string &path, std::string &rpath);

  /**
   * @brief 创建符号链接，参考symlink(2)
   * @param[in] from 目标
   * @param[in] to 链接路径
   * @return  是否成功
   */
  static bool Symlink(const std::string &from, const std::string &to);

  /**
   * @brief 删除文件，参考unlink(2)
   * @param[in] filename 文件名
   * @param[in] exist 是否存在
   * @return  是否成功
   * @note 内部会判断一次是否真的不存在该文件
   */
  static bool Unlink(const std::string &filename, bool exist = false);

  /**
   * @brief 返回文件，即路径中最后一个/前面的部分，不包括/本身，如果未找到，则返回filename
   * @param[in] filename 文件完整路径
   * @return  文件路径
   */
  static std::string Dirname(const std::string &filename);

  /**
   * @brief 返回文件名，即路径中最后一个/后面的部分
   * @param[in] filename 文件完整路径
   * @return  文件名
   */
  static std::string Basename(const std::string &filename);

  /**
   * @brief 以只读方式打开
   * @param[in] ifs 文件流
   * @param[in] filename 文件名
   * @param[in] mode 打开方式
   * @return  是否打开成功
   */
  static bool OpenForRead(std::ifstream &ifs, const std::string &filename, std::ios_base::openmode mode);

  /**
   * @brief 以只写方式打开
   * @param[in] ofs 文件流
   * @param[in] filename 文件名
   * @param[in] mode 打开方式
   * @return  是否打开成功
   */
  static bool OpenForWrite(std::ofstream &ofs, const std::string &filename, std::ios_base::openmode mode);

  static std::string ReadFile(const std::string &filename) {
    std::ifstream f(filename);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  }
};

}  // namespace gudov
