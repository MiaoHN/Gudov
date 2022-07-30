#include "gudov/config.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace gudov {

ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
  auto it = GetDatas().find(name);
  return it == GetDatas().end() ? nullptr : it->second;
}

static void ListAllMember(
    const std::string& prefix, const YAML::Node& node,
    std::list<std::pair<std::string, const YAML::Node>>& output) {
  if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") !=
      std::string::npos) {
    GUDOV_LOG_ERROR(GUDOV_LOG_ROOT())
        << "Config invalid name: " << prefix << " : " << node;
    return;
  }
  output.push_back(std::make_pair(prefix, node));
  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      ListAllMember(prefix.empty() ? it->first.Scalar()
                                   : prefix + "." + it->first.Scalar(),
                    it->second, output);
    }
  }
}

void Config::LoadFromYaml(const YAML::Node& root) {
  std::list<std::pair<std::string, const YAML::Node>> allNodes;
  ListAllMember("", root, allNodes);

  for (auto& node : allNodes) {
    std::string key = node.first;
    if (key.empty()) {
      continue;
    }

    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    ConfigVarBase::ptr var = LookupBase(key);

    if (var) {
      if (node.second.IsScalar()) {
        var->fromString(node.second.Scalar());
      } else {
        std::stringstream ss;
        ss << node.second;
        var->fromString(ss.str());
      }
    }
  }
}

}  // namespace gudov
