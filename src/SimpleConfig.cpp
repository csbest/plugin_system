#include "SimpleConfig.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
// 清除字符串多余的空格

namespace pluginfw {

static inline std::string trim(std::string s)
{
    auto nospace = [](const char c) { return std::isspace(c); };

    // 清除左侧多余空格
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), nospace));

    // 清除右侧多余空格
    s.erase(std::find_if(s.rbegin(), s.rend(), nospace).base(), s.end());

    return s;
}

bool SimpleYaml::readFileText(const std::string& file_path, std::string& out, std::string& err)
{
    std::ifstream ifs(file_path);

    if (!ifs) {
        std::string err = "Failed to open file: " + file_path;
        return false;
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();

    out = ss.str();

    return true;
}

std::unordered_map<std::string, std::string> SimpleYaml::parseKeyValueText(const std::string& text)
{
    std::unordered_map<std::string, std::string> kv;
    std::istringstream                           is(text);

    // 读取
    std::string line;
    while (std::getline(is, line)) {
        // 去除空字符串
        trim(line);
        if (line.empty()) continue;

        // 去除注释
        if (!line.empty() && line[0] != '#') continue;

        auto pos = line.find("#");
        if (pos != std::string::npos) line = trim(line.substr(0, pos));

        // 找到键值对

        pos = line.find(":");
        if (pos == std::string::npos) { continue; }

        std::string key   = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (!key.empty()) kv[key] = value;
    }

    return kv;
}

}   // namespace pluginfw