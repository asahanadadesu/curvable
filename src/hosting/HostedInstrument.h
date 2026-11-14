#pragma once

#include "hosting/HostingContext.h"

#include <atomic>
#include <functional>

namespace curvable
{

/** 在 Curvable 内部托管一个音源插件。

    线程约定：
    - load / unload / restoreState / showEditor 等：只在消息线程调用。
    - prepare / release：由宿主在 prepareToPlay / releaseResources 中调用。
    - process：音频线程。不会阻塞；正在切换插件时输出静音。

    状态变化（加载、卸载、出错）时会发送 change 消息。
*/
class HostedInstrument final : public juce::ChangeBroadcaster
{
public:
    explicit HostedInstrument (HostingContext&);
    ~HostedInstrument() override;

    //==============================================================================
    // 消息线程

    using LoadCallback = std::function<void (const juce::String& error)>;

    /** 异步加载插件，替换当前插件。完成后（成功时 error 为空）调用 onDone。 */
    void load (const juce::PluginDescription&, LoadCallback onDone = {});

    void unload();

    bool isLoaded() const;

    /** 工程里记录了某个音源，但在本机找不到或加载失败。此时原样保留其状态，再次保存时不会丢失。 */
    bool isMissing() const noexcept { return missingState.isValid(); }

    /** 当前（或缺失的）音源名称。 */
    juce::String getPluginName() const;

    juce::String getLastError() const { return lastError; }

    juce::String getLoadedIdentifier() const;

    std::optional<juce::PluginDescription> getLoadedDescription() const;

    void showEditor();
    void closeEditor();
    bool isEditorShowing() const noexcept { return editorWindow != nullptr; }

    /** 保存为 ValueTree（类型为 "HostedPlugin"）；没有音源时返回无效的 ValueTree。 */
    juce::ValueTree saveState() const;

    /** 恢复状态。在消息线程上会同步加载，否则投递到消息线程异步加载。 */
    void restoreState (const juce::ValueTree&);

    //==============================================================================
    // 任意线程

    int getLatencySamples() const noexcept     { return latencySamples.load(); }
    double getTailLengthSeconds() const noexcept { return tailSeconds.load(); }

    //==============================================================================
    // 宿主回调

    void prepare (double sampleRate, int maxBlockSize);
    void release();

    /** 音频线程：把 midi 交给音源，音源输出写入 output（已有内容会被覆盖）。处理后清空 midi。 */
    void process (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi,
                  juce::AudioPlayHead* playHead, bool isNonRealtime) noexcept;

    static constexpr auto stateType = "HostedPlugin";

private:
    void install (std::unique_ptr<juce::AudioPluginInstance>, const juce::MemoryBlock* stateToRestore);
    void restoreNow (const juce::ValueTree&);
    void setError (const juce::String&);
    static void configureBuses (juce::AudioPluginInstance&);
    static int scratchChannelsFor (const juce::AudioPluginInstance&);

    HostingContext& context;

    /** 只在交换指针时短暂持有；音频线程用 try-lock，不会被阻塞。 */
    juce::CriticalSection audioLock;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    juce::AudioBuffer<float> scratch;
    bool lastNonRealtime = false; // 仅音频线程访问

    double sampleRate = 44100.0;
    int blockSize = 512;
    bool prepared = false;

    std::atomic<int> latencySamples { 0 };
    std::atomic<double> tailSeconds { 0.0 };

    juce::ValueTree missingState;
    juce::String lastError;
    int loadGeneration = 0; ///< 让过时的异步加载结果作废

    std::unique_ptr<juce::DocumentWindow> editorWindow;

    JUCE_DECLARE_WEAK_REFERENCEABLE (HostedInstrument)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedInstrument)
};

} // namespace curvable
