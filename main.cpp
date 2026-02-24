#include <iostream>
#include <thread>

#include "PluginManager.h"

/**
 * @brief 状态变化回调函数，打印状态变化信息
 * @param alias 插件别名
 * @param oldstates 旧状态
 * @param newStates 新状态
 * @param msg 附加消息
 */
void printPluginStateconst(std::string alias, pluginfw::PluginState oldstates,
                           pluginfw::PluginState newStates, const std::string& msg)
{
    std::cout << "[状态] " << alias << " 从 " << pluginfw::to_string(oldstates) << " 变为 "
              << pluginfw::to_string(newStates) << " 消息: " << msg << std::endl;
}

int main()
{
    // 创建一个线程对象并立即 join，目的是强制链接 pthread 库
    // 如果不使用 std::thread，某些构建系统（如 CMake）可能会自动将线程库优化掉，
    // 导致链接时找不到 pthread 符号。此处保留可确保线程库被正确链接。
    std::thread test_thread([] { std::cout << "主程序测试线程运行中" << std::endl; });
    test_thread.join();

    // 创建插件管理器实例，传入状态回调函数
    pluginfw::PluginManager manager(printPluginStateconst);

    // 配置文件路径（YAML 格式）
    std::string yaml_config = "./config.yaml";

    // 插件别名和动态库路径
    std::string alias  = "MonitorPlugin";
    std::string soPath = "./lib/libCoreMonitor.so";

    // 注册插件（记录别名和路径，尚未加载）
    if (!manager.registerPlugin(alias, soPath)) {
        std::cerr << "注册失败" << std::endl;
        return 1;
    }

    // 加载插件（打开动态库，获取工厂函数）
    if (!manager.load(alias)) {
        std::cerr << "加载失败" << std::endl;
        return 1;
    }

    // 初始化插件（调用工厂创建实例并执行初始化）
    if (!manager.init(alias)) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }

    // 设置插件配置（传入配置文件路径）
    if (!manager.setPluginConfig(alias, yaml_config)) {
        std::cerr << "参数加载失败" << std::endl;
        return 1;
    }

    // 启动插件
    if (!manager.start(alias)) {
        std::cerr << "开始失败" << std::endl;
        return 1;
    }

    std::cout << "插件已启动，运行中... 按回车停止" << std::endl;
    std::cin.get();

    // 释放资源，在插件管理器的析构函数处，所以此处并无相关代码

    return 0;
}