#include "model/NoteEditing.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>

using namespace curvable;
using namespace curvable::editing;
using Catch::Matchers::WithinAbs;

namespace
{
    Project threeNotes (NoteId& a, NoteId& b, NoteId& c)
    {
        Project p;
        a = p.addNote (0.0, 1.0, 60, 0.5f);
        b = p.addNote (1.0, 0.5, 64, 0.8f);
        c = p.addNote (2.0, 1.0, 67, 0.3f);
        return p;
    }
}

TEST_CASE ("snapping", "[editing]")
{
    CHECK_THAT (snapNearest (1.13, 0.25), WithinAbs (1.25, 1e-12));
    CHECK_THAT (snapNearest (1.12, 0.25), WithinAbs (1.0, 1e-12));
    CHECK_THAT (snapNearest (1.13, 0.0), WithinAbs (1.13, 1e-12));

    CHECK_THAT (snapFloor (1.24, 0.25), WithinAbs (1.0, 1e-12));
    CHECK_THAT (snapFloor (0.75, 0.25), WithinAbs (0.75, 1e-12));
    // 浮点误差导致略小于网格线时不应退一格
    CHECK_THAT (snapFloor (3.0 * (1.0 / 3.0) * 0.75 - 1e-12, 0.25), WithinAbs (0.75, 1e-9));
}

TEST_CASE ("moving a group clamps at time zero and pitch range as a whole", "[editing]")
{
    NoteId a, b, c;
    const auto original = threeNotes (a, b, c);
    auto target = original;

    moveNotes (target, original, { a, b }, -5.0, 0);
    CHECK (findNote (target, a)->start == 0.0); // 已在 0 拍，组不能左移
    CHECK (findNote (target, b)->start == 1.0);

    moveNotes (target, original, { b, c }, -1.5, 100);
    CHECK_THAT (findNote (target, b)->start, WithinAbs (0.0, 1e-12));
    CHECK_THAT (findNote (target, c)->start, WithinAbs (1.0, 1e-12));
    CHECK (findNote (target, c)->pitch == 127);
    CHECK (findNote (target, b)->pitch == 124); // 间隔保持不变
    CHECK (findNote (target, a)->pitch == 60);  // 未选中的不动
}

TEST_CASE ("trimming a note's start keeps its end and the curve where it was", "[editing]")
{
    Project original;
    const auto a = original.addNote (1.0, 2.0, 60, 0.8f);
    const auto b = original.addNote (2.0, 1.0, 64, 0.8f);
    auto& curve = findNote (original, a)->curve;
    insertCurvePoint (curve, 0.0, 0.0);
    insertCurvePoint (curve, 1.0, 2.0);

    // 缩短：切掉开头，剩下的部分音高不变
    auto target = original;
    trimNoteStarts (target, original, { a }, 0.5, minNoteLength);
    const auto* n = findNote (target, a);
    CHECK_THAT (n->start, WithinAbs (1.5, 1e-12));
    CHECK_THAT (n->length, WithinAbs (1.5, 1e-12));
    for (double beat : { 1.6, 2.0, 2.5, 2.9 })
        CHECK_THAT (n->curve.evaluate (beat - n->start), WithinAbs (curve.evaluate (beat - 1.0), 1e-9));

    // 拉长：节点后移，新的开头保持第一个节点的音高
    trimNoteStarts (target, original, { a }, -0.75, minNoteLength);
    n = findNote (target, a);
    CHECK_THAT (n->start, WithinAbs (0.25, 1e-12));
    CHECK_THAT (n->length, WithinAbs (2.75, 1e-12));
    CHECK_THAT (n->curve.evaluate (0.3), WithinAbs (0.0, 1e-9));
    for (double beat : { 1.0, 1.5, 2.0, 2.9 })
        CHECK_THAT (n->curve.evaluate (beat - n->start), WithinAbs (curve.evaluate (beat - 1.0), 1e-9));

    // 整组不早于 0 拍；每个音符至少保留 minLength，终点不动
    trimNoteStarts (target, original, { a, b }, -5.0, minNoteLength);
    CHECK_THAT (findNote (target, a)->start, WithinAbs (0.0, 1e-12));
    CHECK_THAT (findNote (target, b)->start, WithinAbs (1.0, 1e-12));

    trimNoteStarts (target, original, { a, b }, 1.5, 0.25);
    CHECK_THAT (findNote (target, a)->start, WithinAbs (2.5, 1e-12));
    CHECK_THAT (findNote (target, b)->start, WithinAbs (2.75, 1e-12));
    CHECK_THAT (findNote (target, b)->getEnd(), WithinAbs (3.0, 1e-12));
}

TEST_CASE ("drag operations are relative to the original, not cumulative", "[editing]")
{
    NoteId a, b, c;
    const auto original = threeNotes (a, b, c);
    auto target = original;

    resizeNotes (target, original, { a }, 0.5, minNoteLength);
    resizeNotes (target, original, { a }, 0.5, minNoteLength);
    CHECK_THAT (findNote (target, a)->length, WithinAbs (1.5, 1e-12));

    resizeNotes (target, original, { a, b }, -10.0, 0.25);
    CHECK_THAT (findNote (target, a)->length, WithinAbs (0.25, 1e-12));
    CHECK_THAT (findNote (target, b)->length, WithinAbs (0.25, 1e-12));

    changeVelocities (target, original, { a, b }, 0.4f);
    CHECK_THAT (findNote (target, a)->velocity, WithinAbs (0.9, 1e-6));
    CHECK_THAT (findNote (target, b)->velocity, WithinAbs (1.0, 1e-6));

    changeVelocities (target, original, { c }, -1.0f);
    CHECK_THAT (findNote (target, c)->velocity, WithinAbs (0.01, 1e-6));
}

TEST_CASE ("transpose, shift, delete", "[editing]")
{
    NoteId a, b, c;
    auto p = threeNotes (a, b, c);

    transposeNotes (p, { a, c }, 12);
    CHECK (findNote (p, a)->pitch == 72);
    CHECK (findNote (p, c)->pitch == 79);
    CHECK (findNote (p, b)->pitch == 64);

    shiftNotes (p, { b }, -4.0);
    CHECK (findNote (p, b)->start == 0.0);

    deleteNotes (p, { a, c });
    REQUIRE (p.notes.size() == 1);
    CHECK (p.notes.front().id == b);
}

TEST_CASE ("copy, paste and duplicate create new ids", "[editing]")
{
    NoteId a, b, c;
    auto p = threeNotes (a, b, c);

    const auto clip = copyNotes (p, { b, c });
    REQUIRE (clip.size() == 2);
    CHECK_THAT (clip[0].start, WithinAbs (0.0, 1e-12));
    CHECK_THAT (clip[1].start, WithinAbs (1.0, 1e-12));

    const auto pasted = pasteNotes (p, clip, 8.0);
    CHECK (pasted.size() == 2);
    CHECK (p.notes.size() == 5);
    for (auto id : pasted)
    {
        CHECK (id != a);
        CHECK (id != b);
        CHECK (id != c);
        CHECK (findNote (p, id)->start >= 8.0);
    }

    // 选区 [1, 3) 长 2 拍；网格为 1 小节（4 拍）时，复制品放在 1 + 4 = 5
    const auto dup = duplicateNotes (p, { b, c }, 4.0);
    REQUIRE (dup.size() == 2);
    double earliest = 1e9;
    for (auto id : dup)
        earliest = std::min (earliest, findNote (p, id)->start);
    CHECK_THAT (earliest, WithinAbs (5.0, 1e-12));

    // 不吸附时紧接在后面
    const auto dup2 = duplicateNotes (p, { a }, 0.0);
    REQUIRE (dup2.size() == 1);
    CHECK_THAT (findNote (p, *dup2.begin())->start, WithinAbs (1.0, 1e-12));
}

TEST_CASE ("range queries and selection cleanup", "[editing]")
{
    NoteId a, b, c;
    auto p = threeNotes (a, b, c);

    CHECK (notesInRange (p, 0.5, 1.2, 0, 127) == IdSet { a, b });
    CHECK (notesInRange (p, 1.2, 0.5, 127, 0) == IdSet { a, b }); // 反向的矩形
    CHECK (notesInRange (p, 0.0, 10.0, 61, 66) == IdSet { b });
    CHECK (notesInRange (p, 1.0, 1.0, 0, 127).empty() == true);   // 端点相接不算重叠
    CHECK (allNotes (p) == IdSet { a, b, c });

    deleteNotes (p, { b });
    CHECK (existingOnly (p, { a, b }) == IdSet { a });
    CHECK (findNote (p, b) == nullptr);
}

TEST_CASE ("duplicating in place leaves copies under the originals", "[editing]")
{
    NoteId a, b, c;
    auto p = threeNotes (a, b, c);
    findNote (p, b)->curve.points = { { 0.0, 0.0 }, { 0.5, 2.0 } };

    const auto copies = duplicateInPlace (p, { b, c });
    REQUIRE (copies.size() == 2);
    CHECK (p.notes.size() == 5);

    for (auto id : copies)
    {
        const auto* copy = findNote (p, id);
        const auto* original = copy->pitch == 64 ? findNote (p, b) : findNote (p, c);
        CHECK (copy->start == original->start);
        CHECK (copy->length == original->length);
        CHECK (copy->curve == original->curve);
    }

    CHECK (canSlice (*findNote (p, a), 0.5));
    CHECK_FALSE (canSlice (*findNote (p, a), 0.0));
    CHECK_FALSE (canSlice (*findNote (p, a), 1.0));
}
