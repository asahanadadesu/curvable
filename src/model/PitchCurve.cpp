#include "model/PitchCurve.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace
{
    constexpr double maxCurvature = 8.0; ///< tension = ±1 时指数曲线的陡峭程度
}

bool PitchCurve::isFlat() const noexcept
{
    for (const auto& p : points)
        if (p.semitones != 0.0)
            return false;

    return true;
}

double PitchCurve::shape (double x, float tension) noexcept
{
    x = std::clamp (x, 0.0, 1.0);
    const auto k = (double) tension * maxCurvature;

    if (std::abs (k) < 1.0e-6)
        return x;

    return std::expm1 (k * x) / std::expm1 (k);
}

double PitchCurve::evaluate (double time) const noexcept
{
    if (points.empty())
        return 0.0;

    if (time <= points.front().time)
        return points.front().semitones;

    if (time >= points.back().time)
        return points.back().semitones;

    // 节点通常只有几个，线性查找即可
    for (std::size_t i = 1; i < points.size(); ++i)
    {
        const auto& b = points[i];

        if (time < b.time)
        {
            const auto& a = points[i - 1];
            const auto span = b.time - a.time;

            if (span <= 0.0)
                return b.semitones;

            return a.semitones + (b.semitones - a.semitones) * shape ((time - a.time) / span, a.tension);
        }
    }

    return points.back().semitones;
}

double PitchCurve::getMaxAbsOffset (double length) const noexcept
{
    // 形状单调，极值只出现在节点或区间端点上
    double result = std::abs (evaluate (0.0));
    result = std::max (result, std::abs (evaluate (length)));

    for (const auto& p : points)
        if (p.time >= 0.0 && p.time <= length)
            result = std::max (result, std::abs (p.semitones));

    return result;
}

void PitchCurve::normalise()
{
    std::erase_if (points, [] (const CurvePoint& p) { return ! std::isfinite (p.time) || ! std::isfinite (p.semitones); });

    for (auto& p : points)
    {
        p.time = std::max (0.0, p.time);
        p.tension = std::isfinite (p.tension) ? std::clamp (p.tension, -1.0f, 1.0f) : 0.0f;
    }

    std::stable_sort (points.begin(), points.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.time < b.time; });
}

} // namespace curvable
