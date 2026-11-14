#include "model/Project.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

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
