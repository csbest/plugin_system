#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace pluginfw 
{

    enum class Pluginstate : uint8_t 
    {
        UNLOADED = 0,
        LOADED,
        INITIALIZED,
        RUNNING,
        STOPPED,
        ERROR
    };

    inline const char* to_string(Pluginstate s)
    {
        switch (s)
        {
            case Pluginstate::UNLOADED : return "UNLOADED";
            case Pluginstate::STOPPED : return "STOPPED";
            case Pluginstate::RUNNING : return "RUNNING";
            case Pluginstate::LOADED : return "LOADED";
            case Pluginstate::INITIALIZED : return "INITALIZED";
            case Pluginstate::ERROR : return "ERROR";
            default: return "UNKNOWN";
        }
    }

    using StateCallback = std::function<void(
                            const std::string& PluginMesssage,
                            Pluginstate oldstate, 
                            Pluginstate newstate,
                            const std::string& message
    )>;

    class IPlugin 
    {
        public:
            virtual ~IPlugin() = default;
            virtual std::string name() const = 0;
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
            virtual ~IPluginFactory() = default;
            virtual std::unique_ptr<IPlugin> create() = 0;
    };

    using CreatePluginFactory = IPluginFactory* (*)();
    using DestoryPluginFactory = void (*)(IPluginFactory *);

    static constexpr const char* kCreateFactorySymbol = "CreatePluginFactory";
    static constexpr const char* kDestoryFactorySymbol = "DestoryPluginFactory";
    
}