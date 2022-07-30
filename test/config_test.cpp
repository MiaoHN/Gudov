#include "gudov/config.h"

#include <yaml-cpp/exceptions.h>
#include <yaml-cpp/node/node.h>
#include <yaml-cpp/node/parse.h>
#include <yaml-cpp/yaml.h>

#include "gudov/log.h"

gudov::ConfigVar<int>::ptr g_int_value_config =
    gudov::Config::Lookup("system.port", (int)8080, "system port");

gudov::ConfigVar<float>::ptr g_float_value_config =
    gudov::Config::Lookup("system.value", (float)10.2f, "system value");

void printYaml(const YAML::Node& node, int level) {
  if (node.IsScalar()) {
    GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
        << std::string(level * 4, ' ') << node.Scalar() << " - " << node.Type()
        << " - " << level;
  } else if (node.IsNull()) {
    GUDOV_LOG_INFO(GUDOV_LOG_ROOT()) << std::string(level * 4, ' ') << "NULL - "
                                     << node.Type() << " - " << level;
  } else if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
          << std::string(level * 4, ' ') << it->first << " - "
          << it->second.Type() << " - " << level;
      printYaml(it->second, level + 1);
    }
  } else if (node.IsSequence()) {
    for (size_t i = 0; i < node.size(); ++i) {
      GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
          << std::string(level * 4, ' ') << i << " - " << node[i].Type()
          << " - " << level;
      printYaml(node[i], level + 1);
    }
  }
}

void testYaml() {
  YAML::Node root = YAML::LoadFile("conf/log.yml");
  printYaml(root, 0);

  GUDOV_LOG_INFO(GUDOV_LOG_ROOT()) << root.Scalar();
}

void testConfig() {
  GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
      << "before: " << g_int_value_config->getValue();
  GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
      << "before: " << g_float_value_config->toString();

  YAML::Node root = YAML::LoadFile("conf/log.yml");
  gudov::Config::LoadFromYaml(root);

  GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
      << "after: " << g_int_value_config->getValue();
  GUDOV_LOG_INFO(GUDOV_LOG_ROOT())
      << "after: " << g_float_value_config->toString();
}

int main(int argc, char* argv[]) {
  // testYaml();
  testConfig();
  return 0;
}
