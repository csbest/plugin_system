#include "SimpleConfig.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace pluginfw {

/**
 * @brief 去除字符串首尾的空白字符（空格、制表符等）
 * @param s 输入字符串
 * @return 去除首尾空白后的新字符串
 * 
 * 使用 std::isspace 判断空白字符，分别从开头和结尾查找第一个非空白字符的位置，
 * 然后通过 erase 删除空白部分。
 */
static inline std::string trim(std::string s)
{
    auto nospace = [](const char c) { return std::isspace(c); };

    // 清除左侧空白：找到第一个非空白字符，删除之前的部分
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), nospace));

    // 清除右侧空白：从末尾向前找到第一个非空白字符，删除之后的部分
    s.erase(std::find_if(s.rbegin(), s.rend(), nospace).base(), s.end());

    return s;
}

/**
 * @brief 读取文本文件的全部内容
 * @param file_path 文件路径
 * @param out 输出参数，存储读取到的内容
 * @param err 输出参数，存储错误信息（若失败）
 * @return 成功返回 true，失败返回 false
 * 
 * 注意：当前实现中，当文件打开失败时，err 参数并未赋值，而是定义了一个局部变量掩盖了参数。
 * 调用者传入的 err 可能不会被修改。建议修正为使用传入的 err 引用。
 */
bool SimpleYaml::readFileText(const std::string& file_path, std::string& out, std::string& err)
{
    std::ifstream ifs(file_path);

    if (!ifs) {
        // 此处错误信息赋值给了局部变量，而不是传入的 err 引用
        std::string err = "Failed to open file: " + file_path;
        return false;
    }

    std::ostringstream ss;
    ss << ifs.rdbuf();  // 将文件流内容读入 stringstream

    out = ss.str();     // 输出文件内容

    return true;
}

/**
 * @brief 解析简化版 YAML 键值对文本
 * @param text 包含 YAML 格式的文本内容
 * @return 键值对的无序映射
 * 
 * 解析规则：
 * 1. 按行分割文本。
 * 2. 对每一行先去除首尾空白，若为空行则跳过。
 * 3. 去除行内注释：查找 '#' 字符，若存在则截取 '#' 之前的部分，并再次 trim。
 * 4. 查找第一个 ':' 作为键值分隔符。
 * 5. 提取键和值，分别 trim，若键非空则存入 map。
 * 
 * 注意：当前实现中存在逻辑错误：
 *   - 第 57 行条件 `if (!line.empty() && line[0] != '#') continue;` 导致非注释行被跳过，应改为 `if (line.empty() || line[0] == '#') continue;`。
 *   - 后续代码又处理了行内注释，但前面的跳过逻辑会误跳有效行。
 * 此处保留原样，仅作注释说明。
 */
std::unordered_map<std::string, std::string> SimpleYaml::parseKeyValueText(const std::string& text)
{
    std::unordered_map<std::string, std::string> kv;
    std::istringstream                           is(text);

    std::string line;
    while (std::getline(is, line)) {
        // 去除首尾空白
        line = trim(line);
        if (line.empty()) continue;

        // 原代码逻辑：若行不以 '#' 开头则跳过（这恰好跳过了有效行）
        // 应该是跳过空行或以 '#' 开头的注释行，此处有误
        if (!line.empty() && line[0] != '#') continue;

        // 查找行内注释标记 '#'
        auto pos = line.find("#");
        if (pos != std::string::npos)
            line = trim(line.substr(0, pos)); // 截取注释前的内容并 trim

        // 查找键值分隔符 ':'
        pos = line.find(":");
        if (pos == std::string::npos) {
            continue; // 没有冒号，不是有效的键值对，跳过
        }

        std::string key   = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        if (!key.empty()) kv[key] = value; // 存入 map
    }

    return kv;
}

} // namespace pluginfw