#pragma one
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <dlfcn.h>
#include <iostream>
#include "PluginAPI.h"


namespace pluginfw {

// DynamicLibrary 类，在构造时打开动态库，析构时自动关闭
class DynamicLibrary
{
private:
    void* handle_;

public:
    // explicit 禁止使用该构造函数进行隐式类型转换，从而避免意外的类型转换带来的错误
    explicit DynamicLibrary(const std::string& path, int flags = RTLD_NOW)
        : handle_(dlopen(path.c_str(), flags))
    {
        if (!handle_) { throw std::runtime_error(dlerror()); }
    }

    ~DynamicLibrary()
    {
        if (handle_) { dlclose(handle_); }
    }

    // DynamicLibrary a;            // 默认构造
    // DynamicLibrary b = a;        // 拷贝构造（b 是新对象，用 a 初始化）
    // DynamicLibrary c(a);         // 拷贝构造（另一种写法）
    // c = a;                       // 拷贝赋值（c 和 a 都已存在）

    // 拷贝构造函数	    ClassName(const ClassName&)	创建新对象时，用已有对象初始化
    // 必须用引用，避免递归 拷贝赋值运算符	ClassName& operator=(const ClassName&)
    // 两个对象都存在时，赋值	返回引用支持链式赋值

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;

    // DynamicLibrary a;                    // 默认构造
    // DynamicLibrary b = std::move(a);      // 移动构造（b 是新对象，从 a 转移资源）
    // DynamicLibrary c(std::move(b));       // 移动构造（另一种写法）
    // c = std::move(a);                     // 移动赋值（c 和 a 都已存在，资源从 a 转移到 c）

    // 移动构造函数	    ClassName(ClassName&& other)	创建新对象时，用临时对象（右值）初始化
    // 转移资源所有权，源对象置空防止重复释放 移动赋值运算符	ClassName& operator=(ClassName&&
    // other)	两个对象都存在时，将右值对象资源转移给左值
    // 释放旧资源，转移新资源，自赋值检查，返回引用支持链式赋值

    DynamicLibrary(DynamicLibrary&& other)
        : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }

    DynamicLibrary& operator=(DynamicLibrary&& other)
    {
        // 检查对象是否在赋值自己
        if (this != &other) {
            if (handle_) dlclose(handle_);
            handle_       = other.handle_;
            other.handle_ = nullptr;
        }

        return *this;
    }

    void* getSymbol(const std::string& name) const
    {
        // 清除错误
        dlerror();

        // 查找函数
        void*       sym = dlsym(handle_, name.c_str());
        const char* err = dlerror();

        if (err) { throw std::runtime_error(err); }

        return sym;
    }

    template<typename func> func getFunction(const std::string& name)
    {
        return reinterpret_cast<func>(getSymbol(name));
    }
};


/*
    PluginEntry: 管理器内部记录插件全部信息的结构体
    - 动态库句柄
    - 工厂函数/指针
    - 插件实例
    - 当前状态
    - 配置文本

*/

struct PluginEntry
{
    // 插件基本信息

    PluginState state = PluginState::UNLOADED;

    std::string filePath;             // 插件动态库路径：xxx.so
    std::string lastMessage;
    std::string configtext;             
    
    std::unique_ptr<DynamicLibrary> dl;   // dLopen 返回的句柄
    std::unique_ptr<IPlugin> instance;    // 插件实例
    std::unique_ptr<IPluginFactory, std::function<void(IPluginFactory*)>> factory;

    // 动态库导出的函数指针
    CreatePluginFactory  createFactory  = nullptr;
    DestroyPluginFactory destoryFactory = nullptr;

};



class PluginManager
{
public:
    explicit PluginManager(StateCallback cb = nullptr);
    ~PluginManager();

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    bool registerPlugin(const std::string& alias, const std::string& sopath);
    bool setPluginConfig(const std::string& alias, const std::string& configtext);

    bool load(const std::string& alias);
    bool init(const std::string& alias);
    bool start(const std::string& alias);
    bool stop(const std::string& alias) noexcept;
    bool unload(const std::string& alias) noexcept;

    std::vector<std::string> listAlias() const;
    PluginState              getPluginState(const std::string& alias) const;
    std::string              lastErrrorMessage(const std::string& alias) const;

private:
    void emitState(const std::string alias, PluginState oldstates, PluginState newStates,
                   const std::string& msg);

    bool dlopenloacked(PluginEntry& e, const std::string& alias);
    void dlunloadloacked(PluginEntry& e, const std::string alias);

private:
    mutable std::mutex                           mu_;
    std::unordered_map<std::string, PluginEntry> plugin_;
    StateCallback                                cb_;
};

}   // namespace pluginfw
