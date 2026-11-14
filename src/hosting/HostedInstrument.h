#pragma once

#include "hosting/HostingContext.h"

#include <array>
#include <atomic>
#include <bitset>
#include <functional>

namespace curvable
{

/** 在 Curvable 内部托管一个音源插件，并按需复制成多个实例（每个声部一个）。

    第 0 个实例是"主实例"：用户看到和操作的就是它，保存的也是它的状态。其余是副本（clone）：
    - 按 setWantedInstances 在后台逐个创建（异步，不阻塞界面），创建时复制主实例的状态；
    - 主实例的参数变化立即转发给副本；换预设等非参数的变化稍后整体复制一次（也可以手动 syncClones）；
    - 不再需要时延迟几秒再销毁，避免编辑时反复创建；
    - 空闲时暂停运算：没有按住的音符、输出已静音 idleSeconds 秒的副本不再调用 processBlock，
      收到新的音符时立即恢复（类似 FL 的 Smart Disable）。主实例始终运行；导出（非实时）时不暂停。

    线程约定：
    - load / unload / restoreState / getOrCreateEditor 等：只在消息线程调用。
    - prepare / release：由宿主在 prepareToPlay / releaseResources 中调用。
    - process：音频线程。不会阻塞；正在切换插件时输出静音。

    状态变化（加载、卸载、出错）时会发送 change 消息。
*/
class HostedInstrument final : public juce::ChangeBroadcaster,
                               private juce::Timer
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

    /** 音源自己的界面，由 HostedInstrument 持有，调用方把它作为子组件嵌入即可。
        切换或卸载音源时会先销毁旧界面（随后发送 change 消息），调用方不要保存这个指针。
        没有加载音源时返回 nullptr。 */
    juce::AudioProcessorEditor* getOrCreateEditor();

    /** 销毁音源界面。嵌入它的窗口关闭前必须调用：插件的原生子窗口不能比父窗口活得更久。 */
    void releaseEditor();

    juce::AudioProcessorEditor* getEditor() const noexcept { return editor.get(); }

    /** 需要几个实例（包括主实例）。会在后台补齐；多余的延迟销毁。 */
    void setWantedInstances (int count);
    int getWantedInstances() const noexcept { return wantedInstances; }

    /** 最近一次创建副本失败的原因（成功后清空）。 */
    juce::String getCloneError() const { return cloneError; }

    /** 把主实例的完整状态复制给所有副本。 */
    void syncClones();

    static constexpr int maxInstances = 16;

    /** 自上次调用以来，用户是否改动过音源（调参数、换预设等）。任意线程可调用。
        刚加载或恢复后的短时间内，音源自己发出的变化通知会被忽略。 */
    bool consumeUserChange() noexcept { return userChanged.exchange (false); }

    /** 保存为 ValueTree（类型为 "HostedPlugin"）；没有音源时返回无效的 ValueTree。 */
    juce::ValueTree saveState() const;

    /** 恢复状态。在消息线程上会同步加载，否则投递到消息线程异步加载。 */
    void restoreState (const juce::ValueTree&);

    //==============================================================================
    // 任意线程

    int getLatencySamples() const noexcept     { return latencySamples.load(); }

    /** 已就绪、正在参与发声的实例数（没有加载音源时为 0）。 */
    int getNumInstances() const noexcept       { return numInstances.load(); }

    /** 其中正在运算（没有因空闲而暂停）的实例数。 */
    int getNumAwakeInstances() const noexcept  { return numAwake.load(); }

    static constexpr double idleSeconds = 3.0;
    static constexpr float silenceLevel = 1.0e-4f; ///< 约 -80 dB
    double getTailLengthSeconds() const noexcept { return tailSeconds.load(); }

    //==============================================================================
    // 宿主回调

    void prepare (double sampleRate, int maxBlockSize);
    void release();

    /** 音频线程：midi[i] 交给第 i 个实例，所有实例的输出相加后写入 output（已有内容会被覆盖）。
        numMidi 少于实例数时，其余实例收到空的 MIDI。处理后清空所有 midi。 */
    void process (juce::AudioBuffer<float>& output, juce::MidiBuffer* midi, int numMidi,
                  juce::AudioPlayHead* playHead, bool isNonRealtime) noexcept;

    /** 单实例的便捷写法。 */
    void process (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi,
                  juce::AudioPlayHead* playHead, bool isNonRealtime) noexcept
    {
        process (output, &midi, 1, playHead, isNonRealtime);
    }

    static constexpr auto stateType = "HostedPlugin";

private:
    void install (std::unique_ptr<juce::AudioPluginInstance>, const juce::MemoryBlock* stateToRestore);
    void restoreNow (const juce::ValueTree&);
    void setError (const juce::String&);
    static void configureBuses (juce::AudioPluginInstance&);

    void timerCallback() override;
    void createNextClone();
    void removeExtraClones();
    void forwardParameter (int index, float value) noexcept;
    void prepareInstance (juce::AudioPluginInstance&, double rate, int block);
    juce::AudioPluginInstance* getInstanceUnlocked (int index) const noexcept;
    static int scratchChannelsFor (const juce::AudioPluginInstance&);

    HostingContext& context;

    /** 只在交换指针时短暂持有；音频线程用 try-lock，不会被阻塞。 */
    juce::CriticalSection audioLock;
    std::unique_ptr<juce::AudioPluginInstance> instance;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> clones; ///< 修改时持有 audioLock
    std::atomic<int> numInstances { 0 };
    std::atomic<int> numAwake { 0 };

    /** 每个实例的活动状态（音频线程）。下标与实例下标相同；实例增删时重置。 */
    struct Activity
    {
        std::bitset<128> held;      ///< 按住的音符（所有 MIDI 通道合并计算）
        bool sustain = false;       ///< 延音踏板
        double silentSeconds = 0.0; ///< 没有按住的音符、输出静音的累计时长
        bool asleep = false;

        void reset() noexcept { held.reset(); sustain = false; silentSeconds = 0.0; asleep = false; }
    };

    std::array<Activity, maxInstances> activity;

    /** 根据这一块的 MIDI 更新按住的音符；返回这一块是否有新的音符开始。 */
    static bool trackMidi (Activity&, const juce::MidiBuffer&) noexcept;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer spareMidi;
    bool lastNonRealtime = false; // 仅音频线程访问

    double sampleRate = 44100.0;
    int blockSize = 512;
    bool prepared = false;

    std::atomic<int> latencySamples { 0 };
    std::atomic<double> tailSeconds { 0.0 };

    juce::ValueTree missingState;
    juce::String lastError;
    int loadGeneration = 0; ///< 让过时的异步加载结果作废

    // 副本（消息线程）
    int wantedInstances = 1;
    int cloneGeneration = 0;           ///< 换了音源后，旧的副本创建结果作废
    bool creatingClone = false;
    juce::String cloneError;
    juce::uint32 cloneRetryAt = 0;     ///< 创建失败后，到这个时间再重试
    juce::uint32 surplusSince = 0;     ///< 实例多于需要的起始时间（0 = 不多）
    std::atomic<bool> fullSyncPending { false };
    std::atomic<juce::uint32> lastStateChange { 0 };

    std::unique_ptr<juce::AudioProcessorEditor> editor;

    /** 监听被托管插件的参数和状态变化（可能在任意线程上回调）。 */
    struct ChangeWatcher final : public juce::AudioProcessorListener
    {
        explicit ChangeWatcher (HostedInstrument& o) : owner (o) {}
        void audioProcessorParameterChanged (juce::AudioProcessor*, int index, float value) override
        {
            owner.forwardParameter (index, value);
            owner.noteUserChange();
        }

        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
        {
            if (details.parameterInfoChanged || details.programChanged || details.nonParameterStateChanged)
            {
                // 换预设等：参数转发不一定覆盖得到，稍后把完整状态复制给副本
                owner.lastStateChange = juce::Time::getMillisecondCounter();
                owner.fullSyncPending = true;
                owner.noteUserChange();
            }
        }
        HostedInstrument& owner;
    };

    void noteUserChange() noexcept;

    ChangeWatcher changeWatcher { *this };
    std::atomic<bool> userChanged { false };
    std::atomic<juce::uint32> ignoreChangesUntil { 0 };

    JUCE_DECLARE_WEAK_REFERENCEABLE (HostedInstrument)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedInstrument)
};

} // namespace curvable
