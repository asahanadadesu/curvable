#include "engine/VoiceAllocator.h"

#include <algorithm>
#include <limits>

namespace curvable
{

bool areBendCompatible (const ScheduledNote& a, const ScheduledNote& b) noexcept
{
    const auto flatA = a.curve.isFlat(), flatB = b.curve.isFlat();

    if (flatA && flatB)
        return true;

    // 同时开始、曲线相同：弯音轮在两者重叠期间的值完全一样
    return flatA == flatB && a.start == b.start && a.curve == b.curve;
}

VoiceAllocation allocateVoices (std::vector<ScheduledNote>& notes, int maxVoices, double releaseGuard)
{
    maxVoices = std::max (1, maxVoices);
    releaseGuard = std::max (0.0, releaseGuard);

    VoiceAllocation result;
    std::vector<std::vector<std::size_t>> live ((std::size_t) maxVoices); // 每个声部上仍"占用"着的音符
    int used = 0;

    // 音符 j 对后来的音符 i 的占用到何时为止：两者都不弯时只到 j 结束，否则再加保护时长
    auto occupiedUntil = [&] (const ScheduledNote& j, const ScheduledNote& i)
    {
        return j.end + (j.curve.isFlat() && i.curve.isFlat() ? 0.0 : releaseGuard);
    };

    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        auto& note = notes[i];
        int chosen = -1;

        for (int v = 0; v < used && chosen < 0; ++v)
        {
            auto& occupants = live[(std::size_t) v];
            std::erase_if (occupants, [&] (std::size_t j)
            {
                // 对任何后来的音符都不再占用（加上保护时长也已结束）
                return notes[j].end + releaseGuard <= note.start;
            });

            const bool fits = std::all_of (occupants.begin(), occupants.end(), [&] (std::size_t j)
            {
                return occupiedUntil (notes[j], note) <= note.start || areBendCompatible (notes[j], note);
            });

            if (fits)
                chosen = v;
        }

        if (chosen < 0 && used < maxVoices)
            chosen = used++;

        if (chosen < 0)
        {
            // 超出上限：放进冲突最少的声部
            ++result.overflowNotes;
            note.overflow = true;
            auto fewest = std::numeric_limits<std::size_t>::max();

            for (int v = 0; v < used; ++v)
            {
                const auto& occupants = live[(std::size_t) v];
                const auto conflicts = (std::size_t) std::count_if (occupants.begin(), occupants.end(), [&] (std::size_t j)
                {
                    return occupiedUntil (notes[j], note) > note.start && ! areBendCompatible (notes[j], note);
                });

                if (conflicts < fewest)
                {
                    fewest = conflicts;
                    chosen = v;
                }
            }
        }

        // 被迫共用时，同一声部里与它冲突的音符也会受影响：弯音轮跟随最近开始的音符，
        // 所以先前的音符在重叠期间反而会被带偏。两边都标出来
        if (note.overflow)
            for (auto j : live[(std::size_t) chosen])
                if (occupiedUntil (notes[j], note) > note.start && ! areBendCompatible (notes[j], note))
                    notes[j].overflow = true;

        note.voice = chosen;
        live[(std::size_t) chosen].push_back (i);
    }

    result.voiceCount = std::max (1, used);
    return result;
}

} // namespace curvable
