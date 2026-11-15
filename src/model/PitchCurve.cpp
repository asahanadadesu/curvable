#include "model/PitchCurve.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace
{
    constexpr double maxCurvature = 8.0;  ///< tension = ±1 时指数曲线的陡峭程度
    constexpr double maxRelease = 0.125;  ///< 颤音段结尾收住所用的最长时间（拍）
    constexpr double twoPi = 6.283185307179586;
}

//==============================================================================
double Vibrato::beatsPerCycle (double secondsPerBeat) const noexcept
{
    if (rate <= 0.0)
        return 0.0;

    return sync ? 1.0 / rate : 1.0 / (rate * std::max (1.0e-6, secondsPerBeat));
}

double Vibrato::envelope (double x, double runLength) const noexcept
{
    if (x <= 0.0 || x >= runLength)
        return 0.0;

    const auto in = shape == Shape::sine && fade > 0.0 ? std::min (1.0, x / fade) : 1.0;
    const auto release = std::min (maxRelease, runLength * 0.25);
    const auto out = release > 0.0 ? std::min (1.0, (runLength - x) / release) : 1.0;
    return in * out;
}

double Vibrato::evaluate (double x, double runLength, double secondsPerBeat) const noexcept
{
    const auto env = envelope (x, runLength);

    if (env <= 0.0 || depth == 0.0)
        return 0.0;

    const auto cycles = x / beatsPerCycle (secondsPerBeat);

    if (shape == Shape::trill)
        return depth * env * trillWave (cycles - std::floor (cycles));

    return depth * env * std::sin (twoPi * cycles);
}

double Vibrato::trillWave (double phase) noexcept
{
    constexpr double edge = 0.06; // 切换用时（周期的比例），避免音高瞬间跳变产生咔哒声

    auto smooth = [] (double t) { t = std::clamp (t, 0.0, 1.0); return t * t * (3.0 - 2.0 * t); };

    if (phase < 0.5 - edge) return 0.0;
    if (phase < 0.5)        return smooth ((phase - (0.5 - edge)) / edge);
    if (phase < 1.0 - edge) return 1.0;
    return 1.0 - smooth ((phase - (1.0 - edge)) / edge);
}

void Vibrato::setShape (Shape newShape)
{
    if (newShape == shape)
        return;

    shape = newShape;
    const Vibrato defaults;

    if (shape == Shape::trill)
    {
        depth = 2.0; // 上方全音
        sync = true;
        rate = 4.0;  // 每拍 4 个来回 = 每拍交替 8 次
    }
    else
    {
        depth = defaults.depth;
        sync = defaults.sync;
        rate = defaults.rate;
    }
}

void Vibrato::normalise()
{
    auto finiteOr = [] (double v, double fallback) { return std::isfinite (v) ? v : fallback; };

    depth = shape == Shape::trill ? std::clamp (finiteOr (depth, 2.0), -12.0, 12.0)
                                  : std::clamp (std::abs (finiteOr (depth, 0.4)), 0.0, 12.0);
    rate = sync ? std::clamp (finiteOr (rate, 2.0), 0.25, 16.0) : std::clamp (finiteOr (rate, 5.5), 0.1, 20.0);
    fade = std::max (0.0, finiteOr (fade, 0.0));
}

//==============================================================================
bool PitchCurve::pointsFlat() const noexcept
{
    for (const auto& p : points)
        if (p.semitones != 0.0)
            return false;

    return true;
}

namespace
{
    bool isAudible (const Vibrato& v) noexcept { return v.depth != 0.0 && v.rate > 0.0; }
}

bool PitchCurve::hasSegmentVibrato() const noexcept
{
    for (const auto& p : points)
        if (p.vibrato)
            return true;

    return false;
}

bool PitchCurve::hasVibrato() const noexcept
{
    if (vibrato.wholeNote)
        return isAudible (vibrato);

    for (const auto& p : points)
        if (p.vibrato && isAudible (*p.vibrato))
            return true;

    return false;
}

bool PitchCurve::hasVibratoShape (Vibrato::Shape shape) const noexcept
{
    if (vibrato.wholeNote)
        return vibrato.shape == shape;

    for (const auto& p : points)
        if (p.vibrato && p.vibrato->shape == shape)
            return true;

    return false;
}

double PitchCurve::minBeatsPerCycle (double secondsPerBeat) const noexcept
{
    if (vibrato.wholeNote)
        return isAudible (vibrato) ? vibrato.beatsPerCycle (secondsPerBeat) : 0.0;

    double result = 0.0;

    for (const auto& p : points)
        if (p.vibrato && isAudible (*p.vibrato))
            if (const auto bpc = p.vibrato->beatsPerCycle (secondsPerBeat); result == 0.0 || bpc < result)
                result = bpc;

    return result;
}

bool PitchCurve::isFlat() const noexcept
{
    return pointsFlat() && ! hasVibrato();
}

int PitchCurve::segmentAt (double time) const noexcept
{
    int index = -1;

    for (int i = 0; i < (int) points.size(); ++i)
        if (points[(size_t) i].time <= time)
            index = i;

    return index;
}

//==============================================================================
template <typename Callback>
static void forEachRun (const PitchCurve& c, double length, Callback&& callback) noexcept
{
    // 回调返回 true 时停止
    if (c.vibrato.wholeNote)
    {
        if (isAudible (c.vibrato) && length > 0.0)
        {
            PitchCurve::Run run { 0.0, length, c.vibrato, -1, -1 };
            callback (run);
        }
        return;
    }

    // 相邻、参数相同的段连成一段
    bool inRun = false;
    PitchCurve::Run current;
    const auto& points = c.points;

    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const auto segStart = std::min (points[i].time, length);
        const auto segEnd = std::min (i + 1 < points.size() ? points[i + 1].time : length, length);
        const auto& v = points[i].vibrato;
        const auto active = v.has_value() && isAudible (*v) && segEnd > segStart;

        if (inRun && (! active || ! (*v == current.settings)))
        {
            inRun = false;
            if (callback (current))
                return;
        }

        if (! active)
            continue;

        if (! inRun)
        {
            inRun = true;
            current = { segStart, segEnd, *v, (int) i, (int) i };
        }

        current.end = segEnd;
        current.lastPoint = (int) i;
    }

    if (inRun)
        callback (current);
}

bool PitchCurve::findVibratoRun (double time, double length, Run& run) const noexcept
{
    bool found = false;

    forEachRun (*this, length, [&] (const Run& r)
    {
        if (time >= r.start && time < r.end)
        {
            run = r;
            found = true;
        }

        return found;
    });

    return found;
}

std::vector<PitchCurve::Run> PitchCurve::getVibratoRuns (double length) const
{
    std::vector<Run> runs;
    forEachRun (*this, length, [&] (const Run& r) { runs.push_back (r); return false; });
    return runs;
}

double PitchCurve::vibratoAt (double time, double length, double secondsPerBeat) const noexcept
{
    Run run;

    if (! findVibratoRun (time, length, run))
        return 0.0;

    return run.settings.evaluate (time - run.start, run.length(), secondsPerBeat);
}

//==============================================================================
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

    // 颤音：保守地直接加上最大的深度（trill 的音程可能为负）
    double depth = 0.0;
    forEachRun (*this, length, [&] (const Run& r) { depth = std::max (depth, std::abs (r.settings.depth)); return false; });

    return result + depth;
}

void PitchCurve::normalise()
{
    std::erase_if (points, [] (const CurvePoint& p) { return ! std::isfinite (p.time) || ! std::isfinite (p.semitones); });

    for (auto& p : points)
    {
        p.time = std::max (0.0, p.time);
        p.tension = std::isfinite (p.tension) ? std::clamp (p.tension, -1.0f, 1.0f) : 0.0f;

        if (p.vibrato)
        {
            p.vibrato->wholeNote = false;
            p.vibrato->normalise();
        }
    }

    std::stable_sort (points.begin(), points.end(), [] (const CurvePoint& a, const CurvePoint& b) { return a.time < b.time; });
    vibrato.normalise();
}

} // namespace curvable
