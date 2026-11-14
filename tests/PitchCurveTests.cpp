#include "engine/PitchBend.h"
#include "engine/SketchPlayer.h"
#include "model/NoteEditing.h"
#include "model/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <limits>

using namespace curvable;
using Catch::Matchers::WithinAbs;

TEST_CASE ("curve evaluation holds outside the points and interpolates between them", "[curve]")
{
    PitchCurve c;
    CHECK (c.isFlat());
    CHECK (c.evaluate (1.0) == 0.0);

    c.points = { { 0.5, -2.0 }, { 1.0, 0.0 }, { 2.0, 3.0 } };
    CHECK_FALSE (c.isFlat());
    CHECK_THAT (c.evaluate (0.0), WithinAbs (-2.0, 1e-12));   // 第一个节点之前
    CHECK_THAT (c.evaluate (0.75), WithinAbs (-1.0, 1e-12));  // 直线中点
    CHECK_THAT (c.evaluate (1.5), WithinAbs (1.5, 1e-12));
    CHECK_THAT (c.evaluate (9.0), WithinAbs (3.0, 1e-12));    // 最后一个节点之后保持

    CHECK_THAT (c.getMaxAbsOffset (1.5), WithinAbs (2.0, 1e-12));
    CHECK_THAT (c.getMaxAbsOffset (4.0), WithinAbs (3.0, 1e-12));
}

TEST_CASE ("tension bends the segment but keeps its end points", "[curve]")
{
    for (float t : { -1.0f, -0.4f, 0.0f, 0.3f, 1.0f })
    {
        CHECK_THAT (PitchCurve::shape (0.0, t), WithinAbs (0.0, 1e-12));
        CHECK_THAT (PitchCurve::shape (1.0, t), WithinAbs (1.0, 1e-12));
    }

    CHECK (PitchCurve::shape (0.5, 0.5f) < 0.5);  // 先慢后快
    CHECK (PitchCurve::shape (0.5, -0.5f) > 0.5); // 先快后慢

    // 单调
    double previous = 0.0;
    for (int i = 1; i <= 100; ++i)
    {
        const auto v = PitchCurve::shape (i / 100.0, 0.9f);
        CHECK (v >= previous);
        previous = v;
    }
}

TEST_CASE ("normalise sorts, drops invalid values and clamps tension", "[curve]")
{
    PitchCurve c;
    c.points = { { 1.0, 1.0, 5.0f }, { -1.0, 2.0 }, { std::numeric_limits<double>::quiet_NaN(), 0.0 }, { 0.5, 0.0 } };
    c.normalise();

    REQUIRE (c.points.size() == 3);
    CHECK (c.points[0].time == 0.0);
    CHECK (c.points[1].time == 0.5);
    CHECK (c.points[2].time == 1.0);
    CHECK (c.points[2].tension == 1.0f);
}

TEST_CASE ("inserting the first point adds an anchor at the note start", "[curve][editing]")
{
    PitchCurve c;
    CHECK (editing::insertCurvePoint (c, 1.0, 2.0) == 1);
    REQUIRE (c.points.size() == 2);
    CHECK (c.points[0] == CurvePoint { 0.0, 0.0, 0.0f });

    // 起点上的第一个节点（例如滑入）不补锚点
    PitchCurve scoop;
    CHECK (editing::insertCurvePoint (scoop, 0.0, -3.0) == 0);
    CHECK (scoop.points.size() == 1);

    // 插在中间，沿用这一段的弯曲程度
    c.points[0].tension = 0.5f;
    CHECK (editing::insertCurvePoint (c, 0.5, 1.0) == 1);
    CHECK (c.points[1].tension == 0.5f);
}

TEST_CASE ("moving a point keeps the order and stays inside the note", "[curve][editing]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 2.0 }, { 2.0, 0.0 } };

    editing::moveCurvePoint (c, 1, 5.0, 3.0, 4.0);
    CHECK (c.points[1].time == 2.0); // 不越过下一个节点
    CHECK (c.points[1].semitones == 3.0);

    editing::moveCurvePoint (c, 2, 9.0, 0.0, 3.0);
    CHECK (c.points[2].time == 3.0); // 不超过音符长度

    editing::moveCurvePoint (c, 0, -1.0, -1.0, 3.0);
    CHECK (c.points[0].time == 0.0);
}

TEST_CASE ("removing a point keeps the others even when the note no longer bends", "[curve][editing]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 2.0 }, { 2.0, 0.0 } };

    editing::removeCurvePoint (c, 1);
    REQUIRE (c.points.size() == 2);
    CHECK (c.pointsFlat());
    CHECK (c.isFlat());

    editing::removeCurvePoint (c, 0);
    CHECK (c.points.size() == 1);
}

TEST_CASE ("curves and bend range survive serialisation", "[curve][serializer]")
{
    Project p;
    const auto id = p.addNote (0.0, 2.0, 60, 0.8f);
    editing::findNote (p, id)->curve.points = { { 0.0, -1.5, 0.25f }, { 1.0, 0.0 } };
    p.bendRange = 12.0;

    const auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (restored->bendRange == 12.0);
    REQUIRE (restored->notes.size() == 1);
    CHECK (restored->notes[0].curve == p.notes[0].curve);
}

//==============================================================================
namespace
{
    constexpr double spb = 3200.0; // 每拍 3200 个采样 = 100 个弯音间隔

    std::vector<MidiNoteEvent> wheelsOf (const MidiEventList& out)
    {
        std::vector<MidiNoteEvent> result;
        for (const auto& e : out.get())
            if (e.isPitchWheel)
                result.push_back (e);
        return result;
    }
}

TEST_CASE ("a scoop sets the wheel before the note starts and glides to the centre", "[curve][player]")
{
    Project p;
    const auto id = p.addNote (1.0, 2.0, 60, 1.0f);
    editing::findNote (p, id)->curve.points = { { 0.0, -2.0 }, { 1.0, 0.0 } };
    const auto s = Schedule::compile (p);

    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);
    player.render (0.0, 4.0, 0.0, 0, (int) (4 * spb), spb, out);

    const auto& e = out.get();
    REQUIRE (e.size() >= 3);

    // 第一个事件就是弯音（-2 半音 = 范围最低端），与 note on 在同一个采样上、排在它前面
    CHECK (e[0] == MidiNoteEvent::pitchWheel ((int) spb, 0));
    CHECK (e[1] == MidiNoteEvent { (int) spb, true, 60, 127 });

    const auto wheels = wheelsOf (out);
    CHECK (wheels.back().wheelValue == 8192);                       // 滑回原音高
    CHECK (wheels.back().sampleOffset <= (int) (2 * spb));          // 在第二拍结束前到达

    for (std::size_t i = 1; i < wheels.size(); ++i)
    {
        CHECK (wheels[i].wheelValue > wheels[i - 1].wheelValue);     // 单调上升
        CHECK (wheels[i].sampleOffset % SketchPlayer::bendInterval == 0);
    }
}

TEST_CASE ("the wheel holds after the note ends and resets when playback stops", "[curve][player]")
{
    Project p;
    const auto id = p.addNote (0.0, 1.0, 60, 1.0f);
    editing::findNote (p, id)->curve.points = { { 0.0, 0.0 }, { 1.0, -1.0 } }; // 音尾下坠
    const auto s = Schedule::compile (p);

    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);
    player.render (0.0, 3.0, 0.0, 0, (int) (3 * spb), spb, out);

    auto wheels = wheelsOf (out);
    REQUIRE_FALSE (wheels.empty());
    CHECK (wheels.back().sampleOffset < (int) spb);   // 音符结束后不再发送
    CHECK (wheels.back().wheelValue < 8192);           // 停在下坠后的值（释放尾音）

    out.clear();
    player.stopAll (0, out);
    wheels = wheelsOf (out);
    REQUIRE (wheels.size() == 1);
    CHECK (wheels[0].wheelValue == 8192);
}

TEST_CASE ("flat notes never send pitch wheel events", "[curve][player]")
{
    Project p;
    p.addNote (0.0, 1.0, 60, 1.0f);
    p.addNote (1.0, 1.0, 64, 1.0f);
    const auto s = Schedule::compile (p);

    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);
    player.render (0.0, 3.0, 0.0, 0, (int) (3 * spb), spb, out);
    player.stopAll (0, out);

    CHECK (wheelsOf (out).empty());
}

TEST_CASE ("a new flat note takes the wheel back to the centre", "[curve][player]")
{
    Project p;
    const auto bent = p.addNote (0.0, 1.0, 60, 1.0f);
    editing::findNote (p, bent)->curve.points = { { 0.0, 1.0 } };
    p.addNote (2.0, 1.0, 62, 1.0f);
    const auto s = Schedule::compile (p);

    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);
    player.render (0.0, 4.0, 0.0, 0, (int) (4 * spb), spb, out);

    const auto& e = out.get();
    const auto it = std::find (e.begin(), e.end(), MidiNoteEvent { (int) (2 * spb), true, 62, 127 });
    REQUIRE (it != e.end());
    REQUIRE (it != e.begin());
    CHECK (*(it - 1) == MidiNoteEvent::pitchWheel ((int) (2 * spb), 8192));
}

TEST_CASE ("starting in the middle of a bent note chases its current bend", "[curve][player]")
{
    Project p;
    p.bendRange = 12.0;
    const auto id = p.addNote (0.0, 4.0, 60, 1.0f);
    editing::findNote (p, id)->curve.points = { { 0.0, 0.0 }, { 2.0, 12.0 } };
    const auto s = Schedule::compile (p);

    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 1.0, 0, out);
    player.start (1.0, 0, out);

    REQUIRE (out.get().size() == 2);
    CHECK (out.get()[0] == MidiNoteEvent::pitchWheel (0, centsToPitchBend (600.0, 12.0).value));
    CHECK (out.get()[1].noteOn);
}

TEST_CASE ("copying, pasting and moving notes keeps their curves", "[curve][editing]")
{
    Project p;
    const auto id = p.addNote (1.0, 1.0, 60, 0.8f);
    editing::findNote (p, id)->curve.points = { { 0.0, -2.0 }, { 0.5, 0.0 } };
    const auto curve = editing::findNote (p, id)->curve;

    const auto pasted = editing::pasteNotes (p, editing::copyNotes (p, { id }), 4.0);
    REQUIRE (pasted.size() == 1);
    CHECK (editing::findNote (p, *pasted.begin())->curve == curve);

    editing::transposeNotes (p, { id }, 5);
    editing::shiftNotes (p, { id }, 2.0);
    CHECK (editing::findNote (p, id)->curve == curve); // 曲线是相对音符的，移动后不变
}
