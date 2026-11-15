#include "engine/Schedule.h"
#include "model/NoteEditing.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace curvable;
using namespace curvable::editing;
using Catch::Matchers::WithinAbs;

namespace
{
    /** 合并前后的音高在 [from, to) 上逐点比较（不含颤音），步长 1/64 拍。 */
    double maxPitchDifference (const Note& merged, const std::vector<Note>& originals, double from, double to)
    {
        double worst = 0.0;

        for (double t = from; t < to; t += 1.0 / 64.0)
        {
            // 这个时刻原本在响的、最晚开始的音符
            const Note* sounding = nullptr;
            for (const auto& n : originals)
                if (n.start <= t + 1.0e-9 && (sounding == nullptr || n.start > sounding->start))
                    sounding = &n;

            if (sounding == nullptr)
                continue;

            const auto expected = sounding->pitch + sounding->curve.evaluate (t - sounding->start);
            const auto actual = merged.pitch + merged.curve.evaluate (t - merged.start);
            worst = std::max (worst, std::abs (expected - actual));
        }

        return worst;
    }

    /** 曲线在 [0, length] 上是否连续（相邻采样之差不超过 limit）。 */
    bool isContinuous (const PitchCurve& c, double length, double limit)
    {
        double previous = c.evaluate (0.0);

        for (double t = 0.0; t <= length; t += 1.0 / 256.0)
        {
            const auto v = c.evaluate (t);
            if (std::abs (v - previous) > limit)
                return false;
            previous = v;
        }

        return true;
    }
}

TEST_CASE ("two adjacent notes merge into one note that glides to the second pitch", "[glide]")
{
    Project p;
    const auto a = p.addNote (1.0, 1.0, 60, 0.7f);
    const auto b = p.addNote (2.0, 1.0, 64, 0.3f);
    const auto nextId = p.nextId;

    const auto result = glideNotes (p, { a, b });
    REQUIRE (result.error == GlideResult::Error::none);
    CHECK (result.id == a);           // 沿用第一个音符
    REQUIRE (p.notes.size() == 1);
    CHECK (p.nextId == nextId);       // 没有分配新 id

    const auto& n = p.notes.front();
    CHECK (n.start == 1.0);
    CHECK (n.length == 2.0);
    CHECK (n.pitch == 60);
    CHECK (n.velocity == 0.7f);

    CHECK_THAT (n.curve.evaluate (0.0), WithinAbs (0.0, 1e-12));
    CHECK_THAT (n.curve.evaluate (0.75), WithinAbs (0.0, 1e-12));  // 滑音开始前保持原音高
    CHECK_THAT (n.curve.evaluate (1.0), WithinAbs (4.0, 1e-12));   // 在第二个音符原来的起点到达
    CHECK_THAT (n.curve.evaluate (2.0), WithinAbs (4.0, 1e-12));
    CHECK (n.curve.evaluate (0.875) > 0.0);
    CHECK (n.curve.evaluate (0.875) < 4.0);
    CHECK (isContinuous (n.curve, n.length, 0.1));
}

TEST_CASE ("short adjacent notes get a shorter glide; gaps become the ramp", "[glide]")
{
    Project p;
    const auto a = p.addNote (0.0, 0.25, 60, 0.8f);
    const auto b = p.addNote (0.25, 0.25, 62, 0.8f);  // 首尾相接、起点间距 1/4 拍：滑音 1/8 拍
    const auto c = p.addNote (2.0, 1.0, 57, 0.8f);    // 前面空了一段：斜坡就是这段空隙

    const auto originals = p.notes;
    REQUIRE (glideNotes (p, { a, b, c }).error == GlideResult::Error::none);
    const auto& n = p.notes.front();

    CHECK (n.length == 3.0);
    CHECK_THAT (n.curve.evaluate (0.125), WithinAbs (0.0, 1e-12)); // 第一个音符的前一半
    CHECK_THAT (n.curve.evaluate (0.25), WithinAbs (2.0, 1e-12));
    CHECK_THAT (n.curve.evaluate (0.5), WithinAbs (2.0, 1e-12));   // 斜坡从 b 的终点开始
    CHECK_THAT (n.curve.evaluate (1.25), WithinAbs (-0.5, 1e-12)); // 空隙正中：2 → −3 的一半
    CHECK_THAT (n.curve.evaluate (2.0), WithinAbs (-3.0, 1e-12));  // 在 c 的起点到达
    CHECK (isContinuous (n.curve, n.length, 0.2));

    // 除斜坡以外，合并后的音高与原来逐点一致
    CHECK (maxPitchDifference (n, originals, 0.0, 0.0625) < 1e-9);
    CHECK (maxPitchDifference (n, originals, 0.25, 0.5) < 1e-9);
    CHECK (maxPitchDifference (n, originals, 2.0, 3.0) < 1e-9);
}

TEST_CASE ("the ramp runs exactly from the first note's end to the second note's start", "[glide]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (1.5, 1.0, 64, 0.8f);

    REQUIRE (glideNotes (p, { a, b }).error == GlideResult::Error::none);
    const auto& c = p.notes.front().curve;

    CHECK_THAT (c.evaluate (1.0), WithinAbs (0.0, 1e-12));   // 第一个音符结束前不动
    CHECK (c.evaluate (1.01) > 0.0);                         // 结束后立刻开始
    CHECK_THAT (c.evaluate (1.25), WithinAbs (2.0, 1e-12));  // 直线
    CHECK (c.evaluate (1.49) < 4.0);
    CHECK_THAT (c.evaluate (1.5), WithinAbs (4.0, 1e-12));   // 正好在第二个音符的起点到达
}

TEST_CASE ("existing curves are kept and shifted into place", "[glide]")
{
    Project p;
    const auto a = p.addNote (0.0, 2.0, 60, 0.8f);
    const auto b = p.addNote (2.0, 2.0, 65, 0.8f);
    applyShape (findNote (p, a)->curve, Shape::scoop, 2.0, 12.0);
    applyShape (findNote (p, b)->curve, Shape::fall, 2.0, 12.0);

    const auto originals = p.notes;
    REQUIRE (glideNotes (p, { a, b }).error == GlideResult::Error::none);
    const auto& n = p.notes.front();

    CHECK_THAT (n.curve.evaluate (0.0), WithinAbs (-2.0, 1e-12));      // scoop 还在
    CHECK_THAT (n.curve.evaluate (2.0), WithinAbs (5.0, 1e-12));
    CHECK_THAT (n.curve.evaluate (4.0), WithinAbs (0.0, 1e-12));       // fall：65 − 5 = 60，偏移 0
    CHECK (maxPitchDifference (n, originals, 0.0, 1.75) < 1e-9);
    CHECK (maxPitchDifference (n, originals, 2.0, 4.0) < 1e-9);         // fall 的终点也保留了
    CHECK (isContinuous (n.curve, n.length, 0.25));
}

TEST_CASE ("vibrato becomes segment vibrato in each note's own part", "[glide]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (1.0, 2.0, 62, 0.8f);
    auto& vb = findNote (p, b)->curve.vibrato;
    vb.wholeNote = true;
    vb.depth = 0.6;
    vb.rate = 7.0;

    REQUIRE (glideNotes (p, { a, b }).error == GlideResult::Error::none);
    const auto& c = p.notes.front().curve;

    CHECK_FALSE (c.vibrato.wholeNote);

    const auto runs = c.getVibratoRuns (3.0);
    REQUIRE (runs.size() == 1);
    CHECK (runs[0].settings.depth == 0.6); // 参数跟着那个音符走
    CHECK (runs[0].settings.rate == 7.0);
    CHECK_THAT (runs[0].start, WithinAbs (1.0, 1e-12)); // 只在第二个音符那一段，不在第一个音符和滑音上
    CHECK_THAT (runs[0].end, WithinAbs (3.0, 1e-12));
}

TEST_CASE ("descending lines, overlaps and selection order do not matter", "[glide]")
{
    Project p;
    const auto c = p.addNote (2.0, 1.0, 55, 0.8f);
    const auto a = p.addNote (0.0, 1.5, 67, 0.8f); // 与下一个音符重叠
    const auto b = p.addNote (1.0, 1.0, 64, 0.8f);

    REQUIRE (glideNotes (p, { c, b, a }).id == a); // 按起点排序，不是按 id
    const auto& n = p.notes.front();

    CHECK (n.pitch == 67);
    CHECK (n.length == 3.0);
    CHECK_THAT (n.curve.evaluate (1.0), WithinAbs (-3.0, 1e-12)); // 重叠时在下一个音符的起点到达
    CHECK_THAT (n.curve.evaluate (2.5), WithinAbs (-12.0, 1e-12));
}

TEST_CASE ("chords and single notes are not merged", "[glide]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (0.0, 1.0, 64, 0.8f);
    const auto c = p.addNote (1.0, 1.0, 67, 0.8f);
    const auto before = p.notes;

    CHECK (glideNotes (p, { a, b, c }).error == GlideResult::Error::sameStart);
    CHECK (glideNotes (p, { a }).error == GlideResult::Error::tooFewNotes);
    CHECK (glideNotes (p, {}).error == GlideResult::Error::tooFewNotes);
    CHECK (p.notes == before); // 失败时不改动工程

    // 只合并选中的，其他音符不受影响
    REQUIRE (glideNotes (p, { a, c }).error == GlideResult::Error::none);
    CHECK (p.notes.size() == 2);
    CHECK (findNote (p, b) != nullptr);
}

TEST_CASE ("a merged note gets its own voice and needs the right bend range", "[glide]")
{
    Project p;
    p.bendRange = 2.0;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (1.0, 1.0, 67, 0.8f);
    const auto chord = p.addNote (0.0, 2.0, 48, 0.8f);

    REQUIRE (glideNotes (p, { a, b }).error == GlideResult::Error::none);
    const auto* merged = findNote (p, a);
    REQUIRE (merged != nullptr);

    // 偏移 7 半音超出 ±2：界面据此提示（getMaxAbsOffset 与发声时的截断一致）
    CHECK_THAT (merged->curve.getMaxAbsOffset (merged->length), WithinAbs (7.0, 1e-12));

    const auto s = Schedule::compile (p);
    CHECK (s.find (a)->voice != s.find (chord)->voice);
    CHECK (s.find (b) == nullptr);
}
