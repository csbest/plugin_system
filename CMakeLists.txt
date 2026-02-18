# 环境和项目名称
cmake_minimum_required(VERSION 3.10)
project(PluginFramework VERSION 1.0.0 LANGUAGES CXX)

# c++设置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 指定输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 定义框架库
file(GLOB FRAMEWORK_SOURCES "src/*.cpp")
add_library(plugin_framework SHARED ${FRAMEWORK_SOURCES})

# 指定需要包含的头文件路径
target_include_directories(plugin_framework PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 链接需要的库
target_link_libraries(plugin_framework PUBLIC pthread dl)

# 定义主程序
add_executable(main main.cpp)
target_link_libraries(main PRIVATE plugin_framework)

# 定义插件库
set(PLUGIN_SRC plugins/core_monitor/src/CoreMonitorPlugin.cpp)
add_library(CoreMonitor SHARED ${PLUGIN_SRC})

target_include_directories(CoreMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/plugins/core_monitor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(CoreMonitor PUBLIC pthread)
set_target_properties(CoreMonitor PROPERTIES OUTPUT_NAME "CoreMonitor" PREFIX "lib")
