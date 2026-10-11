# 轻量离线学生AI助手 - 顶层 CMake 配置
# 技术栈: C++17 + Qt6 Widgets + llama.cpp(GGUF 推理) + SQLite(Qt Sql)

cmake_minimum_required(VERSION 3.25)

project(StudentAIAssistant VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 源文件为 UTF-8（无 BOM）。MSVC 默认按系统代码页(GBK)解析，
# 必须显式指定 /utf-8，否则中文字符串字面量会被错误解码导致界面乱码。
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Qt 自动生成 moc 代码（Q_OBJECT 类必须）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "构建类型" FORCE)
endif()

# 安装前缀下可覆盖 Qt 位置: -DCMAKE_PREFIX_PATH=<qt根目录>
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Sql Svg)

# ---------------------------------------------------------------------------
# llama.cpp 子模块: third_party/llama.cpp (git submodule, 见 .gitmodules)
# 关闭一切不必要的目标，只保留推理核心库 llama
# ---------------------------------------------------------------------------
set(LLAMA_BUILD_IS_DEV   OFF CACHE BOOL "llama: 关闭开发模式"        FORCE)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "llama: 不构建测试"          FORCE)
set(LLAMA_BUILD_TOOLS    OFF CACHE BOOL "llama: 不构建工具"          FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "llama: 不构建示例"          FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "llama: 不构建 server"       FORCE)
set(LLAMA_BUILD_APP      OFF CACHE BOOL "llama: 不构建统一二进制"    FORCE)
set(LLAMA_BUILD_COMMON   OFF CACHE BOOL "llama: 不构建 common 库"    FORCE)
set(LLAMA_OPENSSL        OFF CACHE BOOL "llama: 不需要 HTTPS"        FORCE)
# Step5 多模态: 独立构建 mtmd 库（视觉编码器推理）。
# 因 LLAMA_BUILD_TOOLS=OFF，此处走 llama.cpp 的 standalone 钩子，
# 只产出 mtmd 静态库，不构建其 CLI 工具。
set(LLAMA_BUILD_MTMD     ON  CACHE BOOL "llama: 构建 mtmd 多模态库"  FORCE)
add_subdirectory(third_party/llama.cpp)

# 本项目路径含中文时，CMake 生成的 unity 汇总文件会把中文路径按本地代码页(GBK)
# 写入，与 MSVC 的 /utf-8 冲突：C4828 -> MSB8084 编译失败。
# llama 目标强制开启 unity 构建，这里关掉以规避（ggml / mtmd 未启用 unity）。
if(MSVC AND TARGET llama)
    set_target_properties(llama PROPERTIES UNITY_BUILD OFF)
endif()

# ---------------------------------------------------------------------------
# 核心静态库: 推理引擎封装 + 工具
# ---------------------------------------------------------------------------
add_library(core STATIC
    src/utils/Logger.cpp
    src/core/LLMEngine.cpp
    src/core/HistoryManager.cpp
    src/core/ImageProcessor.cpp
    src/core/PromptTemplates.cpp
    src/core/ThinkStreamParser.cpp
)
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/common
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/tools/mtmd
)
# ImageProcessor 使用 QPixmap/QImage 生成预览缩略图，需要 Qt6::Gui
target_link_libraries(core PUBLIC Qt6::Core Qt6::Gui Qt6::Network Qt6::Sql llama mtmd)

# ---------------------------------------------------------------------------
# Step1 验收目标: 控制台 hello world（模型加载 + 单轮对话 + 流式输出）
# 用法: hello_world.exe -m <模型路径> [-p "问题"] [-n 最大token] [-t 温度] [-c 上下文]
# ---------------------------------------------------------------------------
add_executable(hello_world src/cli/hello_world.cpp)
target_link_libraries(hello_world PRIVATE core)

# ---------------------------------------------------------------------------
# 主程序：Qt Widgets 桌面应用（Step3 三栏布局 + 聊天界面）
# ---------------------------------------------------------------------------
add_executable(student_ai
    src/main.cpp
    src/ui/SideBar.cpp
    src/ui/ChatWidget.cpp
    src/ui/ChatInputBar.cpp
    src/ui/FunctionPage.cpp
    src/ui/HistoryPanel.cpp
    src/ui/MainWindow.cpp
    src/ui/ThemeManager.cpp
    src/ui/Icons.cpp
    src/ui/SettingsPage.cpp
    src/ui/ThinkBlock.cpp
    src/utils/SystemInfo.cpp
)
target_link_libraries(student_ai PRIVATE core Qt6::Widgets Qt6::Svg)
# GUI 应用：不弹出控制台窗口
set_target_properties(student_ai PROPERTIES WIN32_EXECUTABLE TRUE)
