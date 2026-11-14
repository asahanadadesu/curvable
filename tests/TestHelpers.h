#pragma once

#include "engine/PlaybackEngine.h"

#include <cmath>
#include <map>
#include <vector>

namespace curvable::test
{

/** 带绝对采样位置的事件，便于跨音频块比较。 */
struct AbsEvent
{
    long long sample;
    bool noteOn;
    int pitch;

    bool operator== (const AbsEvent&) const = default;
};

/** 按块驱动 PlaybackEngine，并把输出换算成绝对采样位置。 */
struct EngineRunner
{
    static constexpr double sampleRate = 48000.0;
    static constexpr double bpm = 120.0;
    static constexpr double samplesPerBeat = sampleRate * 60.0 / bpm; // 24000

    explicit EngineRunner (Schedule s) : schedule (std::move (s)) { engine.prepare (sampleRate); }

    PlaybackEngine engine;
    Schedule schedule;
    std::vector<AbsEvent> events;
    long long position = 0; ///< 已处理的采样数（绝对）

    void block (int numSamples, const HostTransport& transport, PlaybackMode mode = PlaybackMode::automatic,
                std::vector<HostNote> hostNotes = {})
    {
        MidiEventList out;
        engine.process (mode, &schedule, transport, numSamples, hostNotes.data(), (int) hostNotes.size(), out);

        for (const auto& e : out.get())
            events.push_back ({ position + e.sampleOffset, e.noteOn, e.pitch });

        position += numSamples;
    }

    /** 宿主从 startPpq 开始连续播放 totalSamples 个采样，块大小依次取自 blockSizes（循环使用）。 */
    void playContinuously (double startPpq, long long totalSamples, const std::vector<int>& blockSizes)
    {
        double ppq = startPpq;
        long long done = 0;
        std::size_t i = 0;

        while (done < totalSamples)
        {
            const int n = (int) std::min<long long> (blockSizes[i++ % blockSizes.size()], totalSamples - done);
            block (n, { true, true, ppq, bpm });
            ppq += n / samplesPerBeat;
            done += n;
        }
    }

    std::vector<AbsEvent> onsFor (int pitch) const
    {
        std::vector<AbsEvent> result;
        for (const auto& e : events)
            if (e.noteOn && e.pitch == pitch)
                result.push_back (e);
        return result;
    }

    /** 事件序列结束时仍在发声的音高（应为空，否则说明有"挂音"）。 */
    std::vector<int> soundingPitches() const
    {
        std::map<int, bool> sounding;
        for (const auto& e : events)
            sounding[e.pitch] = e.noteOn;

        std::vector<int> result;
        for (const auto& [pitch, on] : sounding)
            if (on)
                result.push_back (pitch);
        return result;
    }

    /** 事件的绝对位置是否单调不减。 */
    bool isChronological() const
    {
        for (std::size_t i = 1; i < events.size(); ++i)
            if (events[i].sample < events[i - 1].sample)
                return false;
        return true;
    }
};

inline long long beatToSample (double beat)
{
    return (long long) std::ceil (beat * EngineRunner::samplesPerBeat - 1.0e-6);
}

} // namespace curvable::test
