#pragma once

#include "engine/PlaybackEngine.h"
#include "engine/ScheduleExchange.h"
#include "hosting/HostedInstrument.h"
#include "hosting/HostingContext.h"
#include "plugin/AutomationSlots.h"
#include "model/Project.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace curvable
{

class CurvableProcessor final : public juce::AudioProcessor,
                                private juce::ChangeListener,
                                private juce::Timer
{
public:
    CurvableProcessor();
    ~CurvableProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return hosted.getTailLengthSeconds(); }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    HostingContext& getHostingContext() noexcept { return *hostingContext; }
    HostedInstrument& getHostedInstrument() noexcept { return hosted; }
    AutomationSlots& getAutomationSlots() noexcept { return slots; }

    //==============================================================================
    // 编辑内容（消息线程）

    Project getProject() const;

    /** @param isUserEdit  用户的编辑（会通知宿主"工程有未保存的改动"）；从宿主恢复状态时为 false */
    void setProject (Project, bool isUserEdit = true);
    void setPlaybackMode (PlaybackMode);

    /** 通知宿主：工程有未保存的改动（例如换了音源）。任意线程可调用，实际通知在消息线程上发出。 */
    void markStateChanged() noexcept { stateChanged = true; }

    /** 编辑内容变化时（包括宿主恢复工程）发送 change 消息。 */
    juce::ChangeBroadcaster projectChanged;

    /** 记录一次可撤销的音符编辑：before 是编辑前的工程，编辑后的内容取当前工程。
        音符没有变化时不记录。撤销 / 重做只影响音符，不影响播放模式等设置。 */
    void commitEdit (const Project& before, const juce::String& name);

    juce::UndoManager& getUndoManager() noexcept { return undoManager; }

    /** 试听：立即把音符发给音源（velocity 为 0 表示松开）。消息线程调用。 */
    void previewNote (int pitch, int velocity);

    /** 界面用：当前播放位置（拍）；没有在播放时返回负数。 */
    double getPlayheadBeat() const noexcept { return playheadBeat.load(); }

    /** 宿主最近一次报告的速度（BPM）；界面用来显示以 Hz 计的颤音。 */
    double getHostBpm() const noexcept { return hostBpm.load(); }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    juce::SharedResourcePointer<HostingContext> hostingContext;
    HostedInstrument hosted { *hostingContext };
    AutomationSlots slots { *this, hosted };

    mutable juce::CriticalSection projectLock; ///< 宿主可能在其他线程保存/恢复状态
    Project project;

    ScheduleExchange scheduleExchange;
    std::atomic<PlaybackMode> playbackMode { PlaybackMode::automatic };
    std::atomic<bool> stateChanged { false };
    std::atomic<double> playheadBeat { -1.0 };
    std::atomic<double> hostBpm { 120.0 };

    juce::UndoManager undoManager { 0, 200 };

    // 试听音符：消息线程写入，音频线程读出
    struct PreviewEvent { int pitch; int velocity; };
    juce::AbstractFifo previewFifo { 256 };
    std::array<PreviewEvent, 256> previewEvents {};

    // 以下只在音频线程使用（prepare / release 除外）
    PlaybackEngine engine;
    MidiEventList sketchEvents;
    std::vector<HostNote> hostNotes;
    std::array<juce::MidiBuffer, Schedule::maxVoices> voiceMidi; ///< 第 i 个音源实例收到的 MIDI
    int routingCount = 0; ///< 当前按几个实例分配声部（变化时重新开始发声）


    void routeEvents (const MidiEventList&) noexcept;

    std::atomic<int> wantedVoices { 1 }; ///< 当前工程需要的声部数（消息线程据此调整实例数）

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableProcessor)
};

} // namespace curvable
