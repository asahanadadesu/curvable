#include "engine/Schedule.h"
#include "model/NoteEditing.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using namespace curvable;
using namespace curvable::editing;
using Catch::Matchers::WithinAbs;

namespace
{
    /** 某一时刻正在发声的音符的音高（含曲线，不含颤音）；同一时刻有多个音符时取 pitch 最接近 near 的。 */
    double pitchAt (const Project& p, double beat, double near)
    {
        double best = -1000.0;

        for (const auto& n : p.notes)
        {
            if (beat < n.start - 1.0e-9 || beat > n.getEnd() + 1.0e-9)
                continue;

            const auto value = n.pitch + n.curve.evaluate (beat - n.start);

            if (best < -999.0 || std::abs (value - near) < std::abs (best - near))
                best = value;
        }

        return best;
    }

    /** 切开前后，从 from 到 to 的音高逐点一致（每 1/128 拍）。 */
    double maxDifference (const Note& original, const Project& after, double from, double to)
    {
        double worst = 0.0;

        for (double beat = from; beat <= to; beat += 1.0 / 128.0)
        {
            const auto expected = original.pitch + original.curve.evaluate (beat - original.start);
            worst = std::max (worst, std::abs (pitchAt (after, beat, expected) - expected));
        }

        return worst;
    }
}

//==============================================================================
TEST_CASE ("slicing a plain note gives two plain notes", "[slice]")
{
    Project p;
    const auto id = p.addNote (1.0, 2.0, 60, 0.6f);

    const auto added = sliceNotes (p, { id }, 2.0);
    REQUIRE (added.size() == 1);
    REQUIRE (p.notes.size() == 2);

    const auto* left = findNote (p, id);
    const auto* right = findNote (p, *added.begin());
    CHECK (left->length == 1.0);
    CHECK (right->start == 2.0);
    CHECK (right->length == 1.0);
    CHECK (right->pitch == 60);
    CHECK (right->velocity == 0.6f);
    CHECK (left->curve.points.empty());
    CHECK (right->curve.points.empty());
}

TEST_CASE ("slicing keeps every curve exactly, including the bend shape", "[slice]")
{
    for (const float tension : { 0.0f, 0.7f, -0.9f })
    {
        for (const double cut : { 0.3, 1.0, 1.37, 2.5 })
        {
            Project p;
            const auto id = p.addNote (0.0, 3.0, 60, 0.8f);
            findNote (p, id)->curve.points = { { 0.0, 0.0, tension }, { 2.0, 3.0, tension }, { 2.8, 2.0 } };
            const auto original = *findNote (p, id);

            REQUIRE (sliceNotes (p, { id }, cut).size() == 1);
            CHECK (maxDifference (original, p, 0.0, 3.0) < 1e-6); // 弯曲程度以 float 保存，误差约 1e-8 半音
        }
    }
}

TEST_CASE ("slicing a glided note after the glide gives back the second pitch", "[slice]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (1.5, 1.0, 64, 0.8f);
    REQUIRE (glideNotes (p, { a, b }).error == GlideResult::Error::none);
    const auto merged = *findNote (p, a);

    const auto added = sliceNotes (p, { a }, 2.0);
    REQUIRE (added.size() == 1);
    const auto* right = findNote (p, *added.begin());

    CHECK (right->pitch == 64);                                    // 后半段就是 E
    CHECK_THAT (right->curve.evaluate (0.0), WithinAbs (0.0, 1e-12));
    CHECK (maxDifference (merged, p, 0.0, 2.5) < 1e-9);
}

TEST_CASE ("slicing in the middle of a slide rounds the second part to the nearest semitone", "[slice]")
{
    Project p;
    const auto id = p.addNote (0.0, 2.0, 60, 0.8f);
    findNote (p, id)->curve.points = { { 0.0, 0.0 }, { 2.0, 5.0 } };
    const auto original = *findNote (p, id);

    const auto added = sliceNotes (p, { id }, 1.1); // 切点处 +2.75
    const auto* right = findNote (p, *added.begin());

    CHECK (right->pitch == 63);
    CHECK_THAT (right->curve.evaluate (0.0), WithinAbs (-0.25, 1e-12));
    CHECK (maxDifference (original, p, 0.0, 2.0) < 1e-9);
}

TEST_CASE ("slicing keeps vibrato segments and settings on both sides", "[slice]")
{
    Project p;
    const auto id = p.addNote (0.0, 4.0, 60, 0.8f);
    auto& c = findNote (p, id)->curve;
    Vibrato v;
    v.depth = 0.7;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0, 0.0f, v } };

    const auto added = sliceNotes (p, { id }, 2.0);
    const auto& left = findNote (p, id)->curve;
    const auto& right = findNote (p, *added.begin())->curve;

    REQUIRE (left.getVibratoRuns (2.0).size() == 1);
    CHECK (left.getVibratoRuns (2.0)[0].settings.depth == 0.7);
    CHECK_THAT (left.getVibratoRuns (2.0)[0].start, WithinAbs (1.0, 1e-12));
    REQUIRE (right.getVibratoRuns (2.0).size() == 1);
    CHECK (right.getVibratoRuns (2.0)[0].settings.depth == 0.7);
    CHECK_THAT (right.getVibratoRuns (2.0)[0].start, WithinAbs (0.0, 1e-12)); // 后半段从头就有颤音
}

TEST_CASE ("slicing ignores notes that do not cross the cut or are too close to an end", "[slice]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (2.0, 1.0, 62, 0.8f);
    const auto c = p.addNote (0.0, 4.0, 64, 0.8f);

    const auto added = sliceNotes (p, { a, b, c }, 1.5);
    CHECK (added.size() == 1);          // 只切开了 c
    CHECK (findNote (p, c)->length == 1.5);

    CHECK (sliceNotes (p, { a }, 1.0).empty());                         // 正好在终点
    CHECK (sliceNotes (p, { a }, 1.0 - minNoteLength * 0.5).empty());   // 离终点太近
}

//==============================================================================
TEST_CASE ("voice matching minimises movement and prefers even motion", "[chords]")
{
    // C E G → C F A：C 不动，E→F，G→A
    CHECK (matchVoices ({ 60, 64, 67 }, { 60, 65, 69 }) == std::vector<int> { 0, 1, 2 });

    // 顺序无关：目标乱序时也找到同样的配对
    CHECK (matchVoices ({ 60, 64, 67 }, { 69, 60, 65 }) == std::vector<int> { 1, 2, 0 });

    // 总移动量相同时取更均匀的：C E G → F A C（各上行 5，而不是 C→C、G→A、E→F……这里正好也是平行）
    CHECK (matchVoices ({ 60, 64, 67 }, { 65, 69, 72 }) == std::vector<int> { 0, 1, 2 });

    // 少 → 多：一对一，多出来的目标没有配到
    const auto grow = matchVoices ({ 60, 67 }, { 60, 64, 67 });
    CHECK (grow == std::vector<int> { 0, 2 });

    // 多 → 少：每个目标至少配到一次
    const auto shrink = matchVoices ({ 60, 64, 67 }, { 60, 65 });
    CHECK (shrink[0] == 0);
    CHECK (shrink[1] == 1);
    CHECK (shrink[2] == 1);
}

TEST_CASE ("two chords glide voice by voice into legato notes", "[chords]")
{
    Project p;
    const auto c = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto e = p.addNote (0.0, 1.0, 64, 0.8f);
    const auto g = p.addNote (0.0, 1.0, 67, 0.8f);
    p.addNote (1.5, 1.0, 60, 0.8f);
    p.addNote (1.5, 1.0, 65, 0.8f);
    p.addNote (1.5, 1.0, 69, 0.8f);

    const auto result = glideChords (p, { 1, 2, 3, 4, 5, 6 });
    REQUIRE (result.error == ChordGlideResult::Error::none);
    CHECK (result.ids == IdSet { c, e, g });
    REQUIRE (p.notes.size() == 3);

    for (const auto& [id, from, to] : { std::tuple { c, 60, 60 }, std::tuple { e, 64, 65 }, std::tuple { g, 67, 69 } })
    {
        const auto* n = findNote (p, id);
        REQUIRE (n != nullptr);
        CHECK (n->pitch == from);
        CHECK (n->length == 2.5);
        CHECK_THAT (n->curve.evaluate (1.0), WithinAbs (0.0, 1e-12));                // 第一个和弦结束前不动
        CHECK_THAT (n->curve.evaluate (1.5), WithinAbs ((double) (to - from), 1e-12)); // 在第二个和弦的起点到达
    }

    // 不动的声部和两个移动的声部是不同的曲线，各自分到声部
    const auto s = Schedule::compile (p);
    CHECK (s.find (e)->voice != s.find (g)->voice);
}

TEST_CASE ("fewer notes in the next chord double up, more notes start new voices", "[chords]")
{
    SECTION ("three to two")
    {
        Project p;
        p.addNote (0.0, 1.0, 60, 0.8f);
        p.addNote (0.0, 1.0, 64, 0.8f);
        p.addNote (0.0, 1.0, 67, 0.8f);
        p.addNote (1.0, 1.0, 62, 0.8f);
        p.addNote (1.0, 1.0, 65, 0.8f);

        const auto result = glideChords (p, editing::allNotes (p));
        REQUIRE (result.error == ChordGlideResult::Error::none);
        CHECK (result.ids.size() == 3);
        REQUIRE (p.notes.size() == 3);

        std::vector<int> ends;
        for (const auto& n : p.notes)
            ends.push_back ((int) std::lround (n.pitch + n.curve.evaluate (n.length)));
        std::sort (ends.begin(), ends.end());
        CHECK (ends == std::vector<int> { 62, 65, 65 }); // G 滑到最近的 F，与 E→F 重叠
    }

    SECTION ("two to three")
    {
        Project p;
        p.addNote (0.0, 1.0, 60, 0.8f);
        p.addNote (0.0, 1.0, 67, 0.8f);
        p.addNote (1.0, 1.0, 60, 0.8f);
        const auto newVoice = p.addNote (1.0, 1.0, 64, 0.8f);
        p.addNote (1.0, 1.0, 67, 0.8f);

        const auto result = glideChords (p, editing::allNotes (p));
        REQUIRE (result.error == ChordGlideResult::Error::none);
        CHECK (result.ids.size() == 3);
        CHECK (p.notes.size() == 3);
        CHECK (result.ids.count (newVoice) == 1);       // E 单独开始
        CHECK (findNote (p, newVoice)->start == 1.0);
        CHECK (findNote (p, newVoice)->curve.points.empty());
    }
}

TEST_CASE ("three chords chain and keep each glide on time", "[chords]")
{
    Project p;
    for (const auto pitch : { 60, 64, 67 }) p.addNote (0.0, 1.0, pitch, 0.8f); // C
    for (const auto pitch : { 60, 65, 69 }) p.addNote (1.0, 1.0, pitch, 0.8f); // F/C
    for (const auto pitch : { 59, 62, 67 }) p.addNote (2.5, 1.0, pitch, 0.8f); // G/B（前面有空隙）

    const auto result = glideChords (p, editing::allNotes (p));
    REQUIRE (result.error == ChordGlideResult::Error::none);
    REQUIRE (p.notes.size() == 3);

    std::vector<std::vector<int>> paths;
    for (const auto& n : p.notes)
    {
        CHECK (n.start == 0.0);
        CHECK (n.length == 3.5);
        paths.push_back ({ n.pitch + (int) std::lround (n.curve.evaluate (0.5)),
                           n.pitch + (int) std::lround (n.curve.evaluate (1.5)),
                           n.pitch + (int) std::lround (n.curve.evaluate (3.0)) });

        // 第二个和弦结束（2.0）之前保持，空隙里滑，2.5 到达
        CHECK_THAT (n.curve.evaluate (2.0), WithinAbs (n.curve.evaluate (1.5), 1e-12));
    }

    std::sort (paths.begin(), paths.end());
    CHECK (paths == std::vector<std::vector<int>> { { 60, 60, 59 }, { 64, 65, 62 }, { 67, 69, 67 } });
}

TEST_CASE ("a single line still glides, chords alone are refused", "[chords]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (1.0, 1.0, 62, 0.8f);
    const auto c = p.addNote (2.0, 1.0, 64, 0.8f);

    const auto result = glideChords (p, { a, b, c });
    REQUIRE (result.error == ChordGlideResult::Error::none);
    CHECK (result.ids == IdSet { a });
    REQUIRE (p.notes.size() == 1);
    CHECK_THAT (p.notes.front().curve.evaluate (1.0), WithinAbs (2.0, 1e-12));
    CHECK_THAT (p.notes.front().curve.evaluate (2.0), WithinAbs (4.0, 1e-12));

    Project chord;
    chord.addNote (0.0, 1.0, 60, 0.8f);
    chord.addNote (0.0, 1.0, 64, 0.8f);
    const auto before = chord.notes;
    CHECK (glideChords (chord, editing::allNotes (chord)).error == ChordGlideResult::Error::oneChordOnly);
    CHECK (chord.notes == before);
}
