#pragma once

#include <yaml-cpp/yaml.h>

#include <boost/lexical_cast.hpp>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "log.h"
#include "thread.h"

namespace gudov {

/**
 * @brief 配置项基类，包含公共方法
 *
 */
class ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVarBase>;
  ConfigVarBase(const std::string& name, const std::string& description = "")
      : name_(name), m_description(description) {
    std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);
  }
  virtual ~ConfigVarBase() {}

  const std::string& GetName() const { return name_; }
  const std::string& getDescription() const { return m_description; }

  virtual std::string ToString()                         = 0;
  virtual bool        fromString(const std::string& val) = 0;
  virtual std::string getTypeName() const                = 0;

 private:
  std::string name_;
  std::string m_description;
};

/**
 * @brief 通过语义转换将类型 F 的变量转换为类型 T
 * @attention 该转换中 F 和 T 必有一个是字符串类型
 * @details 本函数借助泛化实现了字符串与 vector, list, set, unordered_set, map,
 * unordered_map 等的互相转换，其余类型使用 boost 提供的 `lexical_cast` 进行转换
 *
 * @tparam F
 * @tparam T
 */
template <typename F, typename T>
class LexicalCast {
 public:
  T operator()(const F& v) { return boost::lexical_cast<T>(v); }
};

template <typename T>
class LexicalCast<std::string, std::vector<T>> {
 public:
  std::vector<T> operator()(const std::string& v) {
    YAML::Node              node = YAML::Load(v);
    typename std::vector<T> vec;
    std::stringstream       ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str();
      ss << node[i];
      vec.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

template <class T>
class LexicalCast<std::vector<T>, std::string> {
 public:
  std::string operator()(const std::vector<T>& v) {
    YAML::Node node;
    for (auto& i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

template <class T>
class LexicalCast<std::string, std::list<T>> {
 public:
  std::list<T> operator()(const std::string& v) {
    YAML::Node            node = YAML::Load(v);
    typename std::list<T> vec;
    std::stringstream     ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.push_back(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

template <class T>
class LexicalCast<std::list<T>, std::string> {
 public:
  std::string operator()(const std::list<T>& v) {
    YAML::Node node;
    for (auto& i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

template <class T>
class LexicalCast<std::string, std::set<T>> {
 public:
  std::set<T> operator()(const std::string& v) {
    YAML::Node           node = YAML::Load(v);
    typename std::set<T> vec;
    std::stringstream    ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.insert(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

template <class T>
class LexicalCast<std::set<T>, std::string> {
 public:
  std::string operator()(const std::set<T>& v) {
    YAML::Node node;
    for (auto& i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

template <class T>
class LexicalCast<std::string, std::unordered_set<T>> {
 public:
  std::unordered_set<T> operator()(const std::string& v) {
    YAML::Node                     node = YAML::Load(v);
    typename std::unordered_set<T> vec;
    std::stringstream              ss;
    for (size_t i = 0; i < node.size(); ++i) {
      ss.str("");
      ss << node[i];
      vec.insert(LexicalCast<std::string, T>()(ss.str()));
    }
    return vec;
  }
};

template <class T>
class LexicalCast<std::unordered_set<T>, std::string> {
 public:
  std::string operator()(const std::unordered_set<T>& v) {
    YAML::Node node;
    for (auto& i : v) {
      node.push_back(YAML::Load(LexicalCast<T, std::string>()(i)));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

template <class T>
class LexicalCast<std::string, std::map<std::string, T>> {
 public:
  std::map<std::string, T> operator()(const std::string& v) {
    YAML::Node                        node = YAML::Load(v);
    typename std::map<std::string, T> vec;
    std::stringstream                 ss;
    for (auto it = node.begin(); it != node.end(); ++it) {
      ss.str("");
      ss << it->second;
      vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
    }
    return vec;
  }
};

template <class T>
class LexicalCast<std::map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::map<std::string, T>& v) {
    YAML::Node node;
    for (auto& i : v) {
      node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

template <class T>
class LexicalCast<std::string, std::unordered_map<std::string, T>> {
 public:
  std::unordered_map<std::string, T> operator()(const std::string& v) {
    YAML::Node                                  node = YAML::Load(v);
    typename std::unordered_map<std::string, T> vec;
    std::stringstream                           ss;
    for (auto it = node.begin(); it != node.end(); ++it) {
      ss.str("");
      ss << it->second;
      vec.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
    }
    return vec;
  }
};

template <class T>
class LexicalCast<std::unordered_map<std::string, T>, std::string> {
 public:
  std::string operator()(const std::unordered_map<std::string, T>& v) {
    YAML::Node node;
    for (auto& i : v) {
      node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second));
    }
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

/**
 * @brief 配置项封装
 * @details 配置项包括配置名称，配置描述以及对应的值
 *
 * @tparam T 配置项类型
 */
template <typename T, typename FromStr = LexicalCast<std::string, T>, typename ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
 public:
  using RWMutexType      = RWMutex;
  using ptr              = std::shared_ptr<ConfigVar>;
  using onChangeCallback = std::function<void(const T& oldValue, const T& newValue)>;

  ConfigVar(const std::string& name, const T& default_value, const std::string& description = "")
      : ConfigVarBase(name, description), m_value(default_value) {}

  std::string ToString() override {
    try {
      RWMutexType::ReadLock lock(mutex_);
      return ToStr()(m_value);
    } catch (std::exception& e) {
      LOG_ERROR(LOG_ROOT()) << "ConfigVar::ToString exception" << e.what() << " convert: " << typeid(m_value).name()
                            << " to string";
    }
    return "";
  }

  bool fromString(const std::string& val) override {
    try {
      setValue(FromStr()(val));
    } catch (std::exception& e) {
      LOG_ERROR(LOG_ROOT()) << "ConfigVar::ToString exception" << e.what() << " convert: string to "
                            << typeid(m_value).name() << " - " << val;
    }
    return false;
  };

  const T GetValue() {
    RWMutexType::ReadLock lock(mutex_);
    return m_value;
  }
  void setValue(const T& v) {
    {
      RWMutexType::ReadLock lock(mutex_);
      if (v == m_value) {
        return;
      }
      for (auto& i : m_callbacks) {
        i.second(m_value, v);
      }
    }
    RWMutexType::ReadLock lock(mutex_);
    m_value = v;
  }

  std::string getTypeName() const override { return typeid(T).name(); }

  uint64_t addListener(onChangeCallback callback) {
    static uint64_t        s_fun_id = 0;
    RWMutexType::WriteLock lock(mutex_);
    ++s_fun_id;
    m_callbacks[s_fun_id] = callback;
    return s_fun_id;
  }

  void delListener(uint64_t key) {
    RWMutexType::WriteLock lock(mutex_);
    m_callbacks.erase(key);
  }

  onChangeCallback getListener(uint64_t key) {
    RWMutexType::ReadLock lock(mutex_);
    auto                  it = m_callbacks.find(key);
    return it == m_callbacks.end() ? nullptr : it->second;
  }

  void clearListener() {
    RWMutexType::WriteLock lock(mutex_);
    m_callbacks.clear();
  }

 private:
  T                                    m_value;
  RWMutexType                          mutex_;
  std::map<uint64_t, onChangeCallback> m_callbacks;
};

class Config {
 public:
  using ConfigVarMap = std::unordered_map<std::string, ConfigVarBase::ptr>;
  using RWMutexType  = RWMutex;

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name, const T& default_value,
                                           const std::string& description = "") {
    RWMutexType::WriteLock lock(GetMutex());
    auto                   it = GetDatas().find(name);
    if (it != GetDatas().end()) {
      auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
      if (tmp) {
        LOG_DEBUG(LOG_ROOT()) << "Lookup name=" << name << " exists";
        return tmp;
      } else {
        LOG_DEBUG(LOG_ROOT()) << "Lookup name=" << name << " exists but type not " << typeid(T).name()
                              << " real_type=" << it->second->getTypeName() << " " << it->second->ToString();
        return nullptr;
      }
    }

    if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
      LOG_ERROR(LOG_ROOT()) << "Lookup name invalid";
      throw std::invalid_argument(name);
    }

    typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
    GetDatas()[name] = v;
    return v;
  }

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
    RWMutexType::ReadLock lock(GetMutex());
    auto                  it = GetDatas().find(name);
    if (it == GetDatas().end()) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
  }

  static void               LoadFromYaml(const YAML::Node& root);
  static ConfigVarBase::ptr LookupBase(const std::string& name);

  static void Visit(std::function<void(ConfigVarBase::ptr)> callback);

  static void Clear() {
    RWMutexType::WriteLock lock(GetMutex());
    GetDatas().clear();
  }

 private:
  static ConfigVarMap& GetDatas() {
    static ConfigVarMap s_datas;
    return s_datas;
  }

  static RWMutexType& GetMutex() {
    static RWMutexType s_mutex;
    return s_mutex;
  }
};

}  // namespace gudov
