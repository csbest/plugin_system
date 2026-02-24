## 1.项目简介

---

本项目是一个轻量级的 C++ 插件框架，支持动态库热插拔和完整的生命周期管理，是我为了深入理解插件化架构与动态库加载技术而设计，
通过抽象化接口和插件工厂模式实现宿主与插件的解耦，适用于需要动态扩展功能的嵌入式或桌面应用

---

## 特性

接口抽象与工厂模式：插件与宿主仅通过纯虚接口交互，由工厂负责插件实例的创建与销毁。

动态库加载与符号查找：支持 Linux 动态库（.so）的运行时加载，通过 dlopen/dlsym 动态获取符号，并使用 extern "C" 避免名字修饰，确保跨编译器兼容性。

严格的生命周期状态机：定义清晰的状态转换路径：UNLOADED → LOADED → INITIALIZED → RUNNING → STOPPED → UNLOADED；每个状态转换均经过合法性检查，异常时自动转入 ERROR 状态，保证插件行为可预测。

线程安全的管理器：插件管理器内部使用 std::mutex 保护核心数据结构，确保多线程环境下插件的并发操作安全无竞态。

事件回调机制：支持注册状态变化回调函数，插件状态变更（包括错误信息）时实时通知宿主，便于界面刷新、日志记录或错误恢复。

---

## 2.项目架构

---

```text
.
├── README.md                 # 项目说明文件（当前文件）
├── config.yaml               # 配置文件（YAML格式）
├── main.cpp                  # 主程序入口
├── .clang-format             # 代码格式化配置
├── CMakeLists.txt            # CMake 构建脚本
├── include/                  # 公共头文件
│   ├── PluginAPI.h           # 插件接口定义
│   ├── PluginManager.h       # 插件管理器声明
│   └── SimpleConfig.h        # 简单配置解析器声明
├── src/                      # 框架源码
│   ├── PluginManager.cpp     # 插件管理器实现
│   └── SimpleConfig.cpp      # 配置解析器实现
├── plugins/                  # 插件目录
│   └── core_monitor/         # 核心监控插件 - 示例插件
│       ├── include/
│       │   └── CoreMonitorPlugin.h   # 插件类声明
│       └── src/
│           └── CoreMonitorPlugin.cpp # 插件类实现
├── build/                    # 构建输出目录（CMake生成）
│   ├── lib/                  # 生成的动态库
│   │   ├── libCoreMonitor.so
│   │   └── libplugin_framework.so
│   ├── bin/                  # 生成的可执行文件
│   │   └── main
│   ├── CMakeFiles/           # CMake 临时文件（依赖、编译命令等）
│   ├── CMakeCache.txt        # CMake 缓存
│   ├── Makefile              # 生成的 Makefile
│   └── cmake_install.cmake   # 安装脚本
└── .git/                     # Git 版本控制目录
    ├── HEAD, config, index, logs/, objects/, refs/ 等

```


---

## 2.架构设计

框架分为四个核心层级：

```text
+---------------------------------------+
|           宿主应用层                   |
|        (Host Application)              |
|                                        |
|   • 业务逻辑实现                        |
|   • 调用管理器API(start/stop/init)      |
|   • 接收插件状态回调                     |
+---------------------------------------+
                  | 控制
                  v
+---------------------------------------+
|             管理层                      |
|        (PluginManager)                 |
|                                        |
|   • 插件动态加载/卸载 (dlopen/dlclose)   |
|   • 生命周期状态机                      |
|     (LOADED → INIT → RUNNING → STOPPED)|
|   • 线程安全锁保护                       |
|   • 状态变更事件分发                     |
+---------------------------------------+
                  | 依赖
                  v
+---------------------------------------+
|             接口层                      |
|        (Interface Layer)               |
|                                        |
|   • IPlugin (init/start/stop)          |
|   • IPluginFactory (create/destroy)    |
|   • 回调函数类型定义                     |
+---------------------------------------+
                  ^
                  | 实现
                  |
+---------------------------------------+
|             插件层                      |
|        (Plugin Layer)                  |
|                                        |
|   • 磁盘监控插件 (DiskMonitor)          |
|     - statvfs 获取磁盘使用率            |
|     - 定时上报数据                      |
+---------------------------------------+
```

- **接口层**      : 定义插件必须实现的接口（`IPlugin`）类和插件工厂函数签名
- **插件层**      : 实际功能模块，以动态库.dll形式存在,导出插件工厂函数
- **管理层**      : 核心 "PluginManager",通过插件的工厂函数创建销毁插件，负责插件的加载、卸载、状态转换及事件分发
- **宿主应用层**  : 使用者代码,通过 "PluginManager" 控制插件，并接收状态回调

---

## 快速开始

### 依赖

- C++17 编译器 (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.10+
- Linux

### 构建与运行

```bash
git clone https://github.com/csbest/plugin_system.git
cd plugin_framework
mkdir build && cd build
cmake .. 
make 

# 运行示例宿主程序

./bin/main