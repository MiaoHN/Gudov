#ifndef __GUDOV_CONFIG_H__
#define __GUDOV_CONFIG_H__

#include <yaml-cpp/yaml.h>

#include <boost/lexical_cast.hpp>
#include <memory>
#include <sstream>
#include <string>

#include "gudov/log.h"

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

  virtual std::string toString() = 0;
  virtual bool fromString(const std::string& val) = 0;

 private:
  std::string name_;
  std::string description_;
};

template <typename T>
class ConfigVar : public ConfigVarBase {
 public:
  using ptr = std::shared_ptr<ConfigVar>;

  ConfigVar(const std::string& name, const T& defaultValue,
            const std::string& description = "")
      : ConfigVarBase(name, description), val_(defaultValue) {}

  std::string toString() override {
    try {
      return boost::lexical_cast<std::string>(val_);
    } catch (std::exception& e) {
      GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())
          << "ConfigVar::toString exception" << e.what()
          << " convert: " << typeid(val_).name() << " to string";
    }
    return "";
  }

  bool fromString(const std::string& val) override {
    try {
      val_ = boost::lexical_cast<T>(val);
    } catch (std::exception& e) {
      GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())
          << "ConfigVar::toString exception" << e.what()
          << " convert: string to " << typeid(val_).name();
    }
    return false;
  };

  const T getValue() const { return val_; }
  void setValue(const T& v) { val_ = v; }

 private:
  T val_;
};

class Config {
 public:
  using ConfigVarMap = std::map<std::string, ConfigVarBase::ptr>;

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(
      const std::string& name, const T& defaultValue,
      const std::string& description = "") {
    auto tmp = Lookup<T>(name);
    if (tmp) {
      GUDOV_LOG_INFO(GUDOV_LOG_ROOT()) << "Lookup name=" << name << " exists";
      return tmp;
    }

    if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") !=
        std::string::npos) {
      GUDOV_LOG_ERROR(GUDOV_LOG_ROOT()) << "Lookup name invalid";
      throw std::invalid_argument(name);
    }

    typename ConfigVar<T>::ptr v(
        new ConfigVar<T>(name, defaultValue, description));
    s_datas_[name] = v;
    return v;
  }

  template <typename T>
  static typename ConfigVar<T>::ptr Lookup(const std::string& name) {
    auto it = s_datas_.find(name);
    if (it == s_datas_.end()) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
  }

  static void LoadFromYaml(const YAML::Node& root);
  static ConfigVarBase::ptr LookupBase(const std::string& name);

 private:
  static ConfigVarMap s_datas_;
};

}  // namespace gudov

#endif  // __GUDOV_CONFIG_H__
