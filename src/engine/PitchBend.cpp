#include "engine/PitchBend.h"

#include <cmath>

namespace curvable
{

PitchBendValue centsToPitchBend (double cents, double rangeSemitones) noexcept
{
    if (! (rangeSemitones > 0.0) || ! std::isfinite (cents))
        return {};

    const double normalised = cents / (rangeSemitones * 100.0); // 范围内为 -1..1
    PitchBendValue result;

    if (normalised > 1.0)  { result.clipped = true; result.value = 16383; return result; }
    if (normalised < -1.0) { result.clipped = true; result.value = 0;     return result; }

    // 14 位弯音不对称：向上 8191 步，向下 8192 步
    const double steps = normalised >= 0.0 ? normalised * 8191.0 : normalised * 8192.0;
    result.value = 8192 + static_cast<int> (std::lround (steps));
    return result;
}

} // namespace curvable
