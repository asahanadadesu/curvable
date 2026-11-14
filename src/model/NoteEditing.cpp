#include "model/NoteEditing.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace curvable::editing
{

namespace
{
    struct Bounds
    {
        bool any = false;
        double minStart = std::numeric_limits<double>::max();
        double maxEnd = 0.0;
        int minPitch = 127;
        int maxPitch = 0;
    };

    Bounds boundsOf (const Project& p, const IdSet& ids)
    {
        Bounds b;

        for (const auto& n : p.notes)
        {
            if (ids.count (n.id) == 0)
                continue;

            b.any = true;
            b.minStart = std::min (b.minStart, n.start);
            b.maxEnd = std::max (b.maxEnd, n.getEnd());
            b.minPitch = std::min (b.minPitch, n.pitch);
            b.maxPitch = std::max (b.maxPitch, n.pitch);
        }

        return b;
    }
}

double snapNearest (double beat, double grid) noexcept
{
    return grid > 0.0 ? std::round (beat / grid) * grid : beat;
}

double snapFloor (double beat, double grid) noexcept
{
    return grid > 0.0 ? std::floor (beat / grid + 1.0e-9) * grid : beat;
}

void moveNotes (Project& target, const Project& original, const IdSet& ids, double deltaBeats, int deltaPitch)
{
    target.notes = original.notes;
    const auto b = boundsOf (original, ids);

    if (! b.any)
        return;

    deltaBeats = std::max (deltaBeats, -b.minStart);
    deltaPitch = std::clamp (deltaPitch, -b.minPitch, 127 - b.maxPitch);

    for (auto& n : target.notes)
    {
        if (ids.count (n.id) != 0)
        {
            n.start += deltaBeats;
            n.pitch += deltaPitch;
        }
    }
}

void resizeNotes (Project& target, const Project& original, const IdSet& ids, double deltaLength, double minLength)
{
    target.notes = original.notes;

    for (auto& n : target.notes)
        if (ids.count (n.id) != 0)
            n.length = std::max (minLength, n.length + deltaLength);
}

void changeVelocities (Project& target, const Project& original, const IdSet& ids, float delta)
{
    target.notes = original.notes;

    for (auto& n : target.notes)
        if (ids.count (n.id) != 0)
            n.velocity = std::clamp (n.velocity + delta, 0.01f, 1.0f);
}

void setVelocity (Project& p, const IdSet& ids, float velocity)
{
    for (auto& n : p.notes)
        if (ids.count (n.id) != 0)
            n.velocity = std::clamp (velocity, 0.01f, 1.0f);
}

void transposeNotes (Project& p, const IdSet& ids, int semitones)
{
    const auto original = p;
    moveNotes (p, original, ids, 0.0, semitones);
}

void shiftNotes (Project& p, const IdSet& ids, double deltaBeats)
{
    const auto original = p;
    moveNotes (p, original, ids, deltaBeats, 0);
}

void deleteNotes (Project& p, const IdSet& ids)
{
    std::erase_if (p.notes, [&] (const Note& n) { return ids.count (n.id) != 0; });
}

std::vector<Note> copyNotes (const Project& p, const IdSet& ids)
{
    std::vector<Note> clip;
    const auto b = boundsOf (p, ids);

    if (! b.any)
        return clip;

    for (const auto& n : p.notes)
    {
        if (ids.count (n.id) != 0)
        {
            auto copy = n;
            copy.start -= b.minStart;
            copy.id = 0;
            clip.push_back (copy);
        }
    }

    return clip;
}

IdSet pasteNotes (Project& p, const std::vector<Note>& clip, double atBeat)
{
    IdSet added;
    atBeat = std::max (0.0, atBeat);

    for (const auto& n : clip)
    {
        const auto id = p.addNote (atBeat + n.start, n.length, n.pitch, n.velocity);
        p.notes.back().curve = n.curve; // 曲线跟着音符走
        added.insert (id);
    }

    return added;
}

IdSet duplicateNotes (Project& p, const IdSet& ids, double grid)
{
    const auto b = boundsOf (p, ids);

    if (! b.any)
        return {};

    const auto span = b.maxEnd - b.minStart;
    auto offset = grid > 0.0 ? std::ceil (span / grid - 1.0e-9) * grid : span;

    if (offset <= 0.0)
        offset = grid > 0.0 ? grid : 1.0;

    return pasteNotes (p, copyNotes (p, ids), b.minStart + offset);
}

IdSet notesInRange (const Project& p, double beatStart, double beatEnd, int pitchLow, int pitchHigh)
{
    if (beatEnd < beatStart)
        std::swap (beatStart, beatEnd);

    if (pitchHigh < pitchLow)
        std::swap (pitchLow, pitchHigh);

    IdSet result;

    for (const auto& n : p.notes)
        if (n.start < beatEnd && n.getEnd() > beatStart && n.pitch >= pitchLow && n.pitch <= pitchHigh)
            result.insert (n.id);

    return result;
}

IdSet allNotes (const Project& p)
{
    IdSet result;

    for (const auto& n : p.notes)
        result.insert (n.id);

    return result;
}

IdSet existingOnly (const Project& p, const IdSet& ids)
{
    IdSet result;

    for (const auto& n : p.notes)
        if (ids.count (n.id) != 0)
            result.insert (n.id);

    return result;
}

Note* findNote (Project& p, NoteId id)
{
    for (auto& n : p.notes)
        if (n.id == id)
            return &n;

    return nullptr;
}

//==============================================================================
int insertCurvePoint (PitchCurve& curve, double time, double semitones)
{
    time = std::max (0.0, time);

    if (curve.points.empty() && time > 1.0e-9)
        curve.points.push_back ({ 0.0, 0.0, 0.0f });

    const auto it = std::upper_bound (curve.points.begin(), curve.points.end(), time,
                                      [] (double t, const CurvePoint& p) { return t < p.time; });

    // 插在一段中间时沿用这一段的弯曲程度
    const auto tension = it != curve.points.begin() ? std::prev (it)->tension : 0.0f;
    const auto inserted = curve.points.insert (it, { time, semitones, tension });
    return (int) std::distance (curve.points.begin(), inserted);
}

void moveCurvePoint (PitchCurve& curve, int index, double time, double semitones, double length)
{
    if (index < 0 || index >= (int) curve.points.size())
        return;

    auto lo = 0.0, hi = std::max (0.0, length);

    if (index > 0)
        lo = std::max (lo, curve.points[(size_t) index - 1].time);

    if (index + 1 < (int) curve.points.size())
        hi = std::min (hi, curve.points[(size_t) index + 1].time);

    auto& p = curve.points[(size_t) index];
    p.time = std::clamp (time, lo, std::max (lo, hi));
    p.semitones = semitones;
}

void removeCurvePoint (PitchCurve& curve, int index)
{
    if (index < 0 || index >= (int) curve.points.size())
        return;

    curve.points.erase (curve.points.begin() + index);

    if (curve.isFlat())
        curve.points.clear();
}

void setCurveTension (PitchCurve& curve, int index, float tension)
{
    if (index >= 0 && index < (int) curve.points.size())
        curve.points[(size_t) index].tension = std::clamp (tension, -1.0f, 1.0f);
}

const Note* findNote (const Project& p, NoteId id)
{
    for (const auto& n : p.notes)
        if (n.id == id)
            return &n;

    return nullptr;
}

} // namespace curvable::editing
