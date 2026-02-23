#pragma once
#include <string>
#include <unordered_map>

namespace pluginfw {

class SimpleYaml
{
public:
    // 读取文件内容
    static bool readFileText(const std::string& file_path, std::string& out, std::string& err);
    // 解析简化版 YAML
    static std::unordered_map<std::string, std::string> parseKeyValueText(const std::string& text);
};

}   // namespace pluginfw