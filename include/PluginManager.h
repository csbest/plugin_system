#pragma one
#include "PluginAPI.h"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace pluginfw
{

    /*
        PluginEntry: 管理器内部记录插件全部信息的结构体
        - 动态库句柄
        - 工厂函数/指针
        - 插件实例
        - 当前状态
        - 配置文本

    */

    struct pluginEntry
    {
        std::string filePath;      // 插件动态库路径：xxx.so
        void* dlHandle = nullptr;  // dLopen 返回的句柄

        // 动态库导出的函数指针
        CreatePluginFactory createFactory = nullptr;
        DestoryPluginFactory destoryFactory = nullptr;

        // 工厂对象 + 插件对象
        IPluginFactory* factory = nullptr;
        std::unique_ptr<IPlugin> instance;

        Pluginstate state = Pluginstate::UNLOADED;
        std::string lastMessage;

        std::string configtext;

    };

    class PluginManager
    {
    public:
        explicit PluginManager(StateCallback cb = nullptr);
        ~PluginManager();

        PluginManager(const PluginManager&) = delete;
        PluginManager&  operator = (const PluginManager&) = delete;

        bool registerPlugin(const std::string& alias, const std::string& sopath);
        bool getContextCfg(const std::string& alias, const std::string& configtext);

        bool load(const std::string& alias);
        bool init(const std::string& alias);
        bool start(const std::string& alias);
        bool stop(const std::string& alias) noexcept;
        bool unload(const std::string& alias) noexcept;

        std::vector<std::string> listAlias() const;
        Pluginstate getPluginState(const std::string& alias) const;
        std::string lastErrrorMessage(const std::string& alias) const;

    private:

        void emitState(const std::string alias, Pluginstate oldstates, Pluginstate newStates, const std::string& msg);

        bool dlopenloacked(pluginEntry& e, const std::string& alias);
        void dlunloadloacked(pluginEntry& e, const std::string alias);
    
    private:
        
        mutable std::mutex mu_;
        std::unordered_map<std::string, pluginEntry> plugin_;
        StateCallback cb_;
        
    };
    
}
