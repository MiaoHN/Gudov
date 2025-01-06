#include <gtest/gtest.h>
#include <yaml-cpp/yaml.h>

#include "gudov/config.h"
#include "gudov/log.h"

using namespace gudov;

void print_yaml(const YAML::Node& node, int level) {
  if (node.IsScalar()) {
    LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ') << node.Scalar() << " - " << node.Type() << " - " << level;
  } else if (node.IsNull()) {
    LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ') << "NULL - " << node.Type() << " - " << level;
  } else if (node.IsMap()) {
    for (auto it = node.begin(); it != node.end(); ++it) {
      LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ') << it->first << " - " << it->second.Type() << " - " << level;
      print_yaml(it->second, level + 1);
    }
  } else if (node.IsSequence()) {
    for (size_t i = 0; i < node.size(); ++i) {
      LOG_INFO(LOG_ROOT()) << std::string(level * 4, ' ') << i << " - " << node[i].Type() << " - " << level;
      print_yaml(node[i], level + 1);
    }
  }
}

TEST(ConfigTest, LoadYAML) {
  YAML::Node root = YAML::LoadFile("./conf/log.yml");

  EXPECT_TRUE(root["logs"].IsDefined());
}

TEST(ConfigTest, ConfigValuesBeforeAndAfter) {
  Config::Clear();

  ConfigVar<int>::ptr   g_int_value_config   = Config::Lookup("system.port", (int)8080, "system port");
  ConfigVar<float>::ptr g_float_value_config = Config::Lookup("system.value", (float)10.2f, "system value");

  EXPECT_EQ(g_int_value_config->GetValue(), 8080);
  EXPECT_FLOAT_EQ(g_float_value_config->GetValue(), 10.2f);

  YAML::Node root = YAML::LoadFile("./conf/test.yml");
  Config::LoadFromYaml(root);

  EXPECT_NE(g_int_value_config->GetValue(), 8080);
  EXPECT_NE(g_float_value_config->GetValue(), 10.2f);
}

class Person {
 public:
  Person(){};
  std::string name_;
  int         m_age = 0;
  bool        m_sex = 0;

  std::string ToString() const {
    std::stringstream ss;
    ss << "[Person name=" << name_ << " age=" << m_age << " sex=" << m_sex << "]";
    return ss.str();
  }

  bool operator==(const Person& oth) const { return name_ == oth.name_ && m_age == oth.m_age && m_sex == oth.m_sex; }
};

namespace gudov {

template <>
class LexicalCast<std::string, Person> {
 public:
  Person operator()(const std::string& v) {
    YAML::Node node = YAML::Load(v);
    Person     p;
    p.name_ = node["name"].as<std::string>();
    p.m_age  = node["age"].as<int>();
    p.m_sex  = node["sex"].as<bool>();
    return p;
  }
};

template <>
class LexicalCast<Person, std::string> {
 public:
  std::string operator()(const Person& p) {
    YAML::Node node;
    node["name"] = p.name_;
    node["age"]  = p.m_age;
    node["sex"]  = p.m_sex;
    std::stringstream ss;
    ss << node;
    return ss.str();
  }
};

}  // namespace gudov

TEST(ClassTest, BeforeAndAfter) {
  Config::Clear();

  ConfigVar<Person>::ptr                        g_person = Config::Lookup("class.person", Person(), "system person");
  ConfigVar<std::map<std::string, Person>>::ptr g_person_map =
      Config::Lookup("class.map", std::map<std::string, Person>(), "system person");
  ConfigVar<std::map<std::string, std::vector<Person>>>::ptr g_person_vec_map =
      Config::Lookup("class.vec_map", std::map<std::string, std::vector<Person>>(), "system person");

  EXPECT_EQ(g_person->GetValue().ToString(), "[Person name= age=0 sex=0]");
  EXPECT_EQ(g_person_map->GetValue().size(), 0);
  EXPECT_EQ(g_person_vec_map->GetValue().size(), 0);

  // Assuming test.yml contains updated values for these variables
  YAML::Node root = YAML::LoadFile("./conf/test.yml");
  Config::LoadFromYaml(root);

  EXPECT_NE(g_person->GetValue().ToString(), "[Person name=name1 age=10 sex=true]");
  EXPECT_NE(g_person_map->GetValue().size(), 0);
  EXPECT_NE(g_person_vec_map->GetValue().size(), 0);
}

TEST(LogTest, LogFunctionality) {
  static Logger::ptr system_log = LOG_NAME("system");
  testing::internal::CaptureStdout();
  LOG_INFO(system_log) << "hello system";
  std::string output = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(output.empty());

  system_log->setFormatter("%d - %m%n");
  testing::internal::CaptureStdout();
  LOG_INFO(system_log) << "hello system";
  output = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(output.empty());
}
