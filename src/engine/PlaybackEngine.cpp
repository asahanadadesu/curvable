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
}

void PlaybackEngine::reset (MidiEventList& out)
{
    stopEverything (0, out);
}

void PlaybackEngine::stopEverything (int sampleOffset, MidiEventList& out)
{
    player.stopAll (sampleOffset, out);
    autoRunning = false;
    trigger.active = false;
    trigger.pitch = -1;
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
        const auto beat = currentMode == PlaybackMode::trigger ? trigger.position : expectedPpq;
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
    if (! trigger.active || toSample <= fromSample)
        return;

    if (trigger.cueEnd - trigger.cueStart < minCueLength)
        return;

    double sample = fromSample;

    while (sample < toSample - 1.0e-9)
    {
        const auto beatsLeftInBlock = (toSample - sample) / samplesPerBeat;
        const auto segmentEnd = std::min (trigger.position + beatsLeftInBlock, trigger.cueEnd);
        const auto segmentBeats = segmentEnd - trigger.position;

        player.render (trigger.position, segmentEnd, sample, fromSample, toSample, samplesPerBeat, out);

        sample += segmentBeats * samplesPerBeat;
        trigger.position = segmentEnd;

        // 到达段落终点：停止仍在响的音符，回到起点继续
        if (trigger.position >= trigger.cueEnd - 1.0e-9)
        {
            const auto loopSample = std::clamp (SketchPlayer::toSampleIndex (sample), fromSample, toSample - 1);
            player.stopAll (loopSample, out);
            trigger.position = trigger.cueStart;
            player.start (trigger.position, loopSample, out);
        }

        if (segmentBeats <= 0.0)
            break; // 防御：不应发生
    }
}

} // namespace curvable
