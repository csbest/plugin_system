#pragma once
#include <sys/statvfs.h>   // 提供 statvfs 系统调用，用于获取磁盘使用信息

#include <atomic>
#include <thread>

#include "PluginAPI.h"      // 插件接口定义（IPlugin, IPluginFactory）

namespace pluginfw {

/**
 * @brief 核心监控插件，实现 IPlugin 接口
 * 
 * 该插件定期收集系统信息（如磁盘使用率），并可模拟 CPU 负载。
 * 运行在独立的监控线程中，通过原子标志控制启停。
 */
class CoreMonitorPlugin : public IPlugin
{
public:
    CoreMonitorPlugin();
    ~CoreMonitorPlugin() override;

    /// @return 插件名称 "CoreMonitor"
    std::string name() const override;

    /// @return 插件版本 "1.0.0"
    std::string version() const override;

    /**
     * @brief 初始化插件，解析 YAML 配置
     * @param yaml 配置字符串（YAML 格式）
     * @param err 若失败，返回错误信息
     * @return 成功返回 true，失败返回 false
     */
    bool init(const std::string& yaml, std::string& err) override;

    /**
     * @brief 启动插件，创建监控线程
     * @param err 若失败，返回错误信息
     * @return 成功返回 true，失败返回 false
     */
    bool start(std::string& err) override;

    /**
     * @brief 停止插件，通知线程退出并等待结束
     * @return 始终返回 true（noexcept 保证）
     */
    bool stop() noexcept override;

private:
    /**
     * @brief 插件配置结构
     */
    struct Config
    {
        int         check_interval_ms = 2000; // 监控间隔（毫秒）
        std::string target_path       = "/";  // 要监控的磁盘路径
        int         cpu_load_sim      = 50;   // 模拟 CPU 负载百分比（0-100）
    };

    /**
     * @brief 系统状态快照结构
     */
    struct SystemState
    {
        uint64_t    timestamp;              // 时间戳（毫秒）
        double      disk_usage_percent;     // 磁盘使用率（0.0 ~ 100.0）
        double      simulate_cpu_load;      // 模拟 CPU 负载值
        std::string status_msg;              // 状态消息
    };

    /**
     * @brief 解析 YAML 配置字符串，填充 config_
     * @param yaml 配置字符串
     * @param err 错误信息输出
     * @return 成功返回 true，失败返回 false
     */
    bool parseConfig(const std::string& yaml, std::string& err);

    /**
     * @brief 监控线程主循环
     * @return 线程退出时返回 false（暂未使用）
     */
    bool monitorLoop();

    /**
     * @brief 获取指定路径的磁盘使用率
     * @param path 磁盘挂载点路径
     * @return 使用率百分比（0.0 ~ 100.0），失败时返回 -1.0
     */
    double getDiskUsage(const std::string& path);

private:
    std::string name_    = "CoreMonitor";   // 插件名称
    std::string version_ = "1.0.0";         // 插件版本

    Config config_;                          // 当前配置

    std::atomic<bool> running_;              // 控制监控线程运行的原子标志
    std::thread       monitor_thread;        // 监控线程对象
};

/**
 * @brief 核心监控插件的工厂类，用于创建插件实例
 */
class CoreMonitorFactory : public IPluginFactory
{
public:
    /**
     * @brief 创建 CoreMonitorPlugin 实例
     * @return 指向新插件对象的 unique_ptr
     */
    std::unique_ptr<IPlugin> create() override
    {
        return std::make_unique<CoreMonitorPlugin>();
    }
};

} // namespace pluginfw
