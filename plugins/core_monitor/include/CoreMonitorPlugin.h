#pragma once
#include <sys/statvfs.h>   // 磁盘监控

#include <atomic>
#include <thread>

#include "PluginAPI.h"

namespace pluginfw {

class CoreMonitorPlugin : public IPlugin
{
public:
    CoreMonitorPlugin();
    ~CoreMonitorPlugin() override;

    std::string name() const override;
    std::string version() const override;

    bool init(const std::string& yaml, std::string& err) override;
    bool start(std::string& err) override;
    bool stop() noexcept override;

private:
    struct Config
    {
        int         check_interval_ms = 2000;
        std::string target_path       = "/";
        int         cpu_load_sim      = 50;
    };

    struct SystemState
    {
        uint64_t    timestamp;
        double      disk_usage_percent;
        double      simulate_cpu_load;
        std::string status_msg;
    };

    bool   parseConfig(const std::string& yaml, std::string& err);
    bool   monitorLoop();
    double getDiskUsage(const std::string& path);

private:
    std::string name_    = "CoreMonitor";
    std::string version_ = "1.0.0";

    Config config_;

    std::atomic<bool> running_;
    std::thread       monitor_thread;
};

class CoreMonitorFactory : public IPluginFactory
{
public:
    std::unique_ptr<IPlugin> create() override { return std::make_unique<CoreMonitorPlugin>(); }
};
}   // namespace pluginfw
