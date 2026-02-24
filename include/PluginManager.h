#pragma once
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <dlfcn.h>
#include <iostream>
#include "PluginAPI.h"

namespace pluginfw {

/**
 * @brief 动态库封装类，RAII 管理 dlopen/dlclose
 * 
 * 在构造时打开动态库，析构时自动关闭。不可拷贝，仅可移动。
 */
class DynamicLibrary
{
private:
    void* handle_; // dlopen 返回的库句柄

public:
    /**
     * @brief 打开动态库
     * @param path 库路径
     * @param flags dlopen 标志，默认 RTLD_NOW
     * @throw std::runtime_error 打开失败时抛出
     */
    explicit DynamicLibrary(const std::string& path, int flags = RTLD_NOW)
        : handle_(dlopen(path.c_str(), flags))
    {
        if (!handle_) { throw std::runtime_error(dlerror()); }
    }

    /**
     * @brief 析构函数，自动关闭库句柄
     */
    ~DynamicLibrary()
    {
        if (handle_) { dlclose(handle_); }
    }

    // 禁止拷贝构造和拷贝赋值，防止重复释放
    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    /**
     * @brief 移动构造函数，转移句柄所有权
     * @param other 源对象，转移后置空
     */
    DynamicLibrary(DynamicLibrary&& other)
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    /**
     * @brief 移动赋值运算符
     * @param other 源对象，转移后置空
     * @return 自身引用
     */
    DynamicLibrary& operator=(DynamicLibrary&& other)
    {
        if (this != &other) {
            if (handle_) dlclose(handle_);
            handle_       = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    /**
     * @brief 获取符号地址（通用版本）
     * @param name 符号名称
     * @return 符号地址
     * @throw std::runtime_error 查找失败时抛出
     */
    void* getSymbol(const std::string& name) const
    {
        dlerror(); // 清除旧错误
        void* sym = dlsym(handle_, name.c_str());
        const char* err = dlerror();
        if (err) { throw std::runtime_error(err); }
        return sym;
    }

    /**
     * @brief 获取函数指针（模板版本）
     * @tparam func 函数类型，如 void(int)
     * @param name 符号名称
     * @return 转换后的函数指针
     */
    template<typename func>
    func getFunction(const std::string& name)
    {
        return reinterpret_cast<func>(getSymbol(name));
    }
};

/**
 * @brief 插件条目，记录一个插件的全部元数据和运行时信息
 */
struct PluginEntry
{
    PluginState state = PluginState::UNLOADED; // 当前状态

    std::string filePath;                       // 插件动态库路径
    std::string lastMessage;                     // 最近操作产生的消息（如错误信息）
    std::string configtext;                      // 插件配置文本（YAML格式）

    std::unique_ptr<DynamicLibrary> dl;          // 动态库对象
    std::unique_ptr<IPlugin> instance;           // 插件实例对象
    std::unique_ptr<IPluginFactory, std::function<void(IPluginFactory*)>> factory; // 工厂对象及自定义删除器

    // 从动态库导出的工厂函数指针
    CreatePluginFactory  createFactory  = nullptr;  // 创建工厂的函数
    DestroyPluginFactory destoryFactory = nullptr;  // 销毁工厂的函数（原命名拼写错误，保留）
};

/**
 * @brief 插件管理器，负责插件的加载、初始化、启动、停止、卸载等生命周期管理
 * 
 * 线程安全：所有公有方法均通过互斥锁保护内部数据。
 */
class PluginManager
{
public:
    /**
     * @brief 构造函数
     * @param cb 状态变化回调函数，可为空
     */
    explicit PluginManager(StateCallback cb = nullptr);

    /**
     * @brief 析构函数，自动卸载所有插件
     */
    ~PluginManager();

    // 禁止拷贝
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * @brief 注册插件（记录路径，尚未加载）
     * @param alias 插件别名，用于后续操作
     * @param sopath 动态库路径
     * @return 成功返回 true，失败返回 false（如别名已存在）
     */
    bool registerPlugin(const std::string& alias, const std::string& sopath);

    /**
     * @brief 为插件设置配置文本
     * @param alias 插件别名
     * @param configtext YAML 格式配置
     * @return 成功返回 true，失败返回 false（别名不存在或状态不允许）
     */
    bool setPluginConfig(const std::string& alias, const std::string& configtext);

    /**
     * @brief 加载插件（打开动态库，获取工厂函数）
     * @param alias 插件别名
     * @return 成功返回 true，失败返回 false
     */
    bool load(const std::string& alias);

    /**
     * @brief 初始化插件（调用插件工厂创建实例并调用 init）
     * @param alias 插件别名
     * @return 成功返回 true，失败返回 false
     */
    bool init(const std::string& alias);

    /**
     * @brief 启动插件（调用插件的 start）
     * @param alias 插件别名
     * @return 成功返回 true，失败返回 false
     */
    bool start(const std::string& alias);

    /**
     * @brief 停止插件（调用插件的 stop）
     * @param alias 插件别名
     * @return 成功返回 true，失败返回 false
     * @note noexcept 保证，内部捕获异常
     */
    bool stop(const std::string& alias) noexcept;

    /**
     * @brief 卸载插件（销毁插件实例和工厂，关闭动态库）
     * @param alias 插件别名
     * @return 成功返回 true，失败返回 false
     * @note noexcept 保证
     */
    bool unload(const std::string& alias) noexcept;

    /**
     * @brief 获取所有已注册的插件别名列表
     * @return 别名列表
     */
    std::vector<std::string> listAlias() const;

    /**
     * @brief 获取插件当前状态
     * @param alias 插件别名
     * @return 插件状态，若别名不存在返回 PluginState::ERROR
     */
    PluginState getPluginState(const std::string& alias) const;
    
private:

    /**
     * @brief 获取插件最后一次错误消息
     * @param alias 插件别名
     * @return 错误消息，若无则返回空字符串
     */
    std::string lastErrorMessage(const std::string& alias) const;

    /**
     * @brief 触发状态变化回调
     * @param alias 插件别名
     * @param oldstate 旧状态
     * @param newstate 新状态
     * @param msg 附加消息
     */
    void emitState(const std::string& alias, PluginState oldstate, PluginState newstate,
                   const std::string& msg);

    /**
     * @brief 实际加载动态库（已加锁调用）
     * @param e 插件条目引用
     * @param alias 插件别名（用于错误消息）
     * @return 成功返回 true，失败返回 false
     */
    bool dlopenLocked(PluginEntry& e, const std::string& alias);

    /**
     * @brief 实际卸载动态库（已加锁调用）
     * @param e 插件条目引用
     * @param alias 插件别名（用于错误消息）
     */
    void dlunloadLocked(PluginEntry& e, const std::string& alias);

private:
    mutable std::mutex mu_;                               // 保护插件映射的互斥锁
    std::unordered_map<std::string, PluginEntry> plugin_; // 别名到插件条目的映射
    StateCallback cb_;      
                                  // 状态变化回调
};

} // namespace pluginfw
