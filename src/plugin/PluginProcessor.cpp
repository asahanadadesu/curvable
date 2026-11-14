#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"
#include "model/ProjectSerializer.h"

namespace curvable
{

namespace
{
    constexpr auto rootType = "CurvableState";
    constexpr int maxHostNotesPerBlock = 1024;
}

CurvableProcessor::CurvableProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    hostNotes.reserve (maxHostNotesPerBlock);
    hosted.addChangeListener (this);
    setProject (Project {}, false);

    // 定期回收音频线程已不再使用的旧 Schedule
    startTimerHz (4);
}

CurvableProcessor::~CurvableProcessor()
{
    stopTimer();
    hosted.removeChangeListener (this);
}

//==============================================================================
void CurvableProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    scratchMidi.ensureSize (64 * 1024);
    hosted.prepare (sampleRate, samplesPerBlock);
    setLatencySamples (hosted.getLatencySamples());
}

void CurvableProcessor::releaseResources()
{
    sketchEvents.clear();
    engine.reset (sketchEvents); // 音频已停止，产生的 note off 不再需要发送
    sketchEvents.clear();
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

    const auto mode = playbackMode.load();
    const auto numSamples = buffer.getNumSamples();

    // 1. 宿主走带
    HostTransport transport;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            transport.isPlaying = position->getIsPlaying();

            if (const auto ppq = position->getPpqPosition())
            {
                transport.hasPosition = true;
                transport.ppq = *ppq;
            }

            if (const auto bpm = position->getBpm())
                transport.bpm = *bpm;
        }
    }

    // 2. 宿主 MIDI：TRIGGER 模式下音符只作为触发，不发给音源；其他消息（CC 等）照常通过
    hostNotes.clear();
    scratchMidi.clear();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (mode == PlaybackMode::trigger && (message.isNoteOn() || message.isNoteOff()))
        {
            if (hostNotes.size() < hostNotes.capacity())
                hostNotes.push_back ({ metadata.samplePosition, message.isNoteOn(), message.getNoteNumber() });
            continue;
        }

        scratchMidi.addEvent (message, metadata.samplePosition);
    }

    // 3. 回放 Curvable 自己的音符
    sketchEvents.clear();
    engine.process (mode, scheduleExchange.acquire(), transport, numSamples,
                    hostNotes.data(), (int) hostNotes.size(), sketchEvents);

    for (const auto& e : sketchEvents.get())
    {
        scratchMidi.addEvent (e.noteOn ? juce::MidiMessage::noteOn (1, e.pitch, (juce::uint8) e.velocity)
                                       : juce::MidiMessage::noteOff (1, e.pitch),
                              e.sampleOffset);
    }

    midi.swapWith (scratchMidi);

    // 4. 交给音源发声
    hosted.process (buffer, midi, getPlayHead(), isNonRealtime());
}

//==============================================================================
Project CurvableProcessor::getProject() const
{
    const juce::ScopedLock sl (projectLock);
    return project;
}

void CurvableProcessor::setProject (Project newProject, bool isUserEdit)
{
    if (isUserEdit)
        markStateChanged();

    {
        const juce::ScopedLock sl (projectLock);
        project = std::move (newProject);
        playbackMode = project.mode;
        scheduleExchange.publish (std::make_unique<Schedule> (Schedule::compile (project)));
    }

    projectChanged.sendChangeMessage();
}

void CurvableProcessor::setPlaybackMode (PlaybackMode mode)
{
    auto p = getProject();
    p.mode = mode;
    setProject (std::move (p));
}

//==============================================================================
juce::AudioProcessorEditor* CurvableProcessor::createEditor()
{
    return new CurvableEditor (*this);
}

void CurvableProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state (rootType);
    state.setProperty ("version", CURVABLE_VERSION_STRING, nullptr);
    state.appendChild (ProjectSerializer::toValueTree (getProject()), nullptr);

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

    // 早期版本的状态没有 Project 节点：保持空工程
    if (auto restored = ProjectSerializer::fromValueTree (state.getChildWithName (ProjectSerializer::type)))
        setProject (std::move (*restored), false);
    else
        setProject (Project {}, false);

    hosted.restoreState (state.getChildWithName (HostedInstrument::stateType));
}

//==============================================================================
void CurvableProcessor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // 换了音源后，延迟可能变化，告知宿主以便做延迟补偿
    if (getLatencySamples() != hosted.getLatencySamples())
        setLatencySamples (hosted.getLatencySamples());
}

void CurvableProcessor::timerCallback()
{
    scheduleExchange.collectGarbage();

    // 用户在 Curvable 或被托管的音源里改了东西：让宿主知道工程需要保存
    // （VST3 中通过 IComponentHandler2::setDirty 通知）
    if (hosted.consumeUserChange())
        stateChanged = true;

    if (stateChanged.exchange (false))
        updateHostDisplay (ChangeDetails().withNonParameterStateChanged (true));
}

} // namespace curvable

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new curvable::CurvableProcessor();
}
