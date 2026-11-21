#include "model/Project.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace envelope
{
    float gainFromDecibels (double decibels) noexcept
    {
        if (! (decibels > volumeMin))
            return 0.0f;

        return (float) std::pow (10.0, std::min (decibels, volumeMax) / 20.0);
    }

    double faderFromDecibels (double decibels) noexcept
    {
        if (! (decibels > volumeMin))
            return 0.0;

        return std::sqrt (std::pow (10.0, std::min (decibels, volumeMax) / 20.0) / 2.0);
    }

    double decibelsFromFader (double fader) noexcept
    {
        const auto gain = 2.0 * fader * fader;
        return gain > 0.0 ? std::clamp (20.0 * std::log10 (gain), volumeMin, volumeMax) : volumeMin;
    }

    double volumeAt (const Envelope& e, double time) noexcept
    {
        const auto& pts = e.points;

        if (pts.empty())
            return 0.0;

        if (time <= pts.front().time)
            return pts.front().semitones;

        for (std::size_t i = 1; i < pts.size(); ++i)
        {
            const auto& a = pts[i - 1];
            const auto& b = pts[i];

            if (time < b.time)
            {
                const auto span = b.time - a.time;

                if (span <= 0.0)
                    return b.semitones;

                if (time <= a.time)
                    return a.semitones; // 正好在节点上：原样返回，不经过换算

                const auto from = faderFromDecibels (a.semitones), to = faderFromDecibels (b.semitones);
                return decibelsFromFader (from + (to - from) * PitchCurve::shape ((time - a.time) / span, a.tension));
            }
        }

        return pts.back().semitones;
    }

    Envelope toFader (Envelope e)
    {
        for (auto& p : e.points)
            p.semitones = faderFromDecibels (p.semitones);

        return e;
    }

    Envelope fromFader (Envelope e)
    {
        for (auto& p : e.points)
            p.semitones = decibelsFromFader (p.semitones);

        return e;
    }

    namespace
    {
        void normaliseValues (Envelope& e, double low, double high)
        {
            e.normalise();
            e.vibrato = {};

            for (auto& p : e.points)
            {
                p.semitones = std::clamp (p.semitones, low, high);
                p.vibrato.reset();
            }
        }
    }

    void normaliseVolume (Envelope& e) { normaliseValues (e, volumeMin, volumeMax); }
    void normalisePan (Envelope& e)    { normaliseValues (e, -1.0, 1.0); }
}

NoteId Project::addNote (double start, double length, int pitch, float velocity)
{
    const auto id = nextId++;
    notes.push_back ({ id, start, length, pitch, velocity });
    return id;
}

double Project::getContentEnd() const noexcept
{
    double end = 0.0;

    for (const auto& n : notes)
        end = std::max (end, n.getEnd());

    return end;
}

Project Project::createTestPhrase()
{
    Project p;

    // 和弦：第 1 小节 C 大三和弦，第 2 小节 G 大三和弦，各持续一整小节
    for (int pitch : { 48, 52, 55 })
        p.addNote (0.0, 4.0, pitch, 0.6f);

    for (int pitch : { 43, 47, 50 })
        p.addNote (4.0, 4.0, pitch, 0.6f);

    // 旋律：八分音符，便于听出节拍是否对齐
    const int melody[] = { 72, 74, 76, 79, 76, 74, 72, 67,
                           71, 74, 79, 83, 79, 74, 71, 67 };

    for (int i = 0; i < 16; ++i)
        p.addNote (i * 0.5, 0.45, melody[i], i % 2 == 0 ? 0.9f : 0.7f);

    return p;
}

double roundUpToBar (double beats, double beatsPerBar) noexcept
{
    if (beats <= 0.0 || beatsPerBar <= 0.0)
        return 0.0;

    // 容差：避免 8.0000000001 被取整到 12
    return std::ceil (beats / beatsPerBar - 1.0e-9) * beatsPerBar;
}

} // namespace curvable
