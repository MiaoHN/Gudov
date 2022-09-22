#ifndef __GUDOV_CONFIG_H__
#define __GUDOV_CONFIG_H__

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

class ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVarBase>;
  ConfigVarBase(const std::string& name, const std::string& description = "")
      : name_(name), description_(description) {
    std::transform(name_.begin(), name_.end(), name_.begin(), ::tolower);
  }
  virtual ~ConfigVarBase() {}

  const std::string& getName() const { return name_; }
  const std::string& getDescription() const { return description_; }

  virtual std::string toString()                         = 0;
  virtual bool        fromString(const std::string& val) = 0;
  virtual std::string getTypeName() const                = 0;

 private:
  std::string name_;
  std::string description_;
};

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
      vec.insert(std::make_pair(it->first.Scalar(),
                                LexicalCast<std::string, T>()(ss.str())));
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
      vec.insert(std::make_pair(it->first.Scalar(),
                                LexicalCast<std::string, T>()(ss.str())));
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

template <typename T, typename FromStr = LexicalCast<std::string, T>,
          typename ToStr = LexicalCast<T, std::string>>
class ConfigVar : public ConfigVarBase {
 public:
  using RWMutexType = RWMutex;
  using ptr         = std::shared_ptr<ConfigVar>;
  using onChangeCb  = std::function<void(const T& oldValue, const T& newValue)>;

  ConfigVar(const std::string& name, const T& defaultValue,
            const std::string& description = "")
      : ConfigVarBase(name, description), val_(defaultValue) {}

  std::string toString() override {
    try {
      RWMutexType::ReadLock lock(mutex_);
      return ToStr()(val_);
    } catch (std::exception& e) {
      GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())
          << "ConfigVar::toString exception" << e.what()
          << " convert: " << typeid(val_).name() << " to string";
    }
    return "";
  }

  bool fromString(const std::string& val) override {
    try {
      setValue(FromStr()(val));
    } catch (std::exception& e) {
      GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())
          << "ConfigVar::toString exception" << e.what()
          << " convert: string to " << typeid(val_).name() << " - " << val;
    }
    return false;
  };

  const T getValue() {
    RWMutexType::ReadLock lock(mutex_);
    return val_;
  }
  void setValue(const T& v) {
    {
      RWMutexType::ReadLock lock(mutex_);
      if (v == val_) {
        return;
      }
      for (auto& i : cbs_) {
        i.second(val_, v);
      }
    }
    RWMutexType::ReadLock lock(mutex_);
    val_ = v;
  }

  std::string getTypeName() const override { return typeid(T).name(); }

  uint64_t addListener(onChangeCb cb) {
    static uint64_t        s_funId = 0;
    RWMutexType::WriteLock lock(mutex_);
    ++s_funId;
    cbs_[s_funId] = cb;
    return s_funId;
  }

  void delListener(uint64_t key) {
    RWMutexType::WriteLock lock(mutex_);
    cbs_.erase(key);
  }

  onChangeCb getListener(uint64_t key) {
    RWMutexType::ReadLock lock(mutex_);
    auto                  it = cbs_.find(key);
    return it == cbs_.end() ? nullptr : it->second;
  }

  void clearListener() {
    RWMutexType::WriteLock lock(mutex_);
    cbs_.clear();
  }

 private:
  T                              val_;
  RWMutexType                    mutex_;
  std::map<uint64_t, onChangeCb> cbs_;
};

class Config {
 public:
  using ConfigVarMap = std::unordered_map<std::string, ConfigVarBase::ptr>;
  using RWMutexType  = RWMutex;

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(
      const std::string& name, const T& defaultValue,
      const std::string& description = "") {
    RWMutexType::WriteLock lock(GetMutex());
    auto                   it = GetDatas().find(name);
    if (it != GetDatas().end()) {
      auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
      if (tmp) {
        GUDOV_LOG_INFO(GUDOV_LOG_ROOT()) << "Lookup name=" << name << " exists";
        return tmp;
      } else {
        GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
            << "Lookup name=" << name << " exists but type not "
            << typeid(T).name() << " real_type=" << it->second->getTypeName()
            << " " << it->second->toString();
        return nullptr;
      }
    }

    if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") !=
        std::string::npos) {
      GUDOV_LOG_ERROR(GUDOV_LOG_ROOT()) << "Lookup name invalid";
      throw std::invalid_argument(name);
    }

    typename ConfigVar<T>::ptr v(
        new ConfigVar<T>(name, defaultValue, description));
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

  static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

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

#endif  // __GUDOV_CONFIG_H__
