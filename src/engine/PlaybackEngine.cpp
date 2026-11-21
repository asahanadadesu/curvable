#include "engine/PlaybackEngine.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace
{
    constexpr double fallbackBpm = 120.0;
    constexpr double minCueLength = 1.0e-3;
}

void PlaybackEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    player.setSampleRate (sampleRate);
}

void PlaybackEngine::reset (MidiEventList& out)
{
    stopEverything (0, out);
}

void PlaybackEngine::stopVoices (MidiEventList& out)
{
    player.stopAll (0, out);
}

void PlaybackEngine::resyncVoices (MidiEventList& out)
{
    player.resetWheels (0, out);

    // AUTO：下一块会按"跳转"处理，从宿主位置补发；TRIGGER、插件内播放：立即从当前位置补发
    autoRunning = false;

    if (internal.active)
        player.start (internal.position, 0, out);
    else if (currentMode == PlaybackMode::trigger && trigger.active)
        player.start (trigger.position, 0, out);
}

void PlaybackEngine::stopEverything (int sampleOffset, MidiEventList& out)
{
    player.stopAll (sampleOffset, out);
    autoRunning = false;
    trigger.active = false;
    trigger.pitch = -1;
    internal.active = false;
}

void PlaybackEngine::startInternal (double fromBeat, double loopStart, double loopEnd) noexcept
{
    internalRequest = { true, fromBeat, loopStart, loopEnd };
}

void PlaybackEngine::beginInternal (MidiEventList& out)
{
    const auto request = internalRequest;
    internalRequest.pending = false;
    auto fromBeat = request.from;
    const auto loopStart = request.loopStart, loopEnd = request.loopEnd;
    stopEverything (0, out);

    internal.loop = loopEnd - loopStart >= minCueLength;

    if (internal.loop)
    {
        internal.start = std::max (0.0, loopStart);
        internal.end = loopEnd;

        if (fromBeat < internal.start || fromBeat >= internal.end)
            fromBeat = internal.start;
    }
    else
    {
        internal.start = 0.0;
        internal.end = schedule != nullptr ? schedule->length : 0.0;

        if (fromBeat >= internal.end - minCueLength)
            return; // 后面没有内容了
    }

    internal.active = true;
    internal.position = std::max (0.0, fromBeat);
    player.start (internal.position, 0, out);
}

void PlaybackEngine::stopInternal (MidiEventList& out)
{
    internalRequest.pending = false;

    if (internal.active)
    {
        player.stopAll (0, out);
        internal.active = false;
    }
}

//==============================================================================
void PlaybackEngine::process (PlaybackMode mode, const Schedule* newSchedule, const HostTransport& transport,
                              int numSamples, const HostNote* hostNotes, int numHostNotes, MidiEventList& out)
{
    if (numSamples <= 0)
        return;

    if (mode != currentMode)
    {
        stopEverything (0, out);
        currentMode = mode;
    }

    if (newSchedule != schedule)
    {
        schedule = newSchedule;
        const auto beat = internal.active ? internal.position
                        : currentMode == PlaybackMode::trigger ? trigger.position : expectedPpq;
        player.setSchedule (schedule, beat, 0, out);

        // 内容变了，触发区间（例如"到内容结尾"）可能随之变化
        if (trigger.active && schedule != nullptr)
            for (const auto& cue : schedule->cues)
                if (cue.key == trigger.pitch || cue.key == TriggerCue::anyKey)
                {
                    schedule->resolveCue (cue, trigger.cueStart, trigger.cueEnd);
                    break;
                }
    }

    const auto bpm = transport.bpm > 0.0 && std::isfinite (transport.bpm) ? transport.bpm : fallbackBpm;
    const auto samplesPerBeat = sampleRate * 60.0 / bpm;

    if (internalRequest.pending)
        beginInternal (out);

    if (internal.active)
    {
        // 宿主接手：AUTO 下宿主开始播放，TRIGGER 下来了触发音符
        const auto hostStarted = currentMode == PlaybackMode::automatic && transport.isPlaying && transport.hasPosition;
        const auto triggered = currentMode == PlaybackMode::trigger
                               && std::any_of (hostNotes, hostNotes + numHostNotes, [] (const HostNote& n) { return n.isOn; });

        if (! hostStarted && ! triggered)
        {
            // 不循环时内容可能变短了
            if (! internal.loop && schedule != nullptr)
                internal.end = schedule->length;

            internal.active = advanceRegion (internal.position, internal.start, internal.end, internal.loop,
                                             0, numSamples, samplesPerBeat, out);
            return;
        }

        stopInternal (out);
    }

    switch (currentMode)
    {
        case PlaybackMode::automatic: processAuto (transport, numSamples, samplesPerBeat, out); break;
        case PlaybackMode::trigger:   processTrigger (numSamples, samplesPerBeat, hostNotes, numHostNotes, out); break;
        case PlaybackMode::off:       break;
    }
}

//==============================================================================
void PlaybackEngine::processAuto (const HostTransport& transport, int numSamples, double samplesPerBeat, MidiEventList& out)
{
    if (! transport.isPlaying || ! transport.hasPosition)
    {
        if (autoRunning || player.hasActiveNotes())
            stopEverything (0, out);
        return;
    }

    const auto blockBeats = numSamples / samplesPerBeat;
    const auto from = transport.ppq;

    // 允许约两个采样的误差；超过即视为跳转（拖动播放头、宿主循环回起点、刚开始播放）
    const auto tolerance = std::max (1.0e-6, 2.0 / samplesPerBeat);

    if (! autoRunning || std::abs (from - expectedPpq) > tolerance)
    {
        player.stopAll (0, out);
        player.start (from, 0, out);
        autoRunning = true;
    }

    player.render (from, from + blockBeats, 0.0, 0, numSamples, samplesPerBeat, out);
    expectedPpq = from + blockBeats;
}

//==============================================================================
void PlaybackEngine::processTrigger (int numSamples, double samplesPerBeat, const HostNote* hostNotes, int numHostNotes,
                                     MidiEventList& out)
{
    int position = 0;

    // 宿主事件按时间顺序到来；把块按事件切开，逐段推进
    for (int i = 0; i < numHostNotes; ++i)
    {
        const auto& note = hostNotes[i];
        const auto offset = std::clamp (note.sampleOffset, position, numSamples - 1);

        advanceTrigger (position, offset, samplesPerBeat, out);
        position = offset;

        if (note.isOn)
        {
            startTrigger (note.pitch, offset, out);
        }
        else if (trigger.active && note.pitch == trigger.pitch)
        {
            player.stopAll (offset, out);
            trigger.active = false;
            trigger.pitch = -1;
        }
    }

    advanceTrigger (position, numSamples, samplesPerBeat, out);
}

void PlaybackEngine::startTrigger (int pitch, int sampleOffset, MidiEventList& out)
{
    if (schedule == nullptr)
        return;

    const TriggerCue* match = nullptr;

    // 精确匹配的触发键优先，其次是"任意键"
    for (const auto& cue : schedule->cues)
        if (cue.key == pitch)
            match = &cue;

    if (match == nullptr)
        for (const auto& cue : schedule->cues)
            if (cue.key == TriggerCue::anyKey)
            {
                match = &cue;
                break;
            }

    if (match == nullptr)
        return; // 这个键没有对应的段落，忽略

    player.stopAll (sampleOffset, out);

    trigger.active = true;
    trigger.pitch = pitch;
    schedule->resolveCue (*match, trigger.cueStart, trigger.cueEnd);
    trigger.position = trigger.cueStart;

    player.start (trigger.position, sampleOffset, out);
}

void PlaybackEngine::advanceTrigger (int fromSample, int toSample, double samplesPerBeat, MidiEventList& out)
{
    if (trigger.active)
        advanceRegion (trigger.position, trigger.cueStart, trigger.cueEnd, true, fromSample, toSample, samplesPerBeat, out);
}

bool PlaybackEngine::advanceRegion (double& position, double start, double end, bool loop,
                                    int fromSample, int toSample, double samplesPerBeat, MidiEventList& out)
{
    if (toSample <= fromSample)
        return true;

    // 区间太短：循环时什么也不做，不循环时直接结束
    if (end - start < minCueLength)
    {
        if (loop)
            return true;

        player.stopAll (fromSample, out);
        return false;
    }

    double sample = fromSample;

    while (sample < toSample - 1.0e-9)
    {
        const auto beatsLeftInBlock = (toSample - sample) / samplesPerBeat;
        const auto segmentEnd = std::min (position + beatsLeftInBlock, end);
        const auto segmentBeats = segmentEnd - position;

        player.render (position, segmentEnd, sample, fromSample, toSample, samplesPerBeat, out);

        sample += segmentBeats * samplesPerBeat;
        position = segmentEnd;

        // 到达终点：停止仍在响的音符；循环时回到起点继续
        if (position >= end - 1.0e-9)
        {
            const auto atSample = std::clamp (SketchPlayer::toSampleIndex (sample), fromSample, toSample - 1);
            player.stopAll (atSample, out);

            if (! loop)
                return false;

            position = start;
            player.start (position, atSample, out);
        }

        if (segmentBeats <= 0.0)
            break; // 防御：不应发生
    }

    return true;
}

} // namespace curvable
