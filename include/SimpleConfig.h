#pragma once
#include <string>
#include <unordered_map>

namespace pluginfw {

/**
 * @brief 简易 YAML 解析器，提供基础的配置文件读取和键值对解析功能
 * 
 * 该类仅支持极简的 YAML 格式，即每行 "key: value" 的形式，不支持嵌套、数组等复杂结构。
 * 主要用于插件的简单配置读取。
 */
class SimpleYaml
{
public:
    /**
     * @brief 读取文本文件的全部内容
     * @param file_path 文件路径
     * @param out 输出参数，用于存储读取到的文件内容
     * @param err 输出参数，若读取失败则存储错误信息
     * @return 成功返回 true，失败返回 false
     */
    static bool readFileText(const std::string& file_path, std::string& out, std::string& err);

    /**
     * @brief 解析简化版 YAML 格式的文本，提取键值对
     * 
     * 解析规则：
     * - 按行分割文本
     * - 忽略空行和以 '#' 开头的注释行
     * - 每行格式为 "key: value"，key 和 value 前后的空白字符会被去除
     * - 若一行包含多个冒号，只取第一个冒号作为分隔符
     * 
     * @param text 包含 YAML 格式的文本内容
     * @return 键值对的无序映射，key 为字符串，value 为字符串
     */
    static std::unordered_map<std::string, std::string> parseKeyValueText(const std::string& text);
};

} // namespace pluginfw