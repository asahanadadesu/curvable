#include "engine/SketchPlayer.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace
{
    constexpr std::size_t maxActiveNotes = 512;
    constexpr std::size_t maxActions = 4096;

    // 宿主的 PPQ 是逐块累加出来的，带有浮点误差。区间边界留出这点余量，
    // 保证正好落在块边界上的音符总是归到同一个块里（不会被漏掉，也不会提前一个采样）。
    constexpr double beatEpsilon = 1.0e-9;
}

SketchPlayer::SketchPlayer()
{
    active.reserve (maxActiveNotes);
    actions.reserve (maxActions);
}

//==============================================================================
void SketchPlayer::activate (const ScheduledNote& note, int sampleOffset, MidiEventList& out)
{
    if (active.size() >= maxActiveNotes)
        return;

    // 同音高已在发声：重新触发
    if (pitchCount[note.pitch] > 0)
        out.add ({ sampleOffset, false, note.pitch, 0 });

    out.add ({ sampleOffset, true, note.pitch, note.velocity });
    ++pitchCount[note.pitch];
    active.push_back ({ note.id, note.pitch, note.end });
}

void SketchPlayer::deactivate (std::size_t index, int sampleOffset, MidiEventList& out)
{
    const auto pitch = active[index].pitch;

    if (--pitchCount[pitch] <= 0)
    {
        pitchCount[pitch] = 0;
        out.add ({ sampleOffset, false, pitch, 0 });
    }

    active[index] = active.back();
    active.pop_back();
}

//==============================================================================
void SketchPlayer::setSchedule (const Schedule* newSchedule, double currentBeat, int sampleOffset, MidiEventList& out)
{
    schedule = newSchedule;

    for (std::size_t i = active.size(); i-- > 0;)
    {
        const auto* note = schedule != nullptr ? schedule->find (active[i].id) : nullptr;

        if (note == nullptr || note->pitch != active[i].pitch || note->end <= currentBeat)
            deactivate (i, sampleOffset, out);
        else
            active[i].end = note->end;
    }
}

int SketchPlayer::toSampleIndex (double exactSample) noexcept
{
    return static_cast<int> (std::ceil (exactSample - 1.0e-6));
}

void SketchPlayer::render (double from, double to, double sampleAtFrom, int blockStart, int blockEnd,
                           double samplesPerBeat, MidiEventList& out)
{
    if (to <= from || blockEnd <= blockStart)
        return;

    actions.clear();

    // 1. 正在发声、在本区间内（或之前，例如结束时间被改短）结束的音符
    for (const auto& a : active)
        if (a.end < to - beatEpsilon)
            actions.push_back ({ std::max (a.end, from), false, nullptr, a.id, a.pitch });

    // 2. 在本区间内开始的音符（以及其中在本区间内就结束的）
    if (schedule != nullptr)
    {
        const auto& notes = schedule->notes;
        auto it = std::lower_bound (notes.begin(), notes.end(), from - beatEpsilon,
                                    [] (const ScheduledNote& n, double beat) { return n.start < beat; });

        for (; it != notes.end() && it->start < to - beatEpsilon && actions.size() + 2 <= actions.capacity(); ++it)
        {
            actions.push_back ({ it->start, true, &*it, it->id, it->pitch });

            if (it->end < to - beatEpsilon)
                actions.push_back ({ it->end, false, nullptr, it->id, it->pitch });
        }
    }

    // 同一时刻：先结束、后开始（同音高的连续音符不会被切断）
    std::sort (actions.begin(), actions.end(), [] (const Action& a, const Action& b)
    {
        if (a.beat != b.beat)
            return a.beat < b.beat;
        return ! a.isStart && b.isStart;
    });

    for (const auto& action : actions)
    {
        const auto offset = std::clamp (toSampleIndex (sampleAtFrom + (action.beat - from) * samplesPerBeat),
                                        blockStart, blockEnd - 1);

        if (action.isStart)
        {
            activate (*action.note, offset, out);
            continue;
        }

        for (std::size_t i = 0; i < active.size(); ++i)
        {
            if (active[i].id == action.id)
            {
                deactivate (i, offset, out);
                break;
            }
        }
    }
}

void SketchPlayer::start (double beat, int sampleOffset, MidiEventList& out)
{
    if (schedule == nullptr)
        return;

    for (const auto& note : schedule->notes)
    {
        if (note.start >= beat - beatEpsilon)
            break; // 已排序；起点在 beat 及之后的交给 render

        if (note.end > beat + beatEpsilon)
            activate (note, sampleOffset, out);
    }
}

void SketchPlayer::stopAll (int sampleOffset, MidiEventList& out)
{
    while (! active.empty())
        deactivate (active.size() - 1, sampleOffset, out);
}

} // namespace curvable
