#pragma once

#include "hosting/HostedInstrument.h"
#include "hosting/HostingContext.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace curvable
{

class CurvableProcessor final : public juce::AudioProcessor,
                                private juce::ChangeListener
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

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::SharedResourcePointer<HostingContext> hostingContext;
    HostedInstrument hosted { *hostingContext };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableProcessor)
};

} // namespace curvable
