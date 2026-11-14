#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

namespace curvable
{

namespace
{
    constexpr auto rootType = "CurvableState";
}

CurvableProcessor::CurvableProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    hosted.addChangeListener (this);
}

CurvableProcessor::~CurvableProcessor()
{
    hosted.removeChangeListener (this);
}

void CurvableProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    hosted.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (hosted.getLatencySamples());
}

void CurvableProcessor::releaseResources()
{
    hosted.release();
}

bool CurvableProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CurvableProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // M1：宿主发来的 MIDI 直接交给音源（直通模式，用于调试）
    hosted.process (buffer, midi, getPlayHead(), isNonRealtime());
}

juce::AudioProcessorEditor* CurvableProcessor::createEditor()
{
    return new CurvableEditor (*this);
}

void CurvableProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state (rootType);
    state.setProperty ("version", CURVABLE_VERSION_STRING, nullptr);

    if (auto hostedState = hosted.saveState(); hostedState.isValid())
        state.appendChild (hostedState, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CurvableProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    const auto state = juce::ValueTree::fromXml (*xml);

    if (! state.hasType (rootType))
        return;

    hosted.restoreState (state.getChildWithName (HostedInstrument::stateType));
}

void CurvableProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // 换了音源后，延迟可能变化，告知宿主以便做延迟补偿
    if (getLatencySamples() != hosted.getLatencySamples())
        setLatencySamples (hosted.getLatencySamples());
}

} // namespace curvable

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new curvable::CurvableProcessor();
}
