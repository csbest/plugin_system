#include "CoreMonitorPlugin.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "SimpleConfig.h"

// 全局工厂实例，供动态库导出使用
static pluginfw::CoreMonitorFactory g_factory;

/**
 * @brief 导出函数：创建插件工厂
 * 
 * 动态库加载时通过此函数获取工厂指针。
 * @return 指向全局工厂实例的指针
 */
extern "C" pluginfw::IPluginFactory* CreatePluginFactory()
{
    return &g_factory;
}

/**
 * @brief 导出函数：销毁插件工厂
 * 
 * 由于工厂是全局静态对象，此处无需实际销毁。
 * @param factory 工厂指针（忽略）
 */
extern "C" void DestroyPluginFactory(pluginfw::IPluginFactory* factory)
{
    (void)factory; // 避免未使用参数警告
}

namespace pluginfw {

/**
 * @brief 构造函数：输出实例化信息
 */
CoreMonitorPlugin::CoreMonitorPlugin()
{
    std::cout << "[" << name_ << "] 构造函数： 插件实例化" << std::endl;
}

/**
 * @brief 析构函数：确保线程停止，输出销毁信息
 */
CoreMonitorPlugin::~CoreMonitorPlugin()
{
    if (running_) stop(); // 如果线程还在运行，先停止
    std::cout << "[" << name_ << "] 析构函数： 插件销毁" << std::endl;
}

/**
 * @brief 获取插件名称
 * @return 插件名字符串
 */
std::string CoreMonitorPlugin::name() const
{
    return name_;
}

/**
 * @brief 获取插件版本
 * @return 版本字符串
 */
std::string CoreMonitorPlugin::version() const
{
    return version_;
}

/**
 * @brief 解析 YAML 配置字符串，填充 config_ 结构体
 * @param yaml 配置字符串
 * @param err 错误信息输出（此实现中未使用）
 * @return 始终返回 true（即使配置项为空也成功）
 * 
 * 注意：此处解析非常基础，仅支持简单键值对，不支持嵌套。
 */
bool CoreMonitorPlugin::parseConfig(const std::string& yaml, std::string& err)
{
    auto key_value = SimpleYaml::parseKeyValueText(yaml);

    for (const auto& kv : key_value) {
        if (kv.first == "check_interval_ms") {
            config_.check_interval_ms = std::stoi(kv.second);
        }
        else if (kv.first == "target_path") {
            config_.target_path = kv.second;
        }
        else if (kv.first == "cpu_load_sim") {
            config_.cpu_load_sim = std::stoi(kv.second);
        }
    }

    return true;
}

/**
 * @brief 插件初始化
 * @param yaml 配置字符串（YAML 格式）
 * @param err 若失败，返回错误信息
 * @return 成功返回 true，失败返回 false
 * 
 * 初始化流程：
 * 1. 如果 yaml 非空，则解析配置。
 * 2. 检查监控间隔是否过短（小于 100ms）。
 * 3. 输出初始化完成信息。
 * 
 * 注意：原代码中 if (yaml.empty()) 的判断逻辑有误，应该是 if (!yaml.empty()) 才解析。
 */
bool CoreMonitorPlugin::init(const std::string& yaml, std::string& err)
{
    if (yaml.empty()) { // 此处逻辑错误：应该是非空才解析，但代码写成了空时解析
        if (!parseConfig(yaml, err)) { // 即使 yaml 为空也会调用 parseConfig（但 parseConfig 总是返回 true）
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

/**
 * @brief 获取指定路径的磁盘使用率
 * @param path 磁盘挂载点路径
 * @return 使用率百分比（0.0 ~ 100.0），失败时返回 -1.0
 * 
 * 通过 statvfs 系统调用获取文件系统统计信息，计算已用空间占总空间的百分比。
 */
double CoreMonitorPlugin::getDiskUsage(const std::string& path)
{
    struct statvfs buf;

    if (statvfs(path.c_str(), &buf) != 0) { return -1.0; }

    uint64_t block_size    = static_cast<uint64_t>(buf.f_frsize);
    uint64_t total_blocks = static_cast<uint64_t>(buf.f_blocks);
    uint64_t free_blocks   = static_cast<uint64_t>(buf.f_bfree);

    uint64_t total = block_size * total_blocks;
    uint64_t free  = block_size * free_blocks;
    uint64_t used  = total - free;

    if (total == 0 || used > total) { return 0.0; }

    return static_cast<double>(used) / (total) * 100.0;
}

/**
 * @brief 监控线程主循环
 * @return 线程退出时返回 true（暂未使用）
 * 
 * 循环执行以下操作：
 * 1. 获取当前时间戳。
 * 2. 获取磁盘使用率，并根据阈值生成状态信息（NORMAL/WARNING/CRITICAL）。
 * 3. 模拟 CPU 负载（基础值加上随机扰动）。
 * 4. 格式化输出系统状态。
 * 5. 按配置的间隔休眠。
 */
bool CoreMonitorPlugin::monitorLoop()
{
    while (running_) {
        SystemState state;
        state.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();

        double disk_usage = getDiskUsage(config_.target_path);
        if (disk_usage < 0) {
            state.status_msg = "Error reading disk";
        }
        else {
            state.disk_usage_percent = disk_usage;
            // 根据使用率设置状态消息
            state.status_msg =
                disk_usage > 90.0 ? "CRITICAL" : (disk_usage > 80 ? "WARNING" : "NORMAL");
        }

        // 模拟 CPU 负载：基础值加上 -5 到 4 的随机扰动
        state.simulate_cpu_load = config_.cpu_load_sim + (rand() % 10 - 5);

        // 格式化输出状态
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

/**
 * @brief 启动插件，创建监控线程
 * @param err 若失败，返回错误信息
 * @return 成功返回 true，失败返回 false
 * 
 * 启动前检查是否已在运行，设置 running_ 标志，然后创建线程执行 monitorLoop。
 * 若线程创建失败（如系统资源不足），捕获 system_error 异常，设置错误信息并返回 false。
 */
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

/**
 * @brief 停止插件，通知线程退出并等待结束
 * @return 始终返回 true
 * 
 * 设置 running_ 为 false，通知监控循环退出。
 * 若线程可 join，则等待其结束。
 */
bool CoreMonitorPlugin::stop() noexcept
{
    std::cout << "[" << name_ << "] 插件正在停止运行" << std::endl;
    running_ = false;

    if (monitor_thread.joinable()) monitor_thread.join();
    std::cout << "[" << name_ << "::Stop] 插件已停止" << std::endl;

    return true;
}

} // namespace pluginfw