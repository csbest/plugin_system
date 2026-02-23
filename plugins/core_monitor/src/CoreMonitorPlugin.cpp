#include "CoreMonitorPlugin.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "SimpleConfig.h"

static pluginfw::CoreMonitorFactory g_factory;

extern "C" pluginfw::IPluginFactory* CreatePluginFactory()
{
    return &g_factory;
}

extern "C" void DestroyPluginFactory(pluginfw::IPluginFactory* factory)
{
    (void)factory;
}

namespace pluginfw {

CoreMonitorPlugin::CoreMonitorPlugin()
{
    std::cout << "[" << name_ << "] 构造函数： 插件实例化" << std::endl;
}

CoreMonitorPlugin::~CoreMonitorPlugin()
{
    if (running_) stop();
    std::cout << "[" << name_ << "] 析构函数： 插件销毁" << std::endl;
}

std::string CoreMonitorPlugin::name() const
{
    return name_;
}
std::string CoreMonitorPlugin::version() const
{
    return version_;
}

bool CoreMonitorPlugin::parseConfig(const std::string& yaml, std::string& err)
{
    auto key_value = SimpleYaml::parseKeyValueText(yaml);

    for (const auto& kv : key_value) {
        if (kv.first == "check_interval_ms") { config_.check_interval_ms = std::stoi(kv.second); }
        else if (kv.first == "target_path") {
            config_.target_path = kv.second;
        }
        else if (kv.first == "cpu_load_sim") {
            config_.cpu_load_sim = std::stoi(kv.second);
        }
    }

    return true;
}

bool CoreMonitorPlugin::init(const std::string& yaml, std::string& err)
{
    if (yaml.empty()) {
        if (!parseConfig(yaml, err)) {
            err = "解析配置失败";
            return false;
        }
    }

    if (config_.check_interval_ms < 100) {
        err = "监控间隔太短";
        return false;
    }

    std::cout << "[" << name_ << "::Init] 配置加载完成：间隔=" << config_.check_interval_ms
              << "ms, 路径=" << config_.target_path << std::endl;

    return true;
}

double CoreMonitorPlugin::getDiskUsage(const std::string& path)
{
    struct statvfs buf;

    if (statvfs(path.c_str(), &buf) != 0) { return -1.0; }

    uint64_t block_size    = static_cast<uint64_t>(buf.f_frsize);
    uint64_t totoal_blocks = static_cast<uint64_t>(buf.f_blocks);
    uint64_t free_blocks   = static_cast<uint64_t>(buf.f_bfree);

    uint64_t total = block_size * totoal_blocks;
    uint64_t free  = block_size * free_blocks;
    uint64_t used  = total - free;

    if (total == 0 || used > total) { return 0.0; }

    return static_cast<double>(used) / (total)*100.0;
}

bool CoreMonitorPlugin::monitorLoop()
{
    while (running_) {
        SystemState state;
        state.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

        double disk_usage = getDiskUsage(config_.target_path);
        if (disk_usage < 0) { state.status_msg = "Error reading disk"; }
        else {
            state.disk_usage_percent = disk_usage;
            state.status_msg =
                disk_usage > 90.0 ? "CRITICAL" : (disk_usage > 80 ? "WARNING" : "NORMAL");
        }

        // 2.模拟平滑控制数据
        state.simulate_cpu_load = config_.cpu_load_sim + (rand() % 10 - 5);

        // 3.模拟
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "[" << name_ << "::Data] Time:" << state.timestamp
            << " | Disk:" << state.disk_usage_percent << "%(" << state.status_msg << ")"
            << " | CPU Sim:" << state.simulate_cpu_load << "%" << std::endl;
        std::cout << oss.str();

        std::this_thread::sleep_for(std::chrono::milliseconds(config_.check_interval_ms));
    }

    return true;
}

bool CoreMonitorPlugin::start(std::string& err)
{
    std::cout << "[" << name_ << "] 插件开始运行" << std::endl;
    std::cout << "当前线程ID: " << std::this_thread::get_id() << std::endl;

    if (running_) {
        err = "线程已经启动";
        return false;
    }

    running_ = true;

    try {
        std::cout << "尝试创建线程..." << std::endl;
        monitor_thread = std::thread(&CoreMonitorPlugin::monitorLoop, this);
        std::cout << "线程创建成功，线程ID: " << monitor_thread.get_id() << std::endl;
        return true;
    }
    catch (const std::system_error& e) {
        err = "线程启动失败：[" + std::to_string(e.code().value()) + "] " + e.what();
        std::cerr << "异常捕获: " << err << std::endl;
        running_ = false;
        return false;
    }
}

bool CoreMonitorPlugin::stop() noexcept
{
    std::cout << "[" << name_ << "] 插件正在停止运行" << std::endl;
    running_ = false;

    if (monitor_thread.joinable()) monitor_thread.join();
    std::cout << "[" << name_ << "::Stop] 插件已停止" << std::endl;

    return true;
}

}   // namespace pluginfw