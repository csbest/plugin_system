C++ Plugin Framework
Lightweight • High Performance • Extensible • Plugin Architecture

Fetched contentFetched contentFetched contentFetched contentFetched content

📦 Overview
C++ Plugin Framework 是一个 轻量级插件框架，用于学习和实践 插件化架构、动态库加载和系统解耦设计。

该框架提供：

🔌 动态插件加载
🔄 插件热插拔
⚙️ 完整生命周期管理
🧩 接口抽象 + 工厂模式
🧵 线程安全插件管理
📡 事件回调机制
适用于：

嵌入式系统
系统监控工具
可扩展桌面应用
模块化软件架构
✨ Features
1️⃣ 插件接口抽象
插件通过 纯虚接口 (IPlugin) 与宿主通信，实现完全解耦。

class IPlugin {
public:
    virtual ~IPlugin() = default;

    virtual bool initialized() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
};
特点：

动态多态
标准插件生命周期
可扩展接口设计
2️⃣ 工厂模式创建插件
插件通过 Factory Pattern 创建实例。

避免宿主直接 new/delete 插件对象。

class IPluginFactory {
public:
    virtual ~IPluginFactory() = default;

    virtual std::unique_ptr<IPlugin> create() = 0;
};
导出插件创建函数：

using CreatePluginFactory = IPluginFactory* (*)();
using DestroyPluginFactory = void (*)(IPluginFactory*);
优势：

避免内存跨模块释放
插件独立控制生命周期
提高稳定性
3️⃣ 动态库加载
框架基于 Linux dlopen/dlsym 实现插件加载。

void* handle = dlopen("./plugin.so", RTLD_LAZY);

auto createFactory =
    (CreatePluginFactory)dlsym(handle, "CreatePluginFactory");
插件通过：

extern "C"
导出函数以避免 C++ Name Mangling。

优势：

支持插件热插拔
无需重新编译主程序
动态扩展功能
4️⃣ 插件生命周期管理
插件拥有严格的状态机管理：

UNLOADED
    ↓
LOADED
    ↓
INITIALIZED
    ↓
RUNNING
    ↓
STOPPED
    ↓
UNLOADED
状态异常自动进入：

ERROR
实现：

enum class PluginState {
    UNLOADED,
    LOADED,
    INITIALIZED,
    RUNNING,
    STOPPED,
    ERROR
};
优点：

防止非法调用
状态可预测
方便调试
5️⃣ 线程安全插件管理
插件管理器内部使用：

std::mutex
std::lock_guard
std::unique_lock
std::condition_variable
示例：

std::mutex mtx;

void compute()
{
    std::lock_guard<std::mutex> lock(mtx);
}
保证：

多线程安全
无数据竞争
无死锁风险
6️⃣ 事件回调机制
插件状态变化会触发回调：

using PluginStateCallback =
    std::function<void(const std::string&, PluginState)>;
应用场景：

GUI刷新
日志记录
自动恢复
监控系统
🏗 Architecture
插件框架分为 四层架构：

+------------------------------------------------+
|                Host Application                 |
|------------------------------------------------|
| Business Logic                                 |
| Plugin Manager API                             |
| Callback Handling                              |
+------------------------------------------------+
                     │
                     ▼
+------------------------------------------------+
|                 Plugin Manager                  |
|------------------------------------------------|
| dlopen / dlclose                               |
| Lifecycle Management                           |
| Thread Safety                                  |
| Event Dispatch                                 |
+------------------------------------------------+
                     │
                     ▼
+------------------------------------------------+
|                Interface Layer                  |
|------------------------------------------------|
| IPlugin                                        |
| IPluginFactory                                 |
| Callback Definitions                           |
+------------------------------------------------+
                     ▲
                     │
+------------------------------------------------+
|                  Plugin Layer                   |
|------------------------------------------------|
| Disk Monitor Plugin                            |
| CPU Monitor Plugin                             |
| Custom Plugins                                 |
+------------------------------------------------+
展开
架构特点：

低耦合
高扩展
模块化设计
📁 Project Structure
.
├── README.md
├── config.yaml
├── main.cpp
├── CMakeLists.txt
├── include/
│   ├── PluginAPI.h
│   ├── PluginManager.h
│   └── SimpleConfig.h
├── src/
│   ├── PluginManager.cpp
│   └── SimpleConfig.cpp
├── plugins/
│   └── core_monitor/
│       ├── include/
│       │   └── CoreMonitorPlugin.h
│       └── src/
│           └── CoreMonitorPlugin.cpp
├── build/
│   ├── lib/
│   │   ├── libCoreMonitor.so
│   │   └── libplugin_framework.so
│   └── bin/
│       └── main
🚀 Quick Start
Requirements
C++17
CMake 3.10+
Linux
Build
git clone https://github.com/csbest/plugin_system.git

cd plugin_framework

mkdir build
cd build

cmake ..
make
Run
./bin/main
🔌 Writing Your Own Plugin
实现插件只需要三步：

1️⃣ 实现接口
class MyPlugin : public IPlugin
{
public:
    bool initialized() override;
    bool start() override;
    bool stop() override;
};
2️⃣ 实现工厂
class MyPluginFactory : public IPluginFactory
{
public:
    std::unique_ptr<IPlugin> create() override
    {
        return std::make_unique<MyPlugin>();
    }
};
3️⃣ 导出工厂函数
extern "C"
IPluginFactory* CreatePluginFactory()
{
    return new MyPluginFactory();
}

extern "C"
void DestroyPluginFactory(IPluginFactory* factory)
{
    delete factory;
}
🎯 Design Goals
该项目旨在深入实践以下技术：

Plugin Architecture
Dynamic Library Loading
C++ Polymorphism
RAII Memory Management
Multithreading Safety
Event Driven Programming

🎯 Design Goals
该项目旨在深入实践以下技术：

Plugin Architecture
Dynamic Library Loading
C++ Polymorphism
RAII Memory Management
Multithreading Safety
Event Driven Programming
📊 Future Improvements
计划新增：

插件热更新
插件依赖管理
插件配置系统
插件隔离机制
Windows DLL 支持
Plugin Sandbox
🤝 Contributing
欢迎提交 PR 改进该项目。

如果你有好的插件架构设计建议，欢迎讨论。

📜 License
MIT License