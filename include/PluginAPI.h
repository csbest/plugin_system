#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace pluginfw {

enum class PluginState : uint8_t
{
    UNLOADED = 0,
    LOADED,
    INITIALIZED,
    RUNNING,
    STOPPED,
    ERROR
};

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

using StateCallback = std::function<void(const std::string& PluginMesssage, PluginState oldstate,
                                         PluginState newstate, const std::string& message)>;

class IPlugin
{
public:
    virtual ~IPlugin()                  = default;
    virtual std::string name() const    = 0;
    virtual std::string version() const = 0;

    // 初始化
    virtual bool init(const std::string& yaml, std::string& err) = 0;
    // 打开
    virtual bool start(std::string& err) = 0;
    // 关闭
    virtual bool stop() noexcept = 0;
};

class IPluginFactory
{
public:
    virtual ~IPluginFactory()                 = default;
    virtual std::unique_ptr<IPlugin> create() = 0;
};

using CreatePluginFactory  = IPluginFactory* (*)();
using DestroyPluginFactory = void (*)(IPluginFactory*);

static constexpr const char* kCreateFactorySymbol  = "CreatePluginFactory";
static constexpr const char* kDestoryFactorySymbol = "DestroyPluginFactory";

}   // namespace pluginfw