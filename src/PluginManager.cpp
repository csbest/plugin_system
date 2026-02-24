#include "PluginManager.h"

namespace pluginfw {

// 构造函数，初始化状态回调
PluginManager::PluginManager(StateCallback cb)
    : cb_(std::move(cb))
{}

// 析构函数：依次停止并卸载所有已注册插件
PluginManager::~PluginManager()
{
    for (auto& name : listAlias()) stop(name);
    for (auto& name : listAlias()) unload(name);
}

/**
 * @brief 注册插件（记录路径，尚未加载）
 * @param alias 插件别名
 * @param soPath 动态库路径
 * @return 成功返回true，别名已存在返回false
 */
bool PluginManager::registerPlugin(const std::string& alias, const std::string& soPath)
{
    std::lock_guard<std::mutex> lk(mu_); // 线程安全

    if (plugin_.count(alias)) return false; // 别名已存在

    PluginEntry e;
    e.state    = PluginState::UNLOADED;
    e.filePath = soPath;

    plugin_.emplace(alias, std::move(e));
    return true;
}

/**
 * @brief 为插件设置配置文本
 * @param alias 插件别名
 * @param configtext YAML格式配置
 * @return 成功返回true，别名不存在返回false
 */
bool PluginManager::setPluginConfig(const std::string& alias, const std::string& configtext)
{
    std::lock_guard<std::mutex> lk(mu_); // 线程安全

    auto it = plugin_.find(alias);
    if (it == plugin_.end()) return false;

    it->second.configtext = configtext;
    return true;
}

/**
 * @brief 触发状态变化回调（如果已设置）
 * @param alias 插件别名
 * @param oldstates 旧状态
 * @param newStates 新状态
 * @param msg 附加消息
 */
void PluginManager::emitState(const std::string& alias, PluginState oldstate, PluginState newState,
                              const std::string& msg)
{
    if (cb_) cb_(alias, oldstate, newState, msg);
}

/**
 * @brief 实际执行动态库加载、工厂创建和插件实例创建（已加锁调用）
 * @param e 插件条目引用
 * @param alias 插件别名（用于错误消息）
 * @return 成功返回true，失败返回false并置状态为ERROR
 * 
 */
bool PluginManager::dlopenLocked(PluginEntry& e, const std::string& alias)
{
    try
    {
        // 1. 打开动态库
        e.dl = std::make_unique<DynamicLibrary>(e.filePath, RTLD_NOW);

        // 2. 获取工厂创建和销毁函数指针
        e.createFactory = e.dl->getFunction<CreatePluginFactory>(kCreateFactorySymbol);
        e.destoryFactory = e.dl->getFunction<DestroyPluginFactory>(kDestroyFactorySymbol);
        
        // 3. 创建工厂实例
        IPluginFactory* factory = e.createFactory();
        if ( !factory ) std::cerr << "Error: Factory create failed" << std::endl;

        // 使用自定义删除器包装工厂指针，确保析构时调用 destroyFactory
        e.factory = std::unique_ptr<IPluginFactory, std::function<void(IPluginFactory*)>>(
            factory,
            std::function<void(IPluginFactory*)>([destroyFunc = e.destoryFactory](IPluginFactory* f) {
                if (destroyFunc) destroyFunc(f);
            })
        );

        // 4. 创建插件实例
        e.instance = e.factory->create();
        if ( !e.instance ) std::cerr << "Error: plugin instance create failed" << std::endl; // 此处逻辑错误：应判断 !e.instance

        // 5. 更新状态
        PluginState old = e.state;
        e.state = PluginState::LOADED;
        e.lastMessage.clear();
        emitState(alias, old, e.state, "success to load");
        return true;       
    }
    catch(const std::exception& ex)
    {
        // 发生异常时清理已分配资源，置为ERROR状态
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

/**
 * @brief 实际执行插件卸载、资源释放（已加锁调用）
 * @param e 插件条目引用
 * @param alias 插件别名（用于回调）
 * 
 * 重置所有智能指针，资源自动释放，状态置为UNLOADED。
 */
void PluginManager::dlunloadLocked(PluginEntry& e, const std::string& alias)
{
    // 重置所有 unique_ptr，资源自动释放：
    // - instance 被 delete（假设 IPlugin 虚析构）
    // - factory 通过 destroyFactory 销毁
    // - dl 关闭动态库句柄

    e.instance.reset();
    e.factory.reset();
    e.dl.reset();

    // 清空相关信息
    e.createFactory  = nullptr;
    e.destoryFactory = nullptr;
    e.configtext.clear();
    e.lastMessage.clear();

    PluginState oldstate = e.state;
    e.state              = PluginState::UNLOADED;
    emitState(alias, oldstate, e.state, "success to unload");

}

/**
 * @brief 加载插件（打开动态库，获取工厂函数）
 * @param alias 插件别名
 * @return 成功返回true，失败返回false
 */
bool PluginManager::load(const std::string& alias)
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }

    dlopenLocked(it->second, alias);
    
    return true;
}

/**
 * @brief 初始化插件（调用插件工厂创建实例并调用 init）
 * @param alias 插件别名
 * @return 成功返回true，失败返回false
 */
bool PluginManager::init(const std::string& alias)
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 如果尚未加载，先自动加载
    if (e.state == PluginState::UNLOADED) {
        if (!PluginManager::load(alias)) return false;
    }

    // 只有 LOADED 或 STOPPED 状态允许初始化
    if (e.state != PluginState::LOADED && e.state != PluginState::STOPPED) {
        e.lastMessage   = std::string("Invalid state for init: ") + to_string(e.state);
        PluginState old = e.state;
        e.state         = PluginState::ERROR;
        emitState(alias, old, PluginState::ERROR, e.lastMessage);
        return false;
    }

    // 调用插件 init 方法
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

/**
 * @brief 启动插件（调用插件的 start）
 * @param alias 插件别名
 * @return 成功返回true，失败返回false
 */
bool PluginManager::start(const std::string& alias)
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 仅允许从 INITIALIZED 或 STOPPED 状态启动
    if (e.state != PluginState::INITIALIZED && e.state != PluginState::STOPPED) {
        e.lastMessage         = "plugin started failed";
        PluginState old_state = e.state;
        e.state               = PluginState::ERROR;
        emitState(alias, old_state, e.state, e.lastMessage);
        return false;
    }

    // 调用插件 start 方法
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

/**
 * @brief 停止插件（调用插件的 stop， noexcept 保证）
 * @param alias 插件别名
 * @return 成功返回true（若插件未运行也返回true），失败返回false（如别名不存在）
 */
bool PluginManager::stop(const std::string& alias) noexcept
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 仅在 RUNNING 状态且实例存在时调用 stop
    if (e.state == PluginState::RUNNING && e.instance) {
        e.instance->stop();
        PluginState old_state = e.state;
        e.state               = PluginState::STOPPED;
        emitState(alias, old_state, e.state, "STOPPED");
    }
    return true;
}

/**
 * @brief 卸载插件（销毁实例和工厂，关闭动态库， noexcept 保证）
 * @param alias 插件别名
 * @return 成功返回true，失败返回false（如别名不存在）
 */
bool PluginManager::unload(const std::string& alias) noexcept
{
    std::lock_guard<std::mutex> lk(mu_);

    auto it = plugin_.find(alias);
    if (it == plugin_.end()) { return false; }
    auto& e = it->second;

    // 如果插件正在运行，先停止它
    if (e.state == PluginState::RUNNING && e.instance) {
        e.instance->stop();
        PluginState old_state = e.state;
        e.state               = PluginState::STOPPED;
        emitState(alias, old_state, e.state, "STOPPED");
    }

    // 卸载动态库，释放资源
    dlunloadLocked(e, alias);

    return true;
}

/**
 * @brief 获取所有已注册插件的别名列表
 * @return 别名列表
 */
std::vector<std::string> PluginManager::listAlias() const
{
    std::lock_guard<std::mutex> lk(mu_);

    std::vector<std::string> out;
    out.reserve(plugin_.size());

    for (const auto& kv : plugin_) out.push_back(kv.first);
    return out;
}

/**
 * @brief 获取插件当前状态
 * @param alias 插件别名
 * @return 插件状态，若别名不存在返回 PluginState::ERROR
 */
PluginState PluginManager::getPluginState(const std::string& alias) const
{
    std::lock_guard<std::mutex> lk(mu_);
    auto                        it = plugin_.find(alias);
    if (it == plugin_.end()) return PluginState::ERROR;
    return it->second.state;
}

/**
 * @brief 获取插件最后一次错误消息
 * @param alias 插件别名
 * @return 错误消息，若无则返回空字符串；若别名不存在返回 "plugin not found"
 */
std::string PluginManager::lastErrorMessage(const std::string& alias) const
{
    std::lock_guard<std::mutex> lk(mu_);
    auto                        it = plugin_.find(alias);
    if (it == plugin_.end()) return "plugin not found";
    return it->second.lastMessage;
}

} // namespace pluginfw