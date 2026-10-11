# Quik · 轻量离线 AI 学习助手

一个**完全离线**运行的桌面端 AI 学习助手。基于 Qt6 + llama.cpp，模型跑在本机 CPU 上，
不需要联网、不需要独立显卡，打开即用。

适用于：对话答疑、拍照识题、笔记总结、离线翻译、错题解析。

---

## 功能

| 模块 | 说明 |
| --- | --- |
| 对话问答 | 多轮对话；支持粘贴截图 / 拖拽图片提问。回答按「深度思考过程 + 正式答案」双区块展示 |
| 拍照识题 | 上传题目图片，模型先描述图中内容再给出解答，看不清时会明确说明而不是编造 |
| 笔记总结 | 粘贴课堂笔记，按「核心要点 / 重要细节 / 应用场景」输出结构化总结 |
| 离线翻译 | 中英互译，自动判断方向，保持原文段落与列表格式 |
| 错题解析 | 输入题目与自己的错误答案，按「错误原因 / 正确解法 / 知识点总结」分析 |
| 历史记录 | 基于 SQLite 的会话管理，可一键导出为 Markdown |
| 界面 | 深色 / 浅色主题切换、流式打字机输出、全局呼出快捷键 |

### 交互速查

- 发送消息：`Ctrl + Enter`（`Enter` 为换行）
- 全局呼出窗口：`Ctrl + Alt + A`（系统级热键）
- 上传图片：输入栏左下角图片按钮，或直接 `Ctrl + V` 粘贴截图 / 拖拽图片

---

## 技术栈

- **C++17** + **Qt 6 Widgets**（Core / Gui / Widgets / Network / Sql / Svg）
- **llama.cpp**：GGUF 量化模型推理，纯 CPU，启用多后端动态加载（按 CPU 指令集选择最优内核）
- **mtmd**：llama.cpp 的多模态框架，配合 mmproj 视觉编码器实现拍照识题
- **SQLite**（Qt Sql）：会话历史持久化
- 构建：**CMake ≥ 3.25** + **Visual Studio 2022**（MSVC，x64）

---

## 目录结构

```
├── CMakeLists.txt          # 顶层构建配置
├── build.ps1               # 一键构建脚本
├── src/
│   ├── main.cpp            # 程序入口
│   ├── core/               # 推理引擎、提示词模板、流式解析、历史管理等
│   ├── ui/                 # 主窗口、聊天区、功能页、设置页、主题、图标
│   └── utils/              # 日志、系统信息
├── resources/              # 图标与主题资源
├── cmake/                  # 自定义 CMake 查找模块
├── third_party/llama.cpp   # 已内嵌的 llama.cpp 源码（含 mtmd）
└── models/                 # 模型放置目录（.gguf 不入库，见下文）
```

---

## 构建

### 依赖

1. **Visual Studio 2022**，安装「使用 C++ 的桌面开发」工作负载
2. **Qt 6.8.0 msvc2022_64**，可用 `aqt` 安装：

   ```powershell
   pip install aqtinstall
   aqt install-qt windows desktop 6.8.0 win64_msvc2022_64 -O tools\qt
   ```

3. **CMake 3.25+**（VS 自带即可）
4. **llama.cpp**：本仓库已把源码内嵌在 `third_party/llama.cpp`，无需额外下载

### 一键构建

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

脚本默认从 `tools\qt\6.8.0\msvc2022_64` 查找 Qt；Qt 装在别处时请先改 `build.ps1` 里的 `$QtDir`，
或直接使用下面的手动命令。

### 手动构建

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH="<Qt 6.8.0 msvc2022_64 目录>" `
      -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --target student_ai --parallel
```

产物：`build\Release\student_ai.exe`

> **注意**：源码是 UTF-8 无 BOM。MSVC 默认按系统代码页解析，因此 `CMakeLists.txt` 里对 MSVC 强制加了
> `/utf-8`，否则中文字符串会乱码。若你的项目路径包含中文，还需注意 llama.cpp 的 unity 构建会因
> 编码冲突报 `C4828`，本项目的 `CMakeLists.txt` 已对 `llama` 目标关闭 unity 构建来规避。

---

## 准备模型

程序按以下顺序确定模型路径：**设置里保存的路径 → 程序目录下的 `models\` → 弹出文件选择框**。
把 GGUF 放进与 exe 同级的 `models\` 目录即可自动识别，无需任何配置。

| 用途 | 文件 | 参考体积 |
| --- | --- | --- |
| 主模型（必需） | `gemma-4-E4B-it-Q4_K_M.gguf` | 约 5.0 GB |
| 视觉编码器（拍照识题用，可选） | `mmproj-gemma-4-E4B-it-BF16.gguf` | 约 0.95 GB |

主模型取 `models\` 下体积最大的、文件名不含 `mmproj` 的 `*.gguf`；视觉编码器取文件名含
`mmproj` 的那个。两者都可以在「设置 → 模型」中手动指定。

> 模型文件体积过大，未随仓库提供（已在 `.gitignore` 中排除）。任何兼容的 GGUF 模型都可以用，
> 带多模态能力时请一并提供对应的 mmproj 文件。

---

## 运行

```powershell
.\build\Release\student_ai.exe
```

首次启动会加载模型（约十几秒），完成后即可对话。把 `models\` 一起拷到 exe 同级目录就是绿色免安装版。

### 系统要求

- Windows 10 / 11，64 位
- CPU 支持 **AVX2**（2013 年后的 Intel / AMD 桌面 CPU 基本都支持）
- 内存建议 8 GB 以上（模型约 5 GB + 程序约 1 GB）；上下文长度越大占用越高

---

## 数据存放位置

| 内容 | 位置 |
| --- | --- |
| 会话历史（SQLite） | `%APPDATA%\StudentAI\学生AI助手\student_ai.db` |
| 程序设置 | 注册表 `HKEY_CURRENT_USER\Software\StudentAI\学生AI助手` |
| 运行日志 | 程序目录下的 `student_ai.log`（可在 `.gitignore` 覆盖范围内，不入库） |

---

## 开源协议

本项目采用 [MIT License](LICENSE) 开源。

内嵌的 `third_party/llama.cpp` 遵循其自身的 MIT 协议，版权归 llama.cpp 原作者所有。