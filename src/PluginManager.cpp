#include "PluginManager.h"

namespace pluginfw {

PluginManager::PluginManager(StateCallback cb)
    : cb_(std::move(cb))
{}

PluginManager::~PluginManager()
{
    for (auto& name : listAlias()) stop(name);
    for (auto& name : listAlias()) unload(name);
}

bool PluginManager::registerPlugin(const std::string& alias, const std::string& soPath)
{
    // 线程安全
    std::lock_guard<std::mutex> lk(mu_);

    // 查找插件
    if (plugin_.count(alias)) return false;

    // 注册插件
    PluginEntry e;
    e.state    = PluginState::UNLOADED;
    e.filePath = soPath;

    // 添加插件
    plugin_.emplace(alias, std::move(e));
    return true;
}

bool PluginManager::setPluginConfig(const std::string& alias, const std::string& configtext)
{
    // 线程安全
    std::lock_guard<std::mutex> lk(mu_);

    // 查找插件
    auto it = plugin_.find(alias);
    if (it == plugin_.end()) return false;

    // 查找
    it->second.configtext = configtext;

    return true;
}

void PluginManager::emitState(const std::string alias, PluginState oldstates, PluginState newStates,
                              const std::string& msg)
{
    if (cb_) cb_(alias, oldstates, newStates, msg);
}

/*

    dLLoadLocked: 真正执行动态库加载 + 创建工厂 + 创建插件实例
    注意： 函数名带 Locked， 表示调用它之前要已经拿到了拿到 mutex (线程安全)

*/
bool PluginManager::dlopenloacked(PluginEntry& e, const std::string& alias)
{
    try
    {
        // 1.打开动态库
        e.dl = std::make_unique<DynamicLibrary>(e.filePath, RTLD_NOW);

        // 2.获取工厂创建、销毁函数
        e.createFactory = e.dl->getFunction<CreatePluginFactory>(kCreateFactorySymbol);
        e.destoryFactory = e.dl->getFunction<DestroyPluginFactory>(kDestoryFactorySymbol);
        
        // 3.创建工厂实例
        IPluginFactory* factory = e.createFactory();
        if ( !factory ) std::cerr << "Error: Factory create failed" << std::endl;

        e.factory = std::unique_ptr<IPluginFactory, std::function<void(IPluginFactory*)>>(
            factory,
            std::function<void(IPluginFactory*)>([destroyFunc = e.destoryFactory](IPluginFactory* f) {
                if (destroyFunc) destroyFunc(f);
            })
        );

        
        // 4.创建插件实例
        e.instance = e.factory->create();
        if ( e.instance ) std::cerr << "Error: plugin instance create failed" << std::endl;

        // 5.更新状态
        PluginState old = e.state;
        e.state = PluginState::LOADED;
        e.lastMessage.clear();
        emitState(alias, old, e.state, "success to load");
        return true;       
    }
    catch(const std::exception& ex)
    {
        // 清空相关资源
        e.lastMessage.clear();
        e.state = PluginState::ERROR;

        e.createFactory = nullptr;
        e.destoryFactory = nullptr;

        e.factory.reset();
        e.instance.reset();
        e.dl.reset();

        emitState(alias, PluginState::UNLOADED, PluginState::ERROR, e.lastMessage);
        std::cerr << ex.what() << '\n';
        return false;
    }
    
}

void PluginManager::dlunloadloacked(PluginEntry& e, const std::string alias)
{
    // 重置所有 unique_ptr，资源自动释放：
    // - instance 被 delete（假设 IPlugin 虚析构）
    // - factory 通过 destroyFactory 销毁
    // - dl 关闭动态库句柄

    e.instance.reset();
    e.factory.reset();
    e.dl.reset();

    // 清空信息
    e.createFactory  = nullptr;
    e.destoryFactory = nullptr;
    e.configtext.clear();
    e.lastMessage.clear();

    PluginState oldstate = e.state;
    e.state              = PluginState::UNLOADED;
    emitState(alias, oldstate, e.state, "success to load");

}

bool PluginManager::load(const std::string& alias)
{
    // 线程安全
    std::lock_guard<std::mutex> lk(mu_);

    //
    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }

    return dlopenloacked(it->second, alias);
}

bool PluginManager::init(const std::string& alias)
{
    // 线程安全
    std::lock_guard<std::mutex> lk(mu_);

    // 查找插件
    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 未加载就先加载一下
    if (e.state == PluginState::UNLOADED) {
        if (!dlopenloacked(e, alias)) return false;
    }

    // 只有停止和已加载可以初始化
    if (e.state != PluginState::LOADED && e.state != PluginState::STOPPED) {
        e.lastMessage   = std::string("Invalid state for init: ") + to_string(e.state);
        PluginState old = e.state;
        e.state         = PluginState::ERROR;
        emitState(alias, old, PluginState::ERROR, e.lastMessage);
        return false;
    }

    // 初始化
    std::string err;
    if (!e.instance->init(e.configtext, err)) {
        e.lastMessage   = "init failed: " + err;
        PluginState old = e.state;
        e.state         = PluginState::ERROR;
        emitState(alias, old, PluginState::ERROR, e.lastMessage);
        return false;
    }

    PluginState old_state = e.state;
    e.state               = PluginState::INITIALIZED;
    emitState(alias, old_state, e.state, e.lastMessage);
    return true;
}

bool PluginManager::start(const std::string& alias)
{
    // 线程安全
    std::lock_guard<std::mutex> lk(mu_);

    // 查找插件
    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 状态限制
    if (e.state != PluginState::INITIALIZED && e.state != PluginState::STOPPED) {
        e.lastMessage         = "plugin started failed";
        PluginState old_state = e.state;
        e.state               = PluginState::ERROR;
        emitState(alias, old_state, e.state, e.lastMessage);
        return false;
    }

    // 插件启动
    std::string err;
    if (!e.instance->start(err)) {
        e.lastMessage         = "plugin started failed" + err;
        PluginState old_state = e.state;
        e.state               = PluginState::ERROR;
        emitState(alias, old_state, e.state, e.lastMessage);
        return false;
    }

    // 更新状态
    PluginState old_state = e.state;
    e.state               = PluginState::RUNNING;
    emitState(alias, old_state, e.state, "Running");
    return true;
}

bool PluginManager::stop(const std::string& alias) noexcept
{
    // 线程安全
    std::lock_guard<std::mutex> lk(mu_);

    // 查找线程
    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 状态查找不报错
    if (e.state == PluginState::RUNNING && e.instance) {
        e.instance->stop();
        PluginState old_state = e.state;
        e.state               = PluginState::STOPPED;
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
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 状态查找
    if (e.state == PluginState::RUNNING && e.instance) {
        e.instance->stop();
        PluginState old_state = e.state;
        e.state               = PluginState::STOPPED;
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

    for (const auto& kv : plugin_) out.push_back(kv.first);
    return out;
}

PluginState PluginManager::getPluginState(const std::string& alias) const
{
    std::lock_guard<std::mutex> lk(mu_);
    auto                        it = plugin_.find(alias);
    if (it == plugin_.end()) return PluginState::ERROR;
    return it->second.state;
}

std::string PluginManager::lastErrrorMessage(const std::string& alias) const
{
    std::lock_guard<std::mutex> lk(mu_);
    auto                        it = plugin_.find(alias);
    if (it == plugin_.end()) return "plugin not found";
    return it->second.lastMessage;
}

}   // namespace pluginfw