#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace pluginfw {

/**
 * @brief 插件生命周期状态枚举
 */
enum class PluginState : uint8_t
{
    UNLOADED = 0,   // 未加载
    LOADED,         // 已加载
    INITIALIZED,    // 已初始化
    RUNNING,        // 运行中
    STOPPED,        // 已停止
    ERROR           // 错误状态
};

/**
 * @brief 将 PluginState 转换为字符串
 * @param s 插件状态
 * @return 对应的字符串描述
 */
inline const char* to_string(PluginState s)
{
    switch (s) {
    case PluginState::UNLOADED: return "UNLOADED";
    case PluginState::STOPPED: return "STOPPED";
    case PluginState::RUNNING: return "RUNNING";
    case PluginState::LOADED: return "LOADED";
    case PluginState::INITIALIZED: return "INITIALIZED";
    case PluginState::ERROR: return "ERROR";
    default: return "UNKNOWN";
    }
}

/**
 * @brief 状态变化回调函数类型
 * @param PluginMessage 插件自定义消息
 * @param oldstate 旧状态
 * @param newstate 新状态
 * @param message 附加信息
 */
using StateCallback = std::function<void(const std::string& PluginMessage, PluginState oldstate,
                                         PluginState newstate, const std::string& message)>;

/**
 * @brief 插件接口，所有插件必须实现的抽象类
 */
class IPlugin
{
public:
    virtual ~IPlugin() = default;

    /**
     * @brief 获取插件名称
     * @return 插件名称
     */
    virtual std::string name() const = 0;

    /**
     * @brief 获取插件版本
     * @return 版本字符串
     */
    virtual std::string version() const = 0;

    /**
     * @brief 初始化插件
     * @param yaml 配置参数（YAML格式字符串）
     * @param err 若失败，返回错误信息
     * @return 成功返回true，失败返回false
     */
    virtual bool init(const std::string& yaml, std::string& err) = 0;

    /**
     * @brief 启动插件
     * @param err 若失败，返回错误信息
     * @return 成功返回true，失败返回false
     */
    virtual bool start(std::string& err) = 0;

    /**
     * @brief 停止插件（不应抛出异常）
     * @return 成功返回true，失败返回false
     */
    virtual bool stop() noexcept = 0;
};

/**
 * @brief 插件工厂接口，用于创建插件实例
 */
class IPluginFactory
{
public:
    virtual ~IPluginFactory() = default;

    /**
     * @brief 创建插件实例
     * @return 指向新插件对象的 unique_ptr
     */
    virtual std::unique_ptr<IPlugin> create() = 0;
};

/**
 * @brief 创建插件工厂的函数指针类型
 */
using CreatePluginFactory = IPluginFactory* (*)();

/**
 * @brief 销毁插件工厂的函数指针类型
 */
using DestroyPluginFactory = void (*)(IPluginFactory*);

/// 导出符号：创建工厂的函数名
static constexpr const char* kCreateFactorySymbol = "CreatePluginFactory";

/// 导出符号：销毁工厂的函数名
static constexpr const char* kDestroyFactorySymbol = "DestroyPluginFactory";

} // namespace pluginfw