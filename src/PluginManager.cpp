#include "PluginManager.h"
#include <dlfcn.h>
#include <iostream>

namespace pluginfw 
{

    PluginManager::PluginManager(StateCallback cb) : cb_(std::move(cb)) {}

    PluginManager::~PluginManager()
    {

        for (auto& name : listAlias() ) stop(name);
        for (auto& name : listAlias() ) unload(name);

    }

    bool PluginManager::registerPlugin(const std::string& alias, const std::string& soPath)
    {
        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 查找插件
        if ( plugin_.count(alias) ) return false;

        // 注册插件
        pluginEntry e;
        e.state = Pluginstate::UNLOADED;
        e.filePath = soPath;

        // 添加插件
        plugin_.emplace(alias, std::move(e));
        return true;

    }

    bool PluginManager::getContextCfg(const std::string& alias, const std::string& configtext)
    {
        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 查找插件
        auto it = plugin_.find(alias);
        if ( it == plugin_.end() ) return false;

        // 查找
        it->second.configtext = configtext;

        return true;
    }

    void PluginManager::emitState(const std::string alias, Pluginstate oldstates, Pluginstate newStates, const std::string& msg)
    {
        if ( cb_ ) cb_(alias, oldstates, newStates, msg);
    }

    /*
        dLLoadLocked: 真正执行动态库加载 + 创建工厂 + 创建插件实例
        注意： 函数名带 Locked， 表示调用它之前要已经拿到了拿到 mutex (线程安全)
    */
    bool PluginManager::dlopenloacked(pluginEntry& e, const std::string& alias)
    {
        // 打开库
        e.dlHandle = dlopen(e.filePath.c_str(), RTLD_NOW);
        if ( !e.dlHandle )
        {
            e.lastMessage = std::string("dlopen failed: ") + dlerror();
            e.state = Pluginstate::ERROR;
            emitState(alias, Pluginstate::UNLOADED, Pluginstate::ERROR, e.lastMessage);
            return false;
        }

        // 从库中工厂创建函数
        dlerror();
        e.createFactory = reinterpret_cast<CreatePluginFactory>(dlsym(e.dlHandle, kCreateFactorySymbol));
        if ( const char * err = dlerror() )
        {
            e.lastMessage = std::string("dlysm cannot find : ") + dlerror();
            e.state = Pluginstate::ERROR;
            emitState(alias, Pluginstate::UNLOADED, Pluginstate::ERROR, e.lastMessage);
            dlclose(e.dlHandle);
            e.dlHandle = nullptr;
            return false;            
        }

        // 从库中找到工厂销毁函数
        dlerror();
        e.destoryFactory = reinterpret_cast<DestoryPluginFactory>(dlsym(e.dlHandle, kDestoryFactorySymbol));
        if (const char * err = dlerror())
        {
            e.lastMessage = std::string("dlysm cannot find : ") + dlerror();
            e.state = Pluginstate::ERROR;
            emitState(alias, Pluginstate::UNLOADED, Pluginstate::ERROR, e.lastMessage);
            dlclose(e.dlHandle);
            e.dlHandle = nullptr;
            return false;      
        }

        // 创建插件工厂
        e.factory = e.createFactory();
        if ( e.factory == nullptr )
        {
            e.lastMessage = " Cannot open factory";
            e.state = Pluginstate::ERROR;
            emitState(alias, Pluginstate::UNLOADED, Pluginstate::ERROR, e.lastMessage);
            dlclose(e.dlHandle);
            e.dlHandle = nullptr;
            return false;
        }

        // 创建插件实例
        e.instance = e.factory->create();
        if ( e.instance == nullptr )
        {
            e.lastMessage = " Cannot create plugin instance ";
            e.state = Pluginstate::ERROR;
            emitState(alias, Pluginstate::UNLOADED, Pluginstate::ERROR, e.lastMessage);
            dlclose(e.dlHandle);
            e.dlHandle = nullptr;
            e.destoryFactory(e.factory);
            e.factory = nullptr;
            return false;
        }

        // 
        Pluginstate oldstate = e.state;
        e.state = Pluginstate::LOADED;
        e.lastMessage.clear();

        emitState(alias, oldstate, e.state, "success to load");   
        return true;
    }

    void PluginManager::dlunloadloacked(pluginEntry& e, const std::string alias)
    {
        // 智能指针释放
        e.instance.reset();

        // 工厂删除
        if ( e.factory && e.destoryFactory )
        {
            e.destoryFactory(e.factory);
        }
        e.factory = nullptr;

        // 关闭库
        dlclose(e.dlHandle);
        e.dlHandle = nullptr;

        // 清空信息
        e.createFactory = nullptr;
        e.destoryFactory = nullptr;
        e.configtext.clear();
        e.lastMessage.clear();

        Pluginstate oldstate = e.state;
        e.state = Pluginstate::UNLOADED;
        emitState(alias, oldstate, e.state, "success to load");

    }

    bool PluginManager::load(const std::string& alias)
    {
        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 
        auto it = plugin_.find(alias);
        if ( it == plugin_.end() )
        {
            return false;
        }

        return dlopenloacked( it->second, alias);
    }

    bool PluginManager::init(const std::string& alias)
    {

        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 查找插件
        auto it = plugin_.find(alias);
        if ( it == plugin_.end() )
        {
            return false;
        }
        auto& e = it->second; 

        // 未加载就先加载一下
        if ( e.state == Pluginstate::UNLOADED ) 
        {
            if( !dlopenloacked( e , alias) ) return false;
        }

        // 只有停止和已加载可以初始化
        if ( e.state != Pluginstate::LOADED && e.state != Pluginstate::STOPPED )
        {
            e.lastMessage = std::string("Invalid state for init: ") + to_string(e.state);
            Pluginstate old = e.state;
            e.state = Pluginstate::ERROR;
            emitState(alias, old, Pluginstate::ERROR, e.lastMessage);
        }

        // 初始化
        std::string err;
        if ( !e.instance->init(e.configtext, err) ) 
        {
            e.lastMessage = "init failed: " + err;
            Pluginstate old = e.state;
            e.state = Pluginstate::ERROR;
            emitState(alias, old, Pluginstate::ERROR, e.lastMessage);
            return false;          
        }

        Pluginstate old_state = e.state;
        e.state = Pluginstate::INITIALIZED;
        emitState(alias, old_state, e.state, e.lastMessage);
        return true;
        
    }

    bool PluginManager::start(const std::string& alias)
    {
        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 查找插件
        auto it = plugin_.find(alias);
        if ( it == plugin_.end() ) 
        {
            return false;
        }
        auto& e = it->second;

        // 状态限制
        if ( e.state != Pluginstate::INITIALIZED && e.state != Pluginstate::STOPPED )
        {
            e.lastMessage = "plugin started failed" ;
            Pluginstate old_state = e.state;
            e.state = Pluginstate::ERROR;
            emitState(alias, old_state, e.state, e.lastMessage);
            return false;
        }

        // 插件启动
        std::string err;
        if ( !e.instance->start(err) )
        {
            e.lastMessage = "plugin started failed" + err;
            Pluginstate old_state = e.state;
            e.state = Pluginstate::ERROR;
            emitState(alias, old_state, e.state, e.lastMessage);    
            return false;      
        }

        // 更新状态
        Pluginstate old_state = e.state;
        e.state = Pluginstate::RUNNING;
        emitState(alias, old_state, e.state, "Running");
        return true;
    }

    bool PluginManager::stop (const std::string& alias) noexcept
    {
        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 查找线程
        auto it = plugin_.find(alias);
        if ( it == plugin_.end() )
        {
            return  false;
        }
        auto& e = it->second;

        // 状态查找不报错
        if ( e.state == Pluginstate::RUNNING && e.instance )
        {
            e.instance->stop();
            Pluginstate old_state = e.state;
            e.state = Pluginstate::STOPPED;
            emitState(alias, old_state, e.state, "STOPPED");            
        }
        return true;
    }

    bool PluginManager::unload(const std::string& alias) noexcept
    {
        // 线程安全
        std::lock_guard<std::mutex> lk(mu_);

        // 查找插件
        auto it = plugin_.find(alias);
        if ( it == plugin_.end() )
        {
            return  false;
        }
        auto& e = it->second;      

        // 状态查找
        if ( e.state == Pluginstate::RUNNING && e.instance )
        {
            e.instance->stop();
            Pluginstate old_state = e.state;
            e.state = Pluginstate::STOPPED;
            emitState(alias, old_state, e.state, "STOPPED");            
        }

        // 卸载动态库
        dlunloadloacked(e, alias);
        return true;
    }

    std::vector<std::string> PluginManager::listAlias() const
    {
        
        std::lock_guard<std::mutex> lk(mu_);

        std::vector<std::string> out;
        out.reserve(plugin_.size());

        for ( const auto& kv : plugin_) out.push_back(kv.first);
        return out;

    }

    Pluginstate PluginManager::getPluginState(const std::string& alias) const 
    {

        std::lock_guard<std::mutex> lk(mu_);
        auto it = plugin_.find(alias);
        if (it == plugin_.end()) return Pluginstate::ERROR;
        return it->second.state;

    }

    std::string PluginManager::lastErrrorMessage(const std::string& alias) const 
    {

        std::lock_guard<std::mutex> lk(mu_);
        auto it = plugin_.find(alias);
        if (it == plugin_.end()) return "plugin not found";
        return it->second.lastMessage;

    }  

}