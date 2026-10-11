#pragma once
// ============================================================================
// LLMEngine: llama.cpp 推理引擎封装（核心模块）
// ----------------------------------------------------------------------------
// 职责:
//   - 模型加载（GGUF, mmap）、上下文管理、采样链、多轮对话
//   - 所有 llama.cpp 调用运行在独立 QThread 中，绝不阻塞 UI 线程
//   - 流式输出：逐 token 通过信号推送到 UI
//   - 支持"停止生成"安全打断
//   - 对话历史超出上下文时自动滑动窗口截断
// 用法:
//   LLMEngine engine;
//   connect(... 信号 ...);
//   engine.loadModelAsync(path);
//   engine.generateAsync("问题");
// ============================================================================

#include <QObject>
#include <QString>
#include <QMutex>
#include <QThread>
#include <memory>
#include <atomic>

// 前向声明 llama.cpp 类型，避免在公共头文件暴露 C API
struct llama_model;
struct llama_context;
struct llama_sampler;
struct llama_vocab;

namespace studentai {

// 采样/生成参数（设置界面可调）
struct GenParams {
    float temperature    = 0.7f;   // 温度：越高越随机，答疑建议 0.7
    float top_p          = 0.9f;   // 核采样
    int   top_k          = 40;     // top-k
    float repeat_penalty = 1.1f;   // 重复惩罚
    int   repeat_last_n  = 64;     // 重复惩罚作用窗口
    int   seed           = -1;     // -1 表示使用随机种子
    int   n_ctx          = 4096;   // 上下文长度（token）
    int   n_predict      = 1024;   // 单次生成最大 token 数
    int   n_threads      = 0;      // 0 = 由 llama.cpp 自动选择
};

// 当前模型信息（状态栏展示用）
struct ModelInfo {
    QString path;         // 模型文件路径
    QString desc;         // 模型描述（来自 GGUF 元数据）
    quint64 nParams = 0;  // 参数量
    quint64 sizeMB = 0;   // 模型文件大小
    bool loaded = false;  // 是否加载成功
};

// 推理引擎：外观类（运行在调用线程），内部将工作转发到推理线程
class LLMEngine : public QObject {
    Q_OBJECT
public:
    explicit LLMEngine(QObject *parent = nullptr);
    ~LLMEngine() override;

    // 异步加载模型（内部会自动先卸载旧模型）
    void loadModelAsync(const QString &modelPath);
    // 异步加载多模态视觉编码器（mmproj GGUF，Step5）；未加载则图片识别不可用
    void loadMmprojAsync(const QString &mmprojPath);
    // 异步生成回复（需先加载模型；正在生成时忽略新请求）
    void generateAsync(const QString &userMessage);
    // 异步生成带图回复（拍照识题，Step5）：imagePath 为本地图片文件
    void generateWithImageAsync(const QString &imagePath, const QString &userMessage);
    // 一次性工具请求（笔记总结/离线翻译/错题解析，Step6）：
    // 在独立干净的上下文里执行，不写入多轮对话历史，也不影响聊天记忆
    void generateOnceAsync(const QString &prompt);
    // 安全打断当前生成
    void stopGenerating();
    // 清空上下文（多轮记忆）并重建采样链
    void resetContext();

    // 生成参数（跨线程安全）
    void setGenParams(const GenParams &params);
    GenParams genParams() const;

    bool isModelLoaded() const;
    bool isGenerating() const;
    bool isMmprojLoaded() const;
    ModelInfo modelInfo() const;

signals:
    // 模型加载进度 0~100，info 为阶段描述
    void modelLoadingProgress(int percent, const QString &info);
    // 模型加载完成（ok=false 时 error 说明原因）
    void modelLoaded(bool ok, const QString &error);
    // 视觉编码器(mmproj)加载完成
    void mmprojLoaded(bool ok, const QString &error);
    // 流式输出：每生成一个 token 片段推送一次
    void tokenGenerated(const QString &piece);
    // 一次生成结束（interrupted=true 表示被用户停止）
    void generationFinished(const QString &fullResponse, bool interrupted, double tokPerSec);
    // 致命错误（不会崩溃，只通知 UI）
    void errorOccurred(const QString &message);

private:
    // 推理线程工作对象：所有 llama.cpp 调用都在这里执行
    class Worker;
    friend class Worker;

    std::unique_ptr<Worker> m_worker;
    QThread m_workerThread;

    // 跨线程共享的可变状态
    struct SharedState {
        std::atomic<bool> stopRequested{false}; // 停止生成标志
        std::atomic<bool> generating{false};    // 是否正在生成
        std::atomic<bool> modelLoaded{false};   // 模型是否已加载
        std::atomic<bool> mmprojLoaded{false};  // 视觉编码器是否已加载
    };
    std::shared_ptr<SharedState> m_state;

    mutable QMutex m_paramsMutex; // 保护生成参数
    GenParams m_params;
    ModelInfo m_modelInfo;
};

} // namespace studentai
