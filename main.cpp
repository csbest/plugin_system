#include <iostream>
#include <thread>

#include "PluginManager.h"

void printPluginStateconst(std::string alias, pluginfw::PluginState oldstates,
                           pluginfw::PluginState newStates, const std::string& msg)
{
    std::cout << "[状态] " << alias << " 从 " << pluginfw::to_string(oldstates) << " 变为 "
              << pluginfw::to_string(newStates) << " 消息: " << msg << std::endl;
}

int main()
{
    std::thread test_thread([] { std::cout << "主程序测试线程运行中" << std::endl; });
    test_thread.join();

    // 创建插件管理器
    pluginfw::PluginManager manager(printPluginStateconst);

    // 参数文件路径
    std::string yaml_config = "./config.yaml";

    // 添加插件
    std::string alias  = "MonitorPlugin";
    std::string soPath = "./build/lib/libCoreMonitor.so";

    if (!manager.registerPlugin(alias, soPath)) {
        std::cerr << "注册失败" << std::endl;
        return 1;
    }

    if (!manager.load(alias)) {
        std::cerr << "加载失败" << std::endl;
        return 1;
    }

    if (!manager.init(alias)) {
        std::cerr << "初始化失败" << std::endl;
        return 1;
    }

    if (!manager.setPluginConfig(alias, yaml_config)) {
        std::cerr << "参数加载失败" << std::endl;
        return 1;
    }

    if (!manager.start(alias)) {
        std::cerr << "开始失败" << std::endl;
        return 1;
    }

    std::cout << "插件已启动，运行中... 按回车停止" << std::endl;
    std::cin.get();

    return 0;
}