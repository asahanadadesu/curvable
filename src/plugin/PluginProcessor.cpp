#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"

namespace curvable
{

CurvableProcessor::CurvableProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void CurvableProcessor::prepareToPlay (double, int) {}

void CurvableProcessor::releaseResources() {}

bool CurvableProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CurvableProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();
}

juce::AudioProcessorEditor* CurvableProcessor::createEditor()
{
    return new CurvableEditor (*this);
}

void CurvableProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("CurvableState");
    state.setProperty ("version", CURVABLE_VERSION_STRING, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void CurvableProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // M0：暂无可恢复的状态，后续里程碑在此读取项目数据
    juce::ignoreUnused (getXmlFromBinary (data, sizeInBytes));
}

} // namespace curvable

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new curvable::CurvableProcessor();
}
