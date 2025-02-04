#include "gudov/config.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

#include "gudov/env.h"
#include "gudov/log.h"
#include "gudov/thread.h"

namespace gudov {

static Logger::ptr g_logger = LOG_NAME("system");

ConfigVarBase::ptr Config::LookupBase(const std::string& name) {
  RWMutexType::ReadLock lock(GetMutex());
  auto                  it = GetDatas().find(name);
  return it == GetDatas().end() ? nullptr : it->second;
}

static void ListAllMember(const std::string& prefix, const YAML::Node& node,
                          std::list<std::pair<std::string, const YAML::Node>>& output) {
  if (prefix.find_first_not_of("abcdefghijklmnopqrstuvwxyz._0123456789") != std::string::npos) {
    LOG_ERROR(LOG_ROOT()) << "Config invalid name: " << prefix << " : " << node;
    return;
  }
  output.push_back(std::make_pair(prefix, node));
  if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      ListAllMember(prefix.empty() ? it->first.Scalar() : prefix + "." + it->first.Scalar(), it->second, output);
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

/// 记录每个文件的修改时间
static std::map<std::string, uint64_t> s_file2modifytime;
/// 是否强制加载配置文件，非强制加载的情况下，如果记录的文件修改时间未变化，则跳过该文件的加载
static Mutex s_mutex;

void Config::LoadFromConfDir(const std::string& path, bool force) {
  std::string              absoulte_path = EnvMgr::GetInstance()->GetAbsolutePath(path);
  std::vector<std::string> files;
  FSUtil::ListAllFile(files, absoulte_path, ".yml");

  for (auto& i : files) {
    {
      struct stat st;
      lstat(i.c_str(), &st);
      Mutex::Locker lock(s_mutex);
      if (!force && s_file2modifytime[i] == (uint64_t)st.st_mtime) {
        continue;
      }
      s_file2modifytime[i] = st.st_mtime;
    }
    try {
      YAML::Node root = YAML::LoadFile(i);
      LoadFromYaml(root);
      LOG_INFO(g_logger) << "LoadConfFile file=" << i << " ok";
    } catch (...) {
      LOG_ERROR(g_logger) << "LoadConfFile file=" << i << " failed";
    }
  }
}

void Config::Visit(std::function<void(ConfigVarBase::ptr)> callback) {
  RWMutexType::ReadLock lock(GetMutex());
  ConfigVarMap&         m = GetDatas();
  for (auto it = m.begin(); it != m.end(); ++it) {
    callback(it->second);
  }
}

}  // namespace gudov
