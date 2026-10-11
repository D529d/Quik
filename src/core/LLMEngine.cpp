#include "core/LLMEngine.h"

#include "utils/Logger.h"

#include "llama.h"
#include "mtmd.h"
#include "mtmd-helper.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>

#include <algorithm>
#include <cstring>
#include <random>
#include <vector>

namespace studentai {

// ============================================================================
// LLMEngine::Worker：推理线程工作对象
// 所有 llama.cpp 调用都发生在 worker 线程，UI 线程永不阻塞。
// ============================================================================
class LLMEngine::Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(LLMEngine *engine, std::shared_ptr<SharedState> state)
        : m_engine(engine), m_state(std::move(state)) {}

    ~Worker() override {
        freeResources();
    }

public slots:
    // 异步加载模型
    void onLoadModel(const QString &modelPath) {
        freeResources();
        // 重新加载主模型时，视觉编码器需重新绑定（其依赖主模型词汇表）
        freeMmproj();
        m_modelPath = modelPath;
        m_messages.clear();
        m_prevLen = 0;

        llama_model_params mparams = llama_model_default_params();
        // mmap 映射加载，减少内存占用（默认即 mmap）
        mparams.load_mode = LLAMA_LOAD_MODE_MMAP;
        mparams.n_gpu_layers = 0; // 纯 CPU 推理
        mparams.progress_callback = [](float progress, void *ud) -> bool {
            auto *self = static_cast<Worker *>(ud);
            emit self->modelProgress(static_cast<int>(progress * 100.0f),
                                     QStringLiteral("正在加载模型 %1%").arg(qRound(progress * 100.0f)));
            return true; // 返回 true 继续加载
        };
        mparams.progress_callback_user_data = this;

        // 阶段性进度提示
        emit modelProgress(1, QStringLiteral("正在读取模型文件..."));
        m_model = llama_model_load_from_file(modelPath.toUtf8().constData(), mparams);
        if (!m_model) {
            const QString err = QStringLiteral("模型加载失败，请检查文件路径是否正确：%1").arg(modelPath);
            LOG_ERROR(err);
            emit modelReady(false, err);
            return;
        }

        emit modelProgress(40, QStringLiteral("模型文件读取完成，正在初始化上下文..."));

        m_vocab = llama_model_get_vocab(m_model);

        llama_context_params cparams = llama_context_default_params();
        cparams.n_ctx = static_cast<uint32_t>(m_engine->genParams().n_ctx);
        cparams.n_batch = 512;            // 批大小
        cparams.n_threads = static_cast<int32_t>(m_engine->genParams().n_threads);
        // CPU 解码中断回调：与"停止生成"联动
        cparams.abort_callback = [](void *data) -> bool {
            auto *st = static_cast<SharedState *>(data);
            return st->stopRequested.load();
        };
        cparams.abort_callback_data = m_state.get();

        m_ctx = llama_init_from_model(m_model, cparams);
        if (!m_ctx) {
            const QString err = QStringLiteral("上下文初始化失败（可能内存不足）");
            LOG_ERROR(err);
            emit modelReady(false, err);
            return;
        }

        emit modelProgress(70, QStringLiteral("上下文初始化完成，正在创建采样器..."));

        // 采样链：重复惩罚 -> top-k -> top-p -> 温度 -> 最终采样
        m_smpl = buildSampler();
        if (!m_smpl) {
            emit modelReady(false, QStringLiteral("采样器创建失败"));
            return;
        }

        // 读取模型自带的聊天模板（GGUF 元数据）
        const char *tmpl = llama_model_chat_template(m_model, nullptr);
        m_template = tmpl ? tmpl : "";
        if (m_template.empty()) {
            // 模型未内嵌模板时回退到 Gemma 4 官方模板
            m_template =
                "{{- bos_token }}\n"
                "{% for message in messages %}\n"
                "{{- '<start_of_turn>' + message['role'] + '\\n' + message['content'] + '<end_of_turn>\\n' }}\n"
                "{% endfor %}\n"
                "{{- '<start_of_turn>model\\n' }}";
        }
        LOG_DEBUG(QString("聊天模板: %1").arg(QString::fromStdString(m_template).left(120)));

        emit modelProgress(90, QStringLiteral("正在读取模型信息..."));

        // 汇总模型信息
        ModelInfo info;
        info.path = modelPath;
        char desc[512] = {0};
        if (llama_model_desc(m_model, desc, sizeof(desc)) > 0) {
            info.desc = QString::fromUtf8(desc);
        }
        info.nParams = llama_model_n_params(m_model);
        info.sizeMB = QFileInfo(modelPath).size() / (1024 * 1024);
        info.loaded = true;

        m_state->modelLoaded = true;
        m_engine->m_modelInfo = info;

        emit modelProgress(100, QStringLiteral("模型加载完成"));
        emit modelReady(true, QString());
    }

    // 异步加载多模态视觉编码器（mmproj）
    void onLoadMmproj(const QString &mmprojPath) {
        freeMmproj(); // 释放旧的视觉编码器
        if (mmprojPath.isEmpty()) {
            m_state->mmprojLoaded = false;
            emit mmprojStatus(false, QStringLiteral("未指定视觉编码器路径"));
            return;
        }
        if (!m_model) {
            m_state->mmprojLoaded = false;
            emit mmprojStatus(false, QStringLiteral("请先加载主模型再加载视觉编码器"));
            return;
        }

        mtmd_context_params mparams = mtmd_context_params_default();
        mparams.use_gpu = false;            // 纯 CPU 推理
        mparams.n_threads = static_cast<int32_t>(m_engine->genParams().n_threads);
        mparams.print_timings = false;

        emit modelProgress(1, QStringLiteral("正在加载视觉编码器..."));
        m_mtmd = mtmd_init_from_file(mmprojPath.toUtf8().constData(), m_model, mparams);
        if (!m_mtmd) {
            m_state->mmprojLoaded = false;
            const QString err = QStringLiteral("视觉编码器加载失败，请检查 mmproj 文件是否正确");
            LOG_ERROR(err);
            emit mmprojStatus(false, err);
            return;
        }
        m_mmprojPath = mmprojPath;
        m_state->mmprojLoaded = true;
        LOG_DEBUG(QString("视觉编码器加载成功: %1").arg(mmprojPath));
        emit modelProgress(100, QStringLiteral("视觉编码器加载完成"));
        emit mmprojStatus(true, QString());
    }

    // 生成带图回复（拍照识题，走 mtmd 多模态路径）
    void onGenerateImage(const QString &imagePath, const QString &userMessage) {
        if (!m_model || !m_ctx) {
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("模型尚未加载"));
            return;
        }
        if (!m_mtmd) {
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("视觉编码器(mmproj)未加载，无法识别图片，请在设置中配置"));
            return;
        }
        if (m_state->generating.load()) {
            return;
        }
        m_state->stopRequested = false;
        m_state->generating = true;

        GenParams params = m_engine->genParams();
        refreshSampler(); // 应用设置页最新的温度/Top-P

        // mtmd 多模态为整段重放（图片必须按标记位置编码进 KV），
        // 因此带图生成不复用增量解码路径，而是清空 KV 后整段重放。
        // 清空后立即置脏：无论本次成功还是中途失败，下一轮都必须整段重放。
        llama_memory_clear(llama_get_memory(m_ctx), true);
        m_prevLen = 0;
        m_kvNeedsReset = true;

        // 追加带图用户消息：媒体标记置于文本之前（大多数模型要求）
        const char *marker = mtmd_default_marker();
        const std::string userContent =
            std::string(marker) + userMessage.toUtf8().constData();
        addMessage("user", QString::fromUtf8(userContent.c_str(),
                                            static_cast<int>(userContent.size())));

        // 用聊天模板得到完整 prompt（媒体标记稍后由 mtmd 替换为图像嵌入）
        std::string prompt;
        buildPrompt(prompt, /*addAssistant=*/true);

        // 从文件解码位图；llama.cpp 内部会按视觉模型要求自动缩放并转 RGB
        mtmd_helper_init_opt opt = mtmd_helper_init_opt_default();
        mtmd_helper_bitmap_wrapper bw =
            mtmd_helper_bitmap_init_from_file(m_mtmd, imagePath.toUtf8().constData(),
                                              /*placeholder=*/false, opt);
        if (!bw.bitmap) {
            removeLastMessage();
            m_state->generating = false;
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("图片解码失败，请换一张图片试试：%1").arg(imagePath));
            return;
        }

        // 按媒体标记切分 prompt，得到 [文本段, 位图, 文本段, ...] 交错序列
        const std::string markerStr(marker);
        std::vector<std::string> segments;
        {
            size_t start = 0, pos = 0;
            while ((pos = prompt.find(markerStr, start)) != std::string::npos) {
                segments.push_back(prompt.substr(start, pos - start));
                start = pos + markerStr.size();
            }
            segments.push_back(prompt.substr(start));
        }

        // 本次仅 1 张图，切分后文本段数应为 2
        const size_t nBitmaps = 1;
        std::vector<mtmd_input_text> texts;
        std::vector<mtmd_input_part> parts;
        std::vector<const mtmd_input_part *> partsPtr;
        // 预留容量：下方取元素地址，扩容会使已存指针失效
        texts.reserve(segments.size());
        parts.reserve(segments.size() * 2);
        partsPtr.reserve(segments.size() * 2);

        for (size_t i = 0; i < segments.size(); ++i) {
            mtmd_input_text t{segments[i].data(), segments[i].size(),
                              /*add_special=*/false, /*parse_special=*/true};
            texts.push_back(t);
            parts.push_back({&texts.back(), nullptr});
            partsPtr.push_back(&parts.back());
            if (i < nBitmaps) {
                parts.push_back({nullptr, bw.bitmap});
                partsPtr.push_back(&parts.back());
            }
        }

        // tokenize 为「文本 + 图像」chunk 序列
        std::unique_ptr<mtmd_input_chunks, void (*)(mtmd_input_chunks *)> chunks(
            mtmd_input_chunks_init(), mtmd_input_chunks_free);
        const int32_t tokRes = mtmd_tokenize_from_parts(m_mtmd, chunks.get(),
                                                        partsPtr.data(), partsPtr.size(),
                                                        /*add_special=*/true);
        if (tokRes != 0) {
            mtmd_bitmap_free(bw.bitmap);
            removeLastMessage();
            m_state->generating = false;
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("多模态提示词解析失败（错误码 %1）").arg(tokRes));
            return;
        }

        // 逐 chunk 求值：文本直接解码，图像先编码为嵌入再解码进主模型
        const int32_t nBatch = 512;
        llama_pos nPast = 0;
        const size_t nChunks = mtmd_input_chunks_size(chunks.get());
        for (size_t i = 0; i < nChunks; ++i) {
            if (m_state->stopRequested.load()) {
                break;
            }
            const mtmd_input_chunk *chunk = mtmd_input_chunks_get(chunks.get(), i);
            const bool isLast = (i == nChunks - 1);
            llama_pos newNPast = nPast;
            int32_t res = 0;
            if (mtmd_input_chunk_get_type(chunk) == MTMD_INPUT_CHUNK_TYPE_TEXT) {
                res = mtmd_helper_eval_chunk_single(m_mtmd, m_ctx, chunk, nPast, 0,
                                                    nBatch, isLast, &newNPast);
            } else {
                res = mtmd_encode_chunk(m_mtmd, chunk);
                if (res == 0) {
                    res = mtmd_helper_decode_image_chunk(m_mtmd, m_ctx, chunk,
                                                         mtmd_get_output_embd(m_mtmd),
                                                         nPast, 0, nBatch,
                                                         &newNPast, nullptr, nullptr);
                }
            }
            if (res != 0) {
                mtmd_bitmap_free(bw.bitmap);
                removeLastMessage();
                m_state->generating = false;
                emit generationDone(QString(), true, 0.0);
                emit errorMessage(QStringLiteral("图片识别推理失败，可能是图片过大或内存不足"));
                return;
            }
            nPast = newNPast;
        }

        mtmd_bitmap_free(bw.bitmap);
        // 图像嵌入已进入 KV；消息中仅保留不含标记的纯文本，
        // 以免后续轮次出现「有标记无位图」导致 tokenize 失败。
        stripMarkerFromLastMessage(markerStr);

        emit generationStarted();
        QString response;
        bool interrupted = false;
        double tokPerSec = 0.0;
        sampleLoop(params, response, interrupted, tokPerSec);

        if (!interrupted && !response.isEmpty()) {
            addMessage("assistant", response);
        }
        m_state->generating = false;
        emit generationDone(response, interrupted, tokPerSec);
    }

    // 生成回复（流式）
    void onGenerate(const QString &userMessage) {
        if (!m_model || !m_ctx) {
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("模型尚未加载"));
            return;
        }
        if (m_state->generating.load()) {
            return; // 正在生成，忽略新请求
        }
        m_state->stopRequested = false;
        m_state->generating = true;

        GenParams params = m_engine->genParams();
        refreshSampler(); // 应用设置页最新的温度/Top-P

        // 1. 追加用户消息
        addMessage("user", userMessage);

        // 2. 应用聊天模板得到完整提示词
        std::string prompt;
        int newLen = buildPrompt(prompt, /*addAssistant=*/true);

        // 3. 滑动窗口：超出上下文预算时丢弃最早消息并重建
        bool trimmed = false;
        while (newLen > 0 && estimateTokens(prompt) > params.n_ctx - 128) {
            trimOldestPair();
            newLen = buildPrompt(prompt, /*addAssistant=*/true);
            trimmed = true;
            LOG_WARN("上下文超限，滑动窗口已丢弃最早的一轮对话");
        }
        if (trimmed || m_kvNeedsReset) {
            // KV 缓存与当前提示词已不对应：滑动窗口丢弃过消息，
            // 或上一轮是图片识别（图片被编码为嵌入，与文本提示词字节不再对应）。
            // 两者都需要清空 KV 后整段重放。
            llama_memory_clear(llama_get_memory(m_ctx), true);
            m_prevLen = 0;
            m_kvNeedsReset = false;
            newLen = buildPrompt(prompt, /*addAssistant=*/true);
        }

        // 4. 只解码提示词中新增的部分（增量解码，复用 KV 缓存）
        std::string delta = (m_prevLen > 0 && !trimmed && newLen > m_prevLen)
                                ? prompt.substr(static_cast<size_t>(m_prevLen))
                                : prompt;
        m_prevLen = newLen;

        if (delta.empty()) {
            LOG_ERROR("提示词为空，可能是聊天模板处理失败");
            m_state->generating = false;
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("提示词构建失败：聊天模板处理异常"));
            return;
        }
        LOG_DEBUG(QString("提示词 delta %1 字节, 完整 %2 字节").arg(delta.size()).arg(prompt.size()));

        emit generationStarted();

        // 5. 推理循环（与多模态共用）
        bool interrupted = false;
        QString response;
        double tokPerSec = 0.0;
        if (!decodeTokens(delta)) {
            interrupted = m_state->stopRequested.load();
        } else {
            sampleLoop(params, response, interrupted, tokPerSec);
        }

        // 6. 收尾
        if (!interrupted && !response.isEmpty()) {
            addMessage("assistant", response);
        }

        m_state->generating = false;
        emit generationDone(response, interrupted, tokPerSec);
    }

    // 一次性工具请求（笔记总结/离线翻译/错题解析）
    // 先把多轮对话记忆暂存起来，用空上下文执行本次提示词，结束再恢复：
    // 这样工具既不受聊天历史干扰，也不会污染或清空聊天记忆。
    void onGenerateOnce(const QString &userPrompt) {
        if (!m_model || !m_ctx) {
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("模型尚未加载"));
            return;
        }
        if (m_state->generating.load()) {
            return;
        }
        m_state->stopRequested = false;
        m_state->generating = true;

        const GenParams params = m_engine->genParams();
        refreshSampler(); // 应用设置页最新的温度/Top-P

        // 暂存对话记忆，置空后本次请求即运行在干净上下文
        std::vector<llama_chat_message> saved;
        saved.swap(m_messages);
        llama_memory_clear(llama_get_memory(m_ctx), true);
        m_prevLen = 0;

        addMessage("user", userPrompt);

        std::string prompt;
        const int newLen = buildPrompt(prompt, /*addAssistant=*/true);

        // 工具类输入（尤其笔记总结）可能很长，超出上下文时给出明确提示，
        // 而不是让模型在截断的内容上生成错误结果
        bool interrupted = false;
        QString response;
        double tokPerSec = 0.0;
        const bool tooLong = newLen <= 0
                             || estimateTokens(prompt) > params.n_ctx - 256;

        if (tooLong) {
            freeMessages();
            m_messages.swap(saved);
            m_kvNeedsReset = true;
            m_state->generating = false;
            emit generationDone(QString(), true, 0.0);
            emit errorMessage(QStringLiteral("内容太长，超出模型上下文（当前 %1 tokens），"
                                             "请分段处理或调大上下文长度")
                                  .arg(params.n_ctx));
            return;
        }

        emit generationStarted();
        // 干净上下文：直接整段解码提示词
        if (decodeTokens(prompt)) {
            sampleLoop(params, response, interrupted, tokPerSec);
        } else {
            interrupted = m_state->stopRequested.load();
        }

        // 释放本次请求产生的消息，恢复对话记忆；
        // KV 里现在是工具请求的内容，标记为脏以便下一轮对话整段重放
        freeMessages();
        m_messages.swap(saved);
        m_kvNeedsReset = true;

        m_state->generating = false;
        emit generationDone(response, interrupted, tokPerSec);
    }

    void onStop() {
        m_state->stopRequested = true;
    }

    void onReset() {
        if (m_ctx) {
            llama_memory_clear(llama_get_memory(m_ctx), true);
        }
        m_messages.clear();
        m_prevLen = 0;
        m_kvNeedsReset = false;
    }

signals:
    void modelProgress(int percent, const QString &info);
    void modelReady(bool ok, const QString &error);
    void mmprojStatus(bool ok, const QString &error);
    void generationStarted();
    void tokenPiece(const QString &piece);
    void generationDone(const QString &fullResponse, bool interrupted, double tokPerSec);
    void errorMessage(const QString &message);

private:
    // 通用采样生成循环：采样 + 流式推送 + 增量解码，直到 EOG/停止/超限
    // response 收集完整文本，interrupted 标记是否被用户停止，返回 true 表示正常结束
    bool sampleLoop(const GenParams &params, QString &response,
                    bool &interrupted, double &tokPerSec) {
        int nPredicted = 0;
        const int64_t tStart = llama_time_us();
        interrupted = false;

        if (!m_smpl) {
            return false;
        }
        while (nPredicted < params.n_predict) {
            if (m_state->stopRequested.load()) {
                interrupted = true;
                break;
            }
            llama_token id = llama_sampler_sample(m_smpl, m_ctx, -1);
            if (llama_vocab_is_eog(m_vocab, id)) {
                break;
            }
            char buf[256];
            const int n = llama_token_to_piece(m_vocab, id, buf, sizeof(buf), 0, true);
            if (n < 0) {
                break;
            }
            const QString piece = QString::fromUtf8(buf, n);
            response += piece;

            // 回合结束符：当模型内嵌的 Jinja 模板无法被 llama.cpp 解析、
            // 回退到手动拼接格式时，模型很容易越过本轮继续臆造
            // "<start_of_turn>user …</start_of_turn>" 之类的后续对话。
            // llama_vocab_is_eog() 只挡真正的 EOG token，挡不住这种被当成
            // 普通文本生成出来的标记，所以这里必须显式检测并截断。
            static const QString kTurnEnd = QStringLiteral("<end_of_turn>");
            if (response.endsWith(kTurnEnd)) {
                response.chop(kTurnEnd.size()); // 标记本身不进入正文
                break;                          // 该片段也不推给 UI
            }

            emit tokenPiece(piece);
            ++nPredicted;

            llama_batch b = llama_batch_get_one(&id, 1);
            if (llama_decode(m_ctx, b) != 0) {
                if (m_state->stopRequested.load()) {
                    interrupted = true;
                } else {
                    emit errorMessage(QStringLiteral("解码失败（可能上下文已满）"));
                }
                break;
            }
        }

        const double elapsed = (llama_time_us() - tStart) / 1e6;
        tokPerSec = elapsed > 0 ? nPredicted / elapsed : 0.0;
        return true;
    }
    // 按当前参数重建采样链。
    // 采样链在加载模型时构建一次，若不重建，设置页调整温度/Top-P 后不会生效。
    void refreshSampler() {
        if (m_smpl) {
            llama_sampler_free(m_smpl);
            m_smpl = nullptr;
        }
        m_smpl = buildSampler();
    }

    // 构建采样链
    llama_sampler *buildSampler() {
        const GenParams &p = m_engine->genParams();
        llama_sampler *chain = llama_sampler_chain_init(llama_sampler_chain_default_params());
        const int32_t nVocab = llama_vocab_n_tokens(m_vocab);
        const int32_t repeatLastN = p.repeat_last_n > 0 ? p.repeat_last_n : 64;
        // 顺序：penalties -> top_k -> top_p -> temp -> dist
        llama_sampler_chain_add(chain, llama_sampler_init_penalties(nVocab, repeatLastN, p.repeat_penalty, 0.0f, 0.0f));
        if (p.top_k > 0) {
            llama_sampler_chain_add(chain, llama_sampler_init_top_k(p.top_k));
        }
        if (p.top_p > 0.0f && p.top_p < 1.0f) {
            llama_sampler_chain_add(chain, llama_sampler_init_top_p(p.top_p, 1));
        }
        llama_sampler_chain_add(chain, llama_sampler_init_temp(p.temperature));
        if (p.seed >= 0) {
            llama_sampler_chain_add(chain, llama_sampler_init_dist(static_cast<uint32_t>(p.seed)));
        } else {
            std::random_device rd;
            llama_sampler_chain_add(chain, llama_sampler_init_dist(rd()));
        }
        return chain;
    }

    // 追加一条消息（拷贝字符串，释放时统一处理）
    void addMessage(const char *role, const QString &content) {
        llama_chat_message msg;
        msg.role = strdup(role);
        msg.content = strdup(content.toUtf8().constData());
        m_messages.push_back(msg);
    }

    // 释放全部消息内存并清空（消息内容为 strdup 分配）
    void freeMessages() {
        for (auto &msg : m_messages) {
            free(const_cast<char *>(msg.role));
            free(const_cast<char *>(msg.content));
        }
        m_messages.clear();
    }

    // 移除最后一条消息（多模态失败回滚时用），并释放其内存
    void removeLastMessage() {
        if (m_messages.empty()) {
            return;
        }
        free(const_cast<char *>(m_messages.back().role));
        free(const_cast<char *>(m_messages.back().content));
        m_messages.pop_back();
    }

    // 去掉最后一条消息内容中的媒体标记（保留纯文本，供后续轮次使用）
    void stripMarkerFromLastMessage(const std::string &marker) {
        if (m_messages.empty()) {
            return;
        }
        std::string content(m_messages.back().content);
        size_t pos = content.find(marker);
        while (pos != std::string::npos) {
            content.erase(pos, marker.size());
            pos = content.find(marker, pos);
        }
        free(const_cast<char *>(m_messages.back().content));
        m_messages.back().content = strdup(content.c_str());
    }

    // 应用聊天模板，得到完整提示词；返回格式化后的字节长度
    int buildPrompt(std::string &outPrompt, bool addAssistant) {
        // 优先使用模型自带模板（Jinja）；失败时回退到手动拼接
        if (!m_template.empty()) {
            // 先试算长度
            int len = llama_chat_apply_template(m_template.c_str(), m_messages.data(),
                                                m_messages.size(), addAssistant, nullptr, 0);
            if (len > 0) {
                std::vector<char> buf(static_cast<size_t>(len) + 8);
                int real = llama_chat_apply_template(m_template.c_str(), m_messages.data(),
                                                     m_messages.size(), addAssistant,
                                                     buf.data(), static_cast<int32_t>(buf.size()));
                if (real > 0) {
                    outPrompt.assign(buf.data(), static_cast<size_t>(real));
                    return real;
                }
            } else {
                LOG_WARN("模型自带聊天模板解析失败（llama.cpp 可能暂不支持该 Jinja 模板），回退到 Gemma 4 手动格式");
            }
        }
        // 回退：手动拼接 Gemma 4 官方对话格式
        std::string s;
        for (const auto &m : m_messages) {
            s += "<start_of_turn>";
            s += m.role;
            s += "\n";
            s += m.content;
            s += "<end_of_turn>\n";
        }
        if (addAssistant) {
            s += "<start_of_turn>model\n";
        }
        outPrompt = std::move(s);
        return static_cast<int>(outPrompt.size());
    }

    // 解码一段文本（作为提示词输入）
    bool decodeTokens(const std::string &text) {
        if (text.empty()) {
            return true;
        }
        // 首次（KV 为空）时加 BOS
        const bool isFirst = llama_memory_seq_pos_max(llama_get_memory(m_ctx), 0) == -1;
        const int nTokens = -llama_tokenize(m_vocab, text.c_str(), static_cast<int32_t>(text.size()),
                                            nullptr, 0, isFirst, true);
        LOG_DEBUG(QString("提示词解码: %1 tokens").arg(nTokens));
        if (nTokens <= 0) {
            return true;
        }
        std::vector<llama_token> tokens(static_cast<size_t>(nTokens));
        if (llama_tokenize(m_vocab, text.c_str(), static_cast<int32_t>(text.size()),
                           tokens.data(), static_cast<int32_t>(tokens.size()), isFirst, true) < 0) {
            return false;
        }
        llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
        return llama_decode(m_ctx, batch) == 0;
    }

    // 估计一段文本的 token 数
    int estimateTokens(const std::string &text) const {
        if (text.empty() || !m_vocab) {
            return 0;
        }
        return -llama_tokenize(m_vocab, text.c_str(), static_cast<int32_t>(text.size()),
                               nullptr, 0, true, true);
    }

    // 丢弃最早的一轮（user + assistant）
    void trimOldestPair() {
        // 至少保留一条 user 消息
        while (m_messages.size() > 1) {
            std::string role(m_messages.front().role);
            if (role != "user") {
                break;
            }
            for (int i = 0; i < 2 && !m_messages.empty(); ++i) {
                free(const_cast<char *>(m_messages.front().role));
                free(const_cast<char *>(m_messages.front().content));
                m_messages.erase(m_messages.begin());
            }
            break;
        }
    }

    // 释放所有 llama.cpp 资源
    void freeResources() {
        freeMmproj();
        if (m_smpl) {
            llama_sampler_free(m_smpl);
            m_smpl = nullptr;
        }
        if (m_ctx) {
            llama_free(m_ctx);
            m_ctx = nullptr;
        }
        if (m_model) {
            llama_model_free(m_model);
            m_model = nullptr;
        }
        m_vocab = nullptr;
        freeMessages();
        m_prevLen = 0;
        m_kvNeedsReset = false;
        m_state->modelLoaded = false;
        m_engine->m_modelInfo = ModelInfo{};
    }

    // 释放视觉编码器（mtmd）
    void freeMmproj() {
        if (m_mtmd) {
            mtmd_free(m_mtmd);
            m_mtmd = nullptr;
        }
        m_mmprojPath.clear();
        m_state->mmprojLoaded = false;
    }

private:
    LLMEngine *m_engine = nullptr;
    std::shared_ptr<SharedState> m_state;

    llama_model   *m_model = nullptr;
    llama_context *m_ctx = nullptr;
    llama_sampler *m_smpl = nullptr;
    const llama_vocab *m_vocab = nullptr;
    mtmd_context *m_mtmd = nullptr;       // 多模态视觉编码器（mmproj）

    QString m_modelPath;
    QString m_mmprojPath;                 // 视觉编码器路径
    std::string m_template;                 // 聊天模板
    std::vector<llama_chat_message> m_messages; // 对话消息
    int m_prevLen = 0;                      // 上次格式化后的提示词长度
    // KV 缓存与文本提示词不再对应（上一轮为图片识别）时为 true，
    // 下一轮需清空 KV 后整段重放，避免增量解码重复追加历史
    bool m_kvNeedsReset = false;
};

// ============================================================================
// LLMEngine：外观实现
// ============================================================================

LLMEngine::LLMEngine(QObject *parent)
    : QObject(parent)
    , m_state(std::make_shared<SharedState>()) {

    // llama.cpp 日志接入本应用日志系统
    llama_log_set([](enum ggml_log_level level, const char *text, void *) {
        if (!text) {
            return;
        }
        switch (level) {
        case GGML_LOG_LEVEL_ERROR:
            LOG_ERROR(QString::fromUtf8(text).trimmed());
            break;
        case GGML_LOG_LEVEL_WARN:
            LOG_WARN(QString::fromUtf8(text).trimmed());
            break;
        default:
            LOG_DEBUG(QString::fromUtf8(text).trimmed());
            break;
        }
    }, nullptr);

    // 初始化动态后端（CPU 直接可跑）
    ggml_backend_load_all();

    // 创建工作线程
    m_worker = std::make_unique<Worker>(this, m_state);
    m_worker->moveToThread(&m_workerThread);

    // worker 信号 -> 引擎信号（跨线程自动排队，保证 UI 线程安全）
    connect(m_worker.get(), &Worker::modelProgress, this, &LLMEngine::modelLoadingProgress);
    connect(m_worker.get(), &Worker::modelReady, this, &LLMEngine::modelLoaded);
    connect(m_worker.get(), &Worker::mmprojStatus, this, &LLMEngine::mmprojLoaded);
    connect(m_worker.get(), &Worker::tokenPiece, this, &LLMEngine::tokenGenerated);
    connect(m_worker.get(), &Worker::generationDone, this, &LLMEngine::generationFinished);
    connect(m_worker.get(), &Worker::errorMessage, this, &LLMEngine::errorOccurred);

    // 对外接口（本线程调用） -> worker（排队到推理线程）
    // 通过 invokeMethod 跨线程排队，保证 llama.cpp 只在推理线程执行

    m_workerThread.start();
}

LLMEngine::~LLMEngine() {
    m_state->stopRequested = true;
    m_workerThread.quit();
    m_workerThread.wait();
    m_worker.reset(); // worker 析构时释放 llama.cpp 资源
}

void LLMEngine::loadModelAsync(const QString &modelPath) {
    if (m_state->generating.load()) {
        m_state->stopRequested = true;
    }
    QMetaObject::invokeMethod(m_worker.get(), "onLoadModel", Qt::QueuedConnection,
                              Q_ARG(QString, modelPath));
}

void LLMEngine::loadMmprojAsync(const QString &mmprojPath) {
    QMetaObject::invokeMethod(m_worker.get(), "onLoadMmproj", Qt::QueuedConnection,
                              Q_ARG(QString, mmprojPath));
}

void LLMEngine::generateWithImageAsync(const QString &imagePath, const QString &userMessage) {
    if (!m_state->mmprojLoaded.load()) {
        emit errorOccurred(QStringLiteral("视觉编码器(mmproj)未加载，请先在设置中配置 mmproj 路径"));
        return;
    }
    QMetaObject::invokeMethod(m_worker.get(), "onGenerateImage", Qt::QueuedConnection,
                              Q_ARG(QString, imagePath), Q_ARG(QString, userMessage));
}

void LLMEngine::generateAsync(const QString &userMessage) {
    if (!m_state->modelLoaded.load()) {
        emit errorOccurred(QStringLiteral("模型尚未加载，请先在设置中选择模型"));
        return;
    }
    QMetaObject::invokeMethod(m_worker.get(), "onGenerate", Qt::QueuedConnection,
                              Q_ARG(QString, userMessage));
}

void LLMEngine::generateOnceAsync(const QString &prompt) {
    if (!m_state->modelLoaded.load()) {
        emit errorOccurred(QStringLiteral("模型尚未加载，请先在设置中选择模型"));
        return;
    }
    QMetaObject::invokeMethod(m_worker.get(), "onGenerateOnce", Qt::QueuedConnection,
                              Q_ARG(QString, prompt));
}

void LLMEngine::stopGenerating() {
    m_state->stopRequested = true;
}

void LLMEngine::resetContext() {
    QMetaObject::invokeMethod(m_worker.get(), &Worker::onReset, Qt::QueuedConnection);
}

void LLMEngine::setGenParams(const GenParams &params) {
    QMutexLocker locker(&m_paramsMutex);
    m_params = params;
}

GenParams LLMEngine::genParams() const {
    QMutexLocker locker(&m_paramsMutex);
    return m_params;
}

bool LLMEngine::isModelLoaded() const {
    return m_state->modelLoaded.load();
}

bool LLMEngine::isGenerating() const {
    return m_state->generating.load();
}

bool LLMEngine::isMmprojLoaded() const {
    return m_state->mmprojLoaded.load();
}

ModelInfo LLMEngine::modelInfo() const {
    return m_modelInfo;
}

} // namespace studentai

#include "LLMEngine.moc"
