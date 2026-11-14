#include "engine/Schedule.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace
{
    constexpr double minLength = 1.0e-6;
    constexpr double barLength = 4.0;
}

Schedule Schedule::compile (const Project& project)
{
    Schedule s;
    s.notes.reserve (project.notes.size());

    for (const auto& n : project.notes)
    {
        if (n.pitch < 0 || n.pitch > 127 || ! std::isfinite (n.start) || ! std::isfinite (n.length)
            || n.length < minLength || n.start < 0.0)
            continue;

        const auto velocity = std::clamp (static_cast<int> (std::lround (n.velocity * 127.0f)), 1, 127);
        s.notes.push_back ({ n.id, n.start, n.start + n.length, n.pitch, velocity });
    }

    std::sort (s.notes.begin(), s.notes.end(), [] (const ScheduledNote& a, const ScheduledNote& b)
    {
        if (a.start != b.start)
            return a.start < b.start;
        return a.pitch < b.pitch;
    });

    double end = 0.0;
    for (const auto& n : s.notes)
        end = std::max (end, n.end);

    s.length = roundUpToBar (end, barLength);
    s.cues = project.cues;
    return s;
}

const ScheduledNote* Schedule::find (NoteId id) const noexcept
{
    for (const auto& n : notes)
        if (n.id == id)
            return &n;

    return nullptr;
}

void Schedule::resolveCue (const TriggerCue& cue, double& start, double& end) const noexcept
{
    start = std::max (0.0, cue.start);
    end = cue.end > start ? cue.end : length;

    if (end <= start + minLength)
        end = start + barLength;
}

} // namespace curvable
