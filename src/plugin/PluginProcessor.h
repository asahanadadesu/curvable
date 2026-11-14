#pragma once

#include "engine/PlaybackEngine.h"
#include "engine/ScheduleExchange.h"
#include "hosting/HostedInstrument.h"
#include "hosting/HostingContext.h"
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

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    juce::SharedResourcePointer<HostingContext> hostingContext;
    HostedInstrument hosted { *hostingContext };

    mutable juce::CriticalSection projectLock; ///< 宿主可能在其他线程保存/恢复状态
    Project project;

    ScheduleExchange scheduleExchange;
    std::atomic<PlaybackMode> playbackMode { PlaybackMode::automatic };
    std::atomic<bool> stateChanged { false };

    // 以下只在音频线程使用（prepare / release 除外）
    PlaybackEngine engine;
    MidiEventList sketchEvents;
    std::vector<HostNote> hostNotes;
    juce::MidiBuffer scratchMidi;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableProcessor)
};

} // namespace curvable
