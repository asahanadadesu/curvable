#include "plugin/PluginProcessor.h"
#include "plugin/PluginEditor.h"
#include "model/ProjectSerializer.h"

namespace curvable
{

namespace
{
    constexpr auto rootType = "CurvableState";
    constexpr int maxHostNotesPerBlock = 1024;

    /** 可撤销的音符编辑：只替换音符，保留播放模式等其他设置。 */
    class NotesEdit final : public juce::UndoableAction
    {
    public:
        NotesEdit (CurvableProcessor& p, std::vector<Note> beforeNotes, std::vector<Note> afterNotes, NoteId nextIdAfter)
            : processor (p), before (std::move (beforeNotes)), after (std::move (afterNotes)), nextId (nextIdAfter) {}

        bool perform() override { apply (after); return true; }
        bool undo() override    { apply (before); return true; }
        int getSizeInUnits() override { return (int) (before.size() + after.size()) + 1; }

    private:
        void apply (const std::vector<Note>& notes)
        {
            auto project = processor.getProject();
            project.notes = notes;
            project.nextId = std::max (project.nextId, nextId); // 撤销后新加的音符也不会和旧 id 冲突
            processor.setProject (std::move (project));
        }

        CurvableProcessor& processor;
        std::vector<Note> before, after;
        NoteId nextId;
    };
}

CurvableProcessor::CurvableProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    hostNotes.reserve (maxHostNotesPerBlock);
    hosted.addChangeListener (this);
    hosted.onUserParameterChange = [this] (int index, float value) { slots.hostedParameterTouched (index, value); };
    setProject (Project {}, false);

    // 定期回收音频线程已不再使用的旧 Schedule
    startTimerHz (4);
}

CurvableProcessor::~CurvableProcessor()
{
    stopTimer();
    hosted.onUserParameterChange = nullptr;
    hosted.removeChangeListener (this);
}

//==============================================================================
void CurvableProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    for (auto& m : voiceMidi)
        m.ensureSize (16 * 1024);
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

            if (transport.bpm > 0.0 && std::isfinite (transport.bpm))
                hostBpm = transport.bpm;
        }
    }

    // 宿主没报告速度时用它最近一次报告的（插件内播放也用这个速度）
    if (! (transport.bpm > 0.0 && std::isfinite (transport.bpm)))
        transport.bpm = hostBpm.load();

    hostPlaying = transport.isPlaying;

    // 2. 声部 → 实例的对应关系：新的实例准备好（或被销毁）后，先在旧的对应关系下停掉所有音符，再重新开始，
    //    避免 note on 和 note off 落在不同的实例上
    //    MPE 模式开关时同样处理
    const auto* schedule = scheduleExchange.acquire();
    const auto available = hosted.getNumInstances();
    const auto mpe = mpeMode.load();
    const auto routingChanged = available != routingCount || mpe != routedMpe;

    if (routingChanged)
    {
        sketchEvents.clear();
        engine.stopVoices (sketchEvents);
        routeEvents (sketchEvents);   // 按旧的对应关系

        routingCount = available;
        routedMpe = mpe;

        for (auto& level : voiceLevels)
            level.reset();

        sketchEvents.clear();
        engine.resyncVoices (sketchEvents);
        routeEvents (sketchEvents);   // 按新的对应关系
    }

    // 3. 宿主 MIDI：音符和弯音交给主实例（TRIGGER 模式下音符只作为触发，不发给音源）；
    //    其他消息（CC、延音踏板等）发给所有实例
    hostNotes.clear();

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn() || message.isNoteOff())
        {
            if (mode == PlaybackMode::trigger)
            {
                if (hostNotes.size() < hostNotes.capacity())
                    hostNotes.push_back ({ metadata.samplePosition, message.isNoteOn(), message.getNoteNumber() });
            }
            else
            {
                voiceMidi[0].addEvent (message, metadata.samplePosition);

                // 在 FL 里弹的音总要听得见（主实例上次的音符可能淡出到了静音）
                if (message.isNoteOn())
                    voiceLevels[0].add (metadata.samplePosition, 1.0f, 0.0f);
            }
            continue;
        }

        if (message.isPitchWheel())
        {
            voiceMidi[0].addEvent (message, metadata.samplePosition);
            continue;
        }

        for (int i = 0; i < juce::jmax (1, routingCount); ++i)
            voiceMidi[(std::size_t) i].addEvent (message, metadata.samplePosition);
    }

    midi.clear();

    // 4. 回放 Curvable 自己的音符（插件内播放的请求在这里交给引擎）
    sketchEvents.clear();

    switch (internalRequest.exchange (InternalRequest::none))
    {
        case InternalRequest::play: engine.startInternal (internalFrom.load(), internalLoopStart.load(), internalLoopEnd.load()); break;
        case InternalRequest::stop: engine.stopInternal (sketchEvents); break;
        case InternalRequest::none: break;
    }

    engine.process (mode, schedule, transport, numSamples,
                    hostNotes.data(), (int) hostNotes.size(), sketchEvents);
    internalPlaying = engine.isInternalActive();

    if (engine.isInternalActive())
        playheadBeat = engine.getInternalPosition();
    else if (mode == PlaybackMode::automatic && transport.isPlaying && transport.hasPosition)
        playheadBeat = transport.ppq;
    else if (mode == PlaybackMode::trigger && engine.isTriggerActive())
        playheadBeat = engine.getTriggerPosition();
    else
        playheadBeat = -1.0;

    // 试听音符（来自卷帘和琴键）交给主实例
    previewFifo.read (previewFifo.getNumReady()).forEach ([this] (int index)
    {
        const auto& e = previewEvents[(size_t) index];
        voiceMidi[0].addEvent (e.velocity > 0 ? juce::MidiMessage::noteOn (1, e.pitch, (juce::uint8) e.velocity)
                                              : juce::MidiMessage::noteOff (1, e.pitch), 0);

        if (e.velocity > 0)
            voiceLevels[0].add (0, 1.0f, 0.0f); // 试听的音总要听得见
    });

    routeEvents (sketchEvents);

    // 5. 宿主对参数槽的自动化 → 音源参数；然后交给音源发声（各实例的输出相加）
    slots.applyToHosted();
    hosted.setLevels (voiceLevels.data(), (int) voiceLevels.size());
    hosted.process (buffer, voiceMidi.data(), (int) voiceMidi.size(), getPlayHead(), isNonRealtime());

    for (auto& level : voiceLevels)
        level.finishBlock();
}

void CurvableProcessor::routeEvents (const MidiEventList& events) noexcept
{
    for (const auto& e : events.get())
    {
        // MPE：所有声部都在主实例上，各用一个通道（2..16）
        const bool hasOwnInstance = routedMpe || e.voice < routingCount;

        // 音量、声像作用于整个实例的输出：MPE 模式下（所有声部在一个实例里）、声部还没有自己的实例时不施加
        if (e.isLevel)
        {
            if (hasOwnInstance && ! routedMpe)
                voiceLevels[(std::size_t) e.voice].add (e.sampleOffset, e.gain, e.pan);
            continue;
        }

        // 声部的实例还没准备好：音符先交给主实例（暂时不弯），但弯音绝不能发过去，
        // 否则会把主实例上其他音符的弯音弄乱
        if (e.isPitchWheel && ! hasOwnInstance)
            continue;

        auto& target = voiceMidi[(std::size_t) (routedMpe || ! hasOwnInstance ? 0 : e.voice)];
        const auto channel = routedMpe ? 2 + e.voice % mpeChannels : 1;

        if (e.isPitchWheel)
            target.addEvent (juce::MidiMessage::pitchWheel (channel, e.wheelValue), e.sampleOffset);
        else if (e.noteOn)
            target.addEvent (juce::MidiMessage::noteOn (channel, e.pitch, (juce::uint8) e.velocity), e.sampleOffset);
        else
            target.addEvent (juce::MidiMessage::noteOff (channel, e.pitch), e.sampleOffset);
    }
}

void CurvableProcessor::setMpeMode (bool shouldBeOn)
{
    if (mpeMode.exchange (shouldBeOn) != shouldBeOn)
    {
        hosted.setWantedInstances (shouldBeOn ? 1 : wantedVoices.load());
        markStateChanged();
        projectChanged.sendChangeMessage(); // 界面刷新
    }
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
        auto schedule = std::make_unique<Schedule> (Schedule::compile (project));
        wantedVoices = schedule->voiceCount;
        scheduleExchange.publish (std::move (schedule));
    }

    projectChanged.sendChangeMessage();
}

void CurvableProcessor::commitEdit (const Project& before, const juce::String& name)
{
    const auto after = getProject();

    if (after.notes == before.notes)
        return;

    undoManager.beginNewTransaction (name);
    undoManager.perform (new NotesEdit (*this, before.notes, after.notes, after.nextId));
}

void CurvableProcessor::playInternally (double fromBeat, double loopStart, double loopEnd) noexcept
{
    internalFrom = fromBeat;
    internalLoopStart = loopStart;
    internalLoopEnd = loopEnd;
    internalRequest = InternalRequest::play;
    internalPlaying = true; // 界面马上显示为正在播放（音频线程下一块确认）
}

void CurvableProcessor::stopInternally() noexcept
{
    internalRequest = InternalRequest::stop;
    internalPlaying = false;
}

void CurvableProcessor::previewNote (int pitch, int velocity)
{
    if (pitch < 0 || pitch > 127)
        return;

    previewFifo.write (1).forEach ([&] (int index)
    {
        previewEvents[(size_t) index] = { pitch, juce::jlimit (0, 127, velocity) };
    });
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

    if (mpeMode.load())
        state.setProperty ("mpe", true, nullptr);
    state.appendChild (ProjectSerializer::toValueTree (getProject()), nullptr);

    if (auto hostedState = hosted.saveState(); hostedState.isValid())
        state.appendChild (hostedState, nullptr);

    state.appendChild (slots.save(), nullptr);

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

    undoManager.clearUndoHistory();
    mpeMode = (bool) state.getProperty ("mpe", false);

    // 早期版本的状态没有 Project 节点：保持空工程
    if (auto restored = ProjectSerializer::fromValueTree (state.getChildWithName (ProjectSerializer::type)))
        setProject (std::move (*restored), false);
    else
        setProject (Project {}, false);

    slots.restore (state.getChildWithName (AutomationSlots::stateType));
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
    if (slots.update())
        stateChanged = true;

    // 按工程需要的声部数准备音源实例（创建在后台进行）
    hosted.setWantedInstances (mpeMode.load() ? 1 : wantedVoices.load());

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
