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

void trimNoteStarts (Project& target, const Project& original, const IdSet& ids, double deltaStart, double minLength)
{
    target.notes = original.notes;

    if (const auto b = boundsOf (original, ids); b.any)
        deltaStart = std::max (deltaStart, -b.minStart);

    for (auto& n : target.notes)
    {
        if (ids.count (n.id) == 0)
            continue;

        const auto d = std::min (deltaStart, n.length - minLength);

        // 音高曲线和包络都留在原来的时间位置
        for (auto* c : { &n.curve, &n.volume, &n.pan })
        {
            if (c->points.empty())
                continue;

            if (d > 0.0)
            {
                *c = c == &n.volume ? envelope::fromFader (splitCurve (envelope::toFader (*c), d).second)
                                    : splitCurve (*c, d).second;
            }
            else if (d < 0.0)
            {
                for (auto& point : c->points)
                    point.time -= d;

                // 新加的开头保持第一个节点的值（曲线在第一个节点之前本来就是这个值）
                auto first = c->points.front();
                first.time = 0.0;
                first.tension = 0.0f;
                first.vibrato.reset();
                c->points.insert (c->points.begin(), first);
            }
        }

        n.start += d;
        n.length -= d;
    }
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

IdSet duplicateInPlace (Project& p, const IdSet& ids)
{
    const auto b = boundsOf (p, ids);
    return b.any ? pasteNotes (p, copyNotes (p, ids), b.minStart) : IdSet {};
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

//==============================================================================
GlideResult glideNotes (Project& p, const IdSet& ids, double glideBeats)
{
    std::vector<Note> chain;

    for (const auto& n : p.notes)
        if (ids.count (n.id) != 0)
            chain.push_back (n);

    if (chain.size() < 2)
        return { 0, GlideResult::Error::tooFewNotes };

    std::stable_sort (chain.begin(), chain.end(), [] (const Note& a, const Note& b) { return a.start < b.start; });

    for (std::size_t i = 1; i < chain.size(); ++i)
        if (chain[i].start - chain[i - 1].start < 1.0e-9)
            return { 0, GlideResult::Error::sameStart };

    const auto& first = chain.front();
    const auto origin = first.start;
    double end = 0.0;

    for (const auto& n : chain)
        end = std::max (end, n.getEnd());

    // 每个音符在合并后的音符里占据 [regionStart, regionEnd)：从它的起点到滑向下一个音符的开始
    const auto count = chain.size();
    std::vector<double> regionStart (count), regionEnd (count);

    for (std::size_t i = 0; i < count; ++i)
        regionStart[i] = chain[i].start - origin;

    for (std::size_t i = 0; i < count; ++i)
    {
        if (i + 1 < count)
        {
            const auto ownEnd = chain[i].getEnd() - origin;

            if (ownEnd < regionStart[i + 1] - 1.0e-9)
            {
                // 两个音符之间有空隙：斜坡从前一个音符的终点开始，到下一个音符的起点为止
                regionEnd[i] = ownEnd;
            }
            else
            {
                // 首尾相接或重叠：没有空隙可用，在下一个音符的起点前留出一小段滑音
                const auto glide = std::max (1.0e-6, std::min (glideBeats, (regionStart[i + 1] - regionStart[i]) * 0.5));
                regionEnd[i] = regionStart[i + 1] - glide;
            }
        }
        else
        {
            regionEnd[i] = end - origin;
        }
    }

    PitchCurve merged;

    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& n = chain[i];
        const auto& c = n.curve;
        const auto offset = (double) (n.pitch - first.pitch);
        const auto a = regionStart[i], b = regionEnd[i];
        const auto localEnd = b - a;

        // 这一段在本音符自己曲线里的某个时刻：弯曲程度和颤音（整个音符的颤音换成每一段各带一份参数）
        auto segmentTension = [&] (double t) { const auto s = c.segmentAt (t); return s >= 0 ? c.points[(size_t) s].tension : 0.0f; };
        auto segmentVibrato = [&] (double t) -> std::optional<Vibrato>
        {
            if (c.vibrato.wholeNote)
            {
                auto v = c.vibrato;
                v.wholeNote = false;
                return v;
            }

            const auto s = c.segmentAt (t);
            return s >= 0 ? c.points[(size_t) s].vibrato : std::nullopt;
        };

        auto add = [&] (double t, float tension, std::optional<Vibrato> vibrato)
        {
            merged.points.push_back ({ a + t, c.evaluate (t) + offset, tension, vibrato });
        };

        // 起点、本音符曲线在区间内的节点、终点（滑音从这里开始）
        add (0.0, segmentTension (0.0), segmentVibrato (0.0));

        // 最后一个音符没有滑出段，它在终点上的节点（例如 Fall 的终点）也要保留
        const auto isLast = i + 1 == count;

        for (const auto& point : c.points)
            if (point.time > 1.0e-9 && (isLast ? point.time <= localEnd + 1.0e-9 : point.time < localEnd - 1.0e-9))
                add (point.time, point.tension, segmentVibrato (point.time));

        if (! isLast)
            add (localEnd, 0.0f, std::nullopt); // 滑音段：直线、没有颤音
    }

    merged.normalise();

    // 包络：每个音符的包络平移到它在合并后音符里的位置；没有包络的音符在它的起点取默认值
    auto mergeEnvelope = [&] (Envelope Note::* member)
    {
        Envelope result;

        if (std::all_of (chain.begin(), chain.end(), [&] (const Note& n) { return (n.*member).points.empty(); }))
            return result;

        for (const auto& n : chain)
        {
            const auto shift = n.start - origin;
            const auto& source = n.*member;

            if (source.points.empty())
                result.points.push_back ({ shift, 0.0 });

            for (const auto& point : source.points)
                result.points.push_back ({ shift + point.time, point.semitones, point.tension });
        }

        result.normalise();
        return result;
    };

    const auto mergedVolume = mergeEnvelope (&Note::volume);
    const auto mergedPan = mergeEnvelope (&Note::pan);

    // 换上合并后的音符
    const auto id = first.id;
    std::erase_if (p.notes, [&] (const Note& n) { return ids.count (n.id) != 0 && n.id != id; });

    if (auto* target = findNote (p, id))
    {
        target->start = origin;
        target->length = end - origin;
        target->curve = merged;
        target->volume = mergedVolume;
        target->pan = mergedPan;
    }

    return { id, GlideResult::Error::none };
}

//==============================================================================
std::vector<int> matchVoices (const std::vector<double>& from, const std::vector<double>& to)
{
    const auto n = (int) from.size(), m = (int) to.size();
    std::vector<int> result ((size_t) n, -1);

    if (n == 0 || m == 0 || m > 16)
        return result;

    // 移动量 + 一点平方项：总移动量相同时，更均匀（不交叉）的配对优先
    auto cost = [&] (int i, int j) { const auto d = std::abs (from[(size_t) i] - to[(size_t) j]); return d + 1.0e-3 * d * d; };

    const auto masks = 1 << m;
    const auto injective = n <= m;
    constexpr double inf = std::numeric_limits<double>::infinity();

    // dp[i][mask]：前 i 个声部已配好、用到的目标集合为 mask 时的最小代价；parent / choice 用于回溯
    std::vector<std::vector<double>> dp ((size_t) n + 1, std::vector<double> ((size_t) masks, inf));
    std::vector<std::vector<int>> choice ((size_t) n + 1, std::vector<int> ((size_t) masks, -1));
    std::vector<std::vector<int>> parent ((size_t) n + 1, std::vector<int> ((size_t) masks, -1));
    dp[0][0] = 0.0;

    for (int i = 0; i < n; ++i)
    {
        for (int mask = 0; mask < masks; ++mask)
        {
            if (dp[(size_t) i][(size_t) mask] == inf)
                continue;

            for (int j = 0; j < m; ++j)
            {
                if (injective && (mask & (1 << j)) != 0)
                    continue;

                const auto next = mask | (1 << j);
                const auto value = dp[(size_t) i][(size_t) mask] + cost (i, j);

                if (value < dp[(size_t) i + 1][(size_t) next])
                {
                    dp[(size_t) i + 1][(size_t) next] = value;
                    choice[(size_t) i + 1][(size_t) next] = j;
                    parent[(size_t) i + 1][(size_t) next] = mask;
                }
            }
        }
    }

    auto popcount = [] (int x) { int c = 0; for (; x != 0; x &= x - 1) ++c; return c; };

    // 结尾：一对一时任意用到 n 个目标的集合；否则必须用到全部目标
    int bestMask = -1;

    for (int mask = 0; mask < masks; ++mask)
    {
        const auto valid = injective ? popcount (mask) == n : mask == masks - 1;

        if (valid && dp[(size_t) n][(size_t) mask] < inf
            && (bestMask < 0 || dp[(size_t) n][(size_t) mask] < dp[(size_t) n][(size_t) bestMask]))
            bestMask = mask;
    }

    if (bestMask < 0)
        return result;

    for (int i = n, mask = bestMask; i > 0; --i)
    {
        result[(size_t) i - 1] = choice[(size_t) i][(size_t) mask];
        mask = parent[(size_t) i][(size_t) mask];
    }

    return result;
}

ChordGlideResult glideChords (Project& p, const IdSet& ids, double glideBeats)
{
    std::vector<Note> selected;

    for (const auto& n : p.notes)
        if (ids.count (n.id) != 0)
            selected.push_back (n);

    if (selected.size() < 2)
        return { {}, ChordGlideResult::Error::tooFewNotes };

    std::stable_sort (selected.begin(), selected.end(), [] (const Note& a, const Note& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });

    // 按起点分组
    std::vector<std::vector<Note>> groups;

    for (const auto& n : selected)
    {
        if (groups.empty() || n.start - groups.back().front().start > 1.0e-6)
            groups.emplace_back();

        groups.back().push_back (n);
    }

    if (groups.size() < 2)
        return { {}, ChordGlideResult::Error::oneChordOnly };

    for (const auto& g : groups)
        if (g.size() > 12)
            return { {}, ChordGlideResult::Error::chordTooLarge };

    // 每个声部是一串音符；先全部配好，最后每个声部只合并一次（滑音时机按原来的音符计算）
    struct Line
    {
        std::vector<NoteId> notes;
        double endPitch = 0.0; ///< 最后一个音符结束时的音高（含曲线）
    };

    auto endPitchOf = [] (const Note& n) { return n.pitch + n.curve.evaluate (n.length); };

    std::vector<Line> lines;

    for (const auto& n : groups.front())
        lines.push_back ({ { n.id }, endPitchOf (n) });

    for (std::size_t g = 1; g < groups.size(); ++g)
    {
        const auto& targets = groups[g];

        std::vector<double> from, to;
        for (const auto& line : lines)
            from.push_back (line.endPitch);
        for (const auto& t : targets)
            to.push_back ((double) t.pitch);

        const auto match = matchVoices (from, to);

        std::vector<bool> used (targets.size(), false);
        std::vector<Line> next;

        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            auto line = lines[i];
            const auto j = match[i];

            if (j < 0)
                continue; // 不应发生

            const auto& target = targets[(size_t) j];
            NoteId targetId = target.id;

            // 同一个音被两个声部配到：复制一份给后来的声部（同音重叠）
            if (used[(size_t) j])
            {
                targetId = p.addNote (target.start, target.length, target.pitch, target.velocity);
                findNote (p, targetId)->curve = target.curve;
            }

            used[(size_t) j] = true;
            line.notes.push_back (targetId);
            line.endPitch = endPitchOf (target);
            next.push_back (line);
        }

        // 没有被配到的音：单独开始一个新声部
        for (std::size_t j = 0; j < targets.size(); ++j)
            if (! used[j])
                next.push_back ({ { targets[j].id }, endPitchOf (targets[j]) });

        lines = std::move (next);
    }

    ChordGlideResult result;

    for (const auto& line : lines)
    {
        if (line.notes.size() < 2)
        {
            result.ids.insert (line.notes.front());
            continue;
        }

        const auto merged = glideNotes (p, IdSet (line.notes.begin(), line.notes.end()), glideBeats);
        result.ids.insert (merged.error == GlideResult::Error::none ? merged.id : line.notes.front());
    }

    return result;
}

//==============================================================================
std::pair<PitchCurve, PitchCurve> splitCurve (const PitchCurve& c, double cut)
{
    PitchCurve left, right;
    left.vibrato = right.vibrato = c.vibrato;

    if (c.points.empty())
        return { left, right };

    const auto& pts = c.points;
    const auto count = (int) pts.size();
    const auto value = c.evaluate (cut);
    const auto seg = c.segmentAt (cut); // 最后一个不晚于切点的节点

    // 被切开的那一段在切点处的比例（指数形状的一部分仍是同一种形状，只是弯曲程度按比例缩小）
    double u = 0.0;
    if (seg >= 0 && seg + 1 < count)
    {
        const auto span = pts[(size_t) seg + 1].time - pts[(size_t) seg].time;
        u = span > 0.0 ? (cut - pts[(size_t) seg].time) / span : 0.0;
    }

    // 前半段：切点之前的节点 + 切点上的节点
    for (int i = 0; i < count; ++i)
    {
        if (pts[(size_t) i].time >= cut - 1.0e-9)
            break;

        auto point = pts[(size_t) i];

        if (i == seg && seg + 1 < count)
            point.tension = (float) (point.tension * u);

        left.points.push_back (point);
    }

    left.points.push_back ({ cut, value, 0.0f, std::nullopt });

    // 后半段：起点上的节点（沿用被切开那一段剩下的部分）+ 切点之后的节点
    CurvePoint first { 0.0, value, 0.0f, std::nullopt };

    if (seg >= 0)
    {
        first.vibrato = pts[(size_t) seg].vibrato;
        first.tension = seg + 1 < count ? (float) (pts[(size_t) seg].tension * (1.0 - u)) : pts[(size_t) seg].tension;
    }

    right.points.push_back (first);

    for (const auto& point : pts)
    {
        if (point.time > cut + 1.0e-9)
        {
            auto shifted = point;
            shifted.time -= cut;
            right.points.push_back (shifted);
        }
    }

    return { left, right };
}

bool canSlice (const Note& n, double atBeat) noexcept
{
    return atBeat > n.start + minNoteLength && atBeat < n.getEnd() - minNoteLength;
}

IdSet sliceNotes (Project& p, const IdSet& ids, double atBeat)
{
    IdSet added;
    std::vector<Note> rights;

    for (auto& n : p.notes)
    {
        if (ids.count (n.id) == 0 || ! canSlice (n, atBeat))
            continue;

        const auto cut = atBeat - n.start;
        auto [left, right] = splitCurve (n.curve, cut);
        auto [volumeLeft, volumeRight] = splitCurve (envelope::toFader (n.volume), cut); // 音量在推子位置上切
        auto [panLeft, panRight] = splitCurve (n.pan, cut);

        Note r = n;
        r.start = atBeat;
        r.length = n.getEnd() - atBeat;
        r.curve = std::move (right);
        r.volume = n.volume.points.empty() ? Envelope {} : envelope::fromFader (std::move (volumeRight));
        r.pan = n.pan.points.empty() ? Envelope {} : std::move (panRight);

        // 后半段的音高改为切点处最接近的半音（例如切在滑音之后，后半段就是滑到的那个音）
        if (! r.curve.points.empty())
        {
            const auto shift = std::clamp ((int) std::lround (r.curve.points.front().semitones), -r.pitch, 127 - r.pitch);
            r.pitch += shift;

            for (auto& point : r.curve.points)
                point.semitones -= shift;
        }

        n.length = cut;
        n.curve = std::move (left);

        if (! n.volume.points.empty())
            n.volume = envelope::fromFader (std::move (volumeLeft));

        if (! n.pan.points.empty())
            n.pan = std::move (panLeft);

        rights.push_back (std::move (r));
    }

    for (auto& r : rights)
    {
        const auto id = p.addNote (r.start, r.length, r.pitch, r.velocity);
        auto* right = findNote (p, id);
        right->curve = std::move (r.curve);
        right->volume = std::move (r.volume);
        right->pan = std::move (r.pan);
        added.insert (id);
    }

    return added;
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

    // 插在一段中间时，两边都沿用这一段的弯曲程度和颤音
    const auto tension = it != curve.points.begin() ? std::prev (it)->tension : 0.0f;
    const auto vibrato = it != curve.points.begin() ? std::prev (it)->vibrato : std::nullopt;
    const auto inserted = curve.points.insert (it, { time, semitones, tension, vibrato });
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
}

void setCurveTension (PitchCurve& curve, int index, float tension)
{
    if (index >= 0 && index < (int) curve.points.size())
        curve.points[(size_t) index].tension = std::clamp (tension, -1.0f, 1.0f);
}

namespace
{
    /** 在 [0, end] 换上新的音头：删掉这段里的节点，插入 head（最后一个点在 end 处）。 */
    void replaceHead (PitchCurve& c, std::vector<CurvePoint> head, double end)
    {
        // end 之后这一段原来的弯曲程度和颤音
        float tensionAfter = 0.0f;
        std::optional<Vibrato> vibratoAfter;
        for (const auto& p : c.points)
        {
            if (p.time <= end + 1.0e-9)
            {
                tensionAfter = p.tension;
                vibratoAfter = p.vibrato;
            }
        }

        std::erase_if (c.points, [&] (const CurvePoint& p) { return p.time <= end + 1.0e-9; });
        head.back().tension = tensionAfter;
        head.back().vibrato = vibratoAfter;
        c.points.insert (c.points.begin(), head.begin(), head.end());
    }

    /** 在 [start, length] 换上新的音尾。 */
    void replaceTail (PitchCurve& c, std::vector<CurvePoint> tail, double start)
    {
        std::erase_if (c.points, [&] (const CurvePoint& p) { return p.time >= start - 1.0e-9; });
        c.points.insert (c.points.end(), tail.begin(), tail.end());
    }
}

void applyShape (PitchCurve& c, Shape shape, double length, double bendRange)
{
    length = std::max (length, minNoteLength);
    const auto limit = [&] (double semitones) { return std::clamp (semitones, -bendRange, bendRange); };

    switch (shape)
    {
        case Shape::clear:
            c = {};
            return;

        case Shape::scoop:
        {
            // 从下方快速滑入，接上原来的曲线
            const auto end = std::min (0.25, length * 0.5);
            const auto target = c.evaluate (end);
            replaceHead (c, { { 0.0, limit (target - 2.0), -0.5f }, { end, target, 0.0f } }, end);
            break;
        }

        case Shape::overshoot:
        {
            // 从下方冲过头，再回落到目标
            const auto peak = std::min (0.12, length * 0.25);
            const auto end = std::min (0.35, length * 0.5);
            const auto target = c.evaluate (end);
            replaceHead (c, { { 0.0, limit (target - 1.0), -0.5f }, { peak, limit (target + 0.5), -0.5f }, { end, target, 0.0f } }, end);
            break;
        }

        case Shape::fall:
        case Shape::doit:
        {
            // 音尾先慢后快地下坠（fall）或上挑（doit）
            const auto start = std::max (0.0, length - std::min (0.5, length * 0.5));
            const auto from = c.evaluate (start);
            const auto amount = shape == Shape::fall ? -5.0 : 3.0;
            replaceTail (c, { { start, from, 0.6f }, { length, limit (from + amount), 0.0f } }, start);
            break;
        }

        case Shape::bendRelease:
        {
            // 吉他式推弦（全音）再放回
            const auto up = limit (2.0);
            c.points = { { 0.0, 0.0, -0.3f }, { length * 0.3, up, 0.0f }, { length * 0.6, up, -0.3f }, { length * 0.9, 0.0, 0.0f } };
            break;
        }
    }

    c.normalise();
}

//==============================================================================
namespace
{
    /** 这个音符上某种形状的颤音参数：优先 preferred 附近的段，其次任意一段、整个音符的参数，否则默认值。 */
    Vibrato vibratoFor (const PitchCurve& c, int preferred, Vibrato::Shape shape)
    {
        auto matches = [&] (int i)
        {
            return i >= 0 && i < (int) c.points.size() && c.points[(size_t) i].vibrato && c.points[(size_t) i].vibrato->shape == shape;
        };

        std::optional<Vibrato> found;

        if (matches (preferred - 1))
            found = c.points[(size_t) preferred - 1].vibrato;
        else if (matches (preferred + 1))
            found = c.points[(size_t) preferred + 1].vibrato;
        else
            for (int i = 0; i < (int) c.points.size() && ! found; ++i)
                if (matches (i))
                    found = c.points[(size_t) i].vibrato;

        Vibrato v;

        if (found)
            v = *found;
        else if (c.vibrato.shape == shape)
            v = c.vibrato; // 整个音符的参数（去掉后仍保留着）
        else
            v.setShape (shape);

        v.wholeNote = false;
        return v;
    }
}

void setSegmentVibrato (PitchCurve& c, int segment, std::optional<Vibrato::Shape> shape)
{
    if (segment < 0 || segment >= (int) c.points.size())
        return;

    // 整个音符的颤音换成按段：每一段都带上这份参数，再改这一段
    if (c.vibrato.wholeNote)
    {
        auto v = c.vibrato;
        v.wholeNote = false;
        c.vibrato.wholeNote = false;

        for (auto& p : c.points)
            p.vibrato = v;
    }

    auto& target = c.points[(size_t) segment].vibrato;

    if (! shape)
        target.reset();
    else if (! target || target->shape != *shape)
        target = vibratoFor (c, segment, *shape);
}

void setWholeNoteVibrato (PitchCurve& c, std::optional<Vibrato::Shape> shape)
{
    if (shape)
    {
        c.vibrato = vibratoFor (c, -1, *shape);
        c.vibrato.wholeNote = true;
    }
    else
    {
        c.vibrato.wholeNote = false;
    }

    for (auto& p : c.points)
        p.vibrato.reset();
}

void removeVibrato (PitchCurve& c)
{
    setWholeNoteVibrato (c, std::nullopt);
}

void setRunVibrato (PitchCurve& c, const PitchCurve::Run& run, const Vibrato& settings)
{
    if (run.isWholeNote())
    {
        c.vibrato = settings;
        c.vibrato.wholeNote = true;
        c.vibrato.normalise();
        return;
    }

    auto v = settings;
    v.wholeNote = false;
    v.normalise();

    for (int i = std::max (0, run.firstPoint); i <= run.lastPoint && i < (int) c.points.size(); ++i)
        if (c.points[(size_t) i].vibrato)
            c.points[(size_t) i].vibrato = v;
}

void forEachVibrato (PitchCurve& c, const std::function<void (Vibrato&)>& change)
{
    if (c.vibrato.wholeNote)
    {
        change (c.vibrato);
        return;
    }

    for (auto& p : c.points)
        if (p.vibrato)
            change (*p.vibrato);
}

void applyPreset (PitchCurve& target, const PitchCurve& preset, double presetLength, double noteLength)
{
    const auto scale = presetLength > 0.0 && noteLength > 0.0 ? noteLength / presetLength : 1.0;

    target = preset;

    for (auto& p : target.points)
        p.time *= scale;

    target.normalise();
}

int nearestVibratoSyncIndex (double cyclesPerBeat) noexcept
{
    int best = 0;

    for (int i = 1; i < (int) std::size (vibratoSyncRates); ++i)
        if (std::abs (vibratoSyncRates[i] - cyclesPerBeat) < std::abs (vibratoSyncRates[best] - cyclesPerBeat))
            best = i;

    return best;
}

double snapSyncRate (const Vibrato& v, double cyclesPerBeat) noexcept
{
    if (v.shape == Vibrato::Shape::trill)
        return vibratoSyncRates[nearestVibratoSyncIndex (cyclesPerBeat * 2.0)] * 0.5;

    return vibratoSyncRates[nearestVibratoSyncIndex (cyclesPerBeat)];
}

const char* syncRateName (const Vibrato& v) noexcept
{
    return vibratoSyncNames[nearestVibratoSyncIndex (v.shape == Vibrato::Shape::trill ? v.rate * 2.0 : v.rate)];
}

int insertEnvelopePoint (Vibrato& v, double runLength, double x)
{
    auto pts = v.getEnvelope (runLength);

    if (pts.size() < 2 || (int) pts.size() >= Vibrato::maxEnvelopePoints || runLength <= 0.0)
        return -1;

    // 不能和首尾的点重合（首尾固定在 0 和 1）
    x = std::clamp (x, 1.0e-4, 1.0 - 1.0e-4);
    const auto value = Vibrato::levelAt (pts, x);
    const auto at = (size_t) (std::upper_bound (pts.begin() + 1, pts.end() - 1, x,
                                                [] (double u, const Vibrato::EnvelopePoint& p) { return u < p.x; }) - pts.begin());
    pts.insert (pts.begin() + (std::ptrdiff_t) at, { x, value });
    v.setEnvelope (pts);
    return (int) at;
}

void applyVolumeShape (Envelope& e, VolumeShape shape, double length)
{
    constexpr auto silent = envelope::volumeMin;

    switch (shape)
    {
        case VolumeShape::fadeIn:  e.points = { { 0.0, silent }, { length, 0.0 } }; break;
        case VolumeShape::fadeOut: e.points = { { 0.0, 0.0 }, { length, silent } }; break;
        case VolumeShape::swell:   e.points = { { 0.0, -24.0 }, { length * 0.5, 0.0 }, { length, -24.0 } }; break;
    }

    envelope::normaliseVolume (e);
}

Envelope stretchEnvelope (const Envelope& source, double sourceLength, double targetLength)
{
    auto result = source;
    const auto scale = sourceLength > 0.0 ? targetLength / sourceLength : 1.0;

    for (auto& p : result.points)
        p.time *= scale;

    return result;
}

void addVibrato (PitchCurve& c, int segment, double depth)
{
    if (segment < 0)
        setWholeNoteVibrato (c, Vibrato::Shape::sine);
    else
        setSegmentVibrato (c, segment, Vibrato::Shape::sine);

    auto* target = segment < 0 ? &c.vibrato
                 : (segment < (int) c.points.size() && c.points[(size_t) segment].vibrato ? &*c.points[(size_t) segment].vibrato : nullptr);

    if (target == nullptr)
        return;

    target->setEnvelope ({});
    target->depth = depth;
    target->normalise();
}

void removeSilentVibrato (PitchCurve& c)
{
    if (c.vibrato.wholeNote && c.vibrato.depth == 0.0)
        setWholeNoteVibrato (c, std::nullopt);

    for (auto& p : c.points)
        if (p.vibrato && p.vibrato->depth == 0.0)
            p.vibrato.reset();
}

void moveEnvelopePoint (Vibrato& v, double runLength, int index, double x, double depthThere)
{
    auto pts = v.getEnvelope (runLength);

    if (index < 0 || index >= (int) pts.size())
        return;

    const auto last = (int) pts.size() - 1;

    if (index == 0)
        x = 0.0;
    else if (index == last)
        x = 1.0;
    else
        x = std::clamp (x, pts[(size_t) index - 1].x + 1.0e-4, pts[(size_t) index + 1].x - 1.0e-4);

    depthThere = std::max (0.0, depthThere);
    const auto depth = std::abs (v.depth);

    if (depthThere > depth || depth <= 0.0)
    {
        // 比当前深度还深：整段加深，其他点按比例调低，实际深度不变
        const auto scale = depthThere > 0.0 ? depth / depthThere : 0.0;

        for (auto& p : pts)
            p.level *= scale;

        v.depth = depthThere;
        pts[(size_t) index] = { x, 1.0 };
    }
    else
    {
        pts[(size_t) index] = { x, depthThere / depth };
    }

    v.setEnvelope (pts);
}

bool removeEnvelopePoint (Vibrato& v, double runLength, int index)
{
    auto pts = v.getEnvelope (runLength);

    if (index <= 0 || index >= (int) pts.size() - 1)
        return false;

    pts.erase (pts.begin() + index);
    v.setEnvelope (pts);
    return true;
}

void setVibratoSync (Vibrato& v, bool sync, double secondsPerBeat)
{
    if (sync == v.sync)
        return;

    const auto cyclesPerBeat = 1.0 / std::max (1.0e-6, v.beatsPerCycle (secondsPerBeat));
    v.sync = sync;
    v.rate = sync ? snapSyncRate (v, cyclesPerBeat)
                  : std::round (cyclesPerBeat / std::max (1.0e-6, secondsPerBeat) * 10.0) / 10.0;
    v.normalise();
}

const Note* findNote (const Project& p, NoteId id)
{
    for (const auto& n : p.notes)
        if (n.id == id)
            return &n;

    return nullptr;
}

} // namespace curvable::editing
