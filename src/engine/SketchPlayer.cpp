#include "engine/SketchPlayer.h"
#include "engine/PitchBend.h"

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
void SketchPlayer::activate (const ScheduledNote& note, double beat, int sampleOffset, MidiEventList& out)
{
    if (active.size() >= maxActiveNotes)
        return;

    // 弯音轮交给新音符：先把弯音设到它当前位置的值，再发 note on
    wheelOwner = note.id;
    sendWheel (wheelValueAt (note, beat), sampleOffset, out);

    // 同音高已在发声：重新触发
    if (pitchCount[note.pitch] > 0)
        out.add ({ sampleOffset, false, note.pitch, 0 }, voice);

    out.add ({ sampleOffset, true, note.pitch, note.velocity }, voice);
    ++pitchCount[note.pitch];
    active.push_back ({ note.id, note.pitch, note.end, &note, ++activationCounter });
}

void SketchPlayer::deactivate (std::size_t index, int sampleOffset, MidiEventList& out)
{
    const auto pitch = active[index].pitch;

    if (--pitchCount[pitch] <= 0)
    {
        pitchCount[pitch] = 0;
        out.add ({ sampleOffset, false, pitch, 0 }, voice);
    }

    const auto id = active[index].id;
    active[index] = active.back();
    active.pop_back();

    // 弯音轮交还给仍在发声的、最近开始的音符；没有的话保持当前值（释放尾音）
    if (id == wheelOwner)
    {
        std::uint64_t latest = 0;
        wheelOwner = 0;

        for (const auto& a : active)
            if (a.order > latest)
            {
                latest = a.order;
                wheelOwner = a.id;
            }
    }
}

//==============================================================================
const SketchPlayer::Active* SketchPlayer::findWheelOwner() const noexcept
{
    if (wheelOwner == 0)
        return nullptr;

    for (const auto& a : active)
        if (a.id == wheelOwner)
            return &a;

    return nullptr;
}

void SketchPlayer::sendWheel (int value, int sampleOffset, MidiEventList& out)
{
    if (value == lastWheel)
        return;

    lastWheel = value;
    out.add (MidiNoteEvent::pitchWheel (sampleOffset, value, voice));
}

int SketchPlayer::wheelValueAt (const ScheduledNote& note, double beat) const noexcept
{
    if (note.curve.isFlat())
        return 8192;

    const auto t = std::clamp (beat - note.start, 0.0, note.end - note.start);
    const auto range = schedule != nullptr ? schedule->bendRange : 2.0;
    return centsToPitchBend (note.curve.evaluate (t, note.end - note.start, secondsPerBeat) * 100.0, range).value;
}

void SketchPlayer::renderBends (double beatFrom, double beatTo, double from, double sampleAtFrom,
                                int blockStart, int blockEnd, double samplesPerBeat, MidiEventList& out)
{
    const auto* owner = findWheelOwner();

    if (owner == nullptr || owner->note == nullptr || owner->note->curve.isFlat())
        return;

    const auto firstSample = std::max (blockStart, toSampleIndex (sampleAtFrom + (beatFrom - from) * samplesPerBeat));
    const auto endSample = std::min (blockEnd, toSampleIndex (sampleAtFrom + (beatTo - from) * samplesPerBeat));

    // 采样位置对齐到间隔的整数倍（每块的第 0 个采样总会求值一次）
    auto s = ((firstSample + bendInterval - 1) / bendInterval) * bendInterval;

    for (; s < endSample; s += bendInterval)
    {
        const auto beat = from + (s - sampleAtFrom) / samplesPerBeat;
        sendWheel (wheelValueAt (*owner->note, beat), s, out);
    }
}

//==============================================================================
void SketchPlayer::setSchedule (const Schedule* newSchedule, double currentBeat, int sampleOffset, MidiEventList& out)
{
    schedule = newSchedule;

    for (std::size_t i = active.size(); i-- > 0;)
    {
        const auto* note = schedule != nullptr ? schedule->find (active[i].id) : nullptr;

        if (note == nullptr || note->pitch != active[i].pitch || note->voice != voice || note->end <= currentBeat)
        {
            deactivate (i, sampleOffset, out);
        }
        else
        {
            active[i].end = note->end;
            active[i].note = note;
        }
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

    secondsPerBeat = samplesPerBeat / sampleRate;

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
            if (it->voice != voice)
                continue;

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

    // 按时间顺序处理音符事件，事件之间按间隔更新弯音
    double cursor = from;

    for (const auto& action : actions)
    {
        if (action.beat > cursor)
        {
            renderBends (cursor, action.beat, from, sampleAtFrom, blockStart, blockEnd, samplesPerBeat, out);
            cursor = action.beat;
        }

        const auto offset = std::clamp (toSampleIndex (sampleAtFrom + (action.beat - from) * samplesPerBeat),
                                        blockStart, blockEnd - 1);

        if (action.isStart)
        {
            activate (*action.note, action.beat, offset, out);
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

    renderBends (cursor, to, from, sampleAtFrom, blockStart, blockEnd, samplesPerBeat, out);
}

void SketchPlayer::start (double beat, int sampleOffset, MidiEventList& out)
{
    if (schedule == nullptr)
        return;

    for (const auto& note : schedule->notes)
    {
        if (note.start >= beat - beatEpsilon)
            break; // 已排序；起点在 beat 及之后的交给 render

        if (note.voice == voice && note.end > beat + beatEpsilon)
            activate (note, beat, sampleOffset, out);
    }
}

void SketchPlayer::resetWheel (int sampleOffset, MidiEventList& out)
{
    lastWheel = 8192;
    out.add (MidiNoteEvent::pitchWheel (sampleOffset, 8192, voice));
}

void SketchPlayer::stopAll (int sampleOffset, MidiEventList& out)
{
    while (! active.empty())
        deactivate (active.size() - 1, sampleOffset, out);

    // 停止播放：弯音回到中心，免得宿主里随后弹的音也是歪的
    wheelOwner = 0;
    sendWheel (8192, sampleOffset, out);
}

} // namespace curvable
