#include "engine/SketchPlayer.h"
#include "engine/VoiceAllocator.h"
#include "model/NoteEditing.h"
#include "model/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using namespace curvable;
using Catch::Matchers::WithinAbs;

namespace
{
    Vibrato makeVibrato (double depth, double rate, bool sync, double fade, bool wholeNote = true)
    {
        Vibrato v;
        v.wholeNote = wholeNote;
        v.depth = depth;
        v.rate = rate;
        v.sync = sync;
        v.fade = fade;
        return v;
    }
}

TEST_CASE ("vibrato fades in, swings by its depth and settles at the end of its run", "[vibrato]")
{
    const auto v = makeVibrato (0.5, 2.0, true, 1.0); // 每拍两个周期，渐强 1 拍

    CHECK (v.evaluate (0.0, 4.0, 0.5) == 0.0);                     // 从相位 0 开始，不会跳变
    CHECK_THAT (v.envelope (0.5, 4.0), WithinAbs (0.5, 1e-12));    // 渐强中途
    CHECK_THAT (v.evaluate (1.125, 4.0, 0.5), WithinAbs (0.5, 1e-9));  // 完全起振后，四分之一周期处到达峰值
    CHECK_THAT (v.evaluate (1.375, 4.0, 0.5), WithinAbs (-0.5, 1e-9));
    CHECK (v.envelope (4.0, 4.0) == 0.0);                          // 段尾收住
    CHECK (v.envelope (3.95, 4.0) < 0.5);
}

TEST_CASE ("vibrato rate in Hz depends on the tempo, synced rate does not", "[vibrato]")
{
    const auto hz = makeVibrato (1.0, 4.0, false, 0.0);
    CHECK_THAT (hz.beatsPerCycle (0.5), WithinAbs (0.5, 1e-12));  // 120 BPM：4 Hz = 每拍两个周期
    CHECK_THAT (hz.beatsPerCycle (1.0), WithinAbs (0.25, 1e-12)); // 60 BPM：每拍四个周期

    const auto synced = makeVibrato (1.0, 4.0, true, 0.0);
    CHECK_THAT (synced.beatsPerCycle (0.5), WithinAbs (0.25, 1e-12));
    CHECK_THAT (synced.beatsPerCycle (1.0), WithinAbs (0.25, 1e-12));

    // 切换 sync 时保持听起来的速度
    auto v = makeVibrato (1.0, 4.0, false, 0.0);
    editing::setVibratoSync (v, true, 0.5);
    CHECK (v.rate == 2.0);  // 每拍两个周期 = 1/8
    editing::setVibratoSync (v, false, 0.5);
    CHECK_THAT (v.rate, WithinAbs (4.0, 1e-9));
}

TEST_CASE ("segment vibrato follows the curve points and joins neighbouring segments", "[vibrato]")
{
    PitchCurve c;
    c.vibrato = makeVibrato (0.5, 4.0, true, 0.0, false);
    c.points = { { 0.0, 0.0 }, { 1.0, 2.0 }, { 2.0, 2.0 }, { 3.0, 0.0 } };
    CHECK_FALSE (c.hasVibrato());

    // 只给 [1, 2) 加颤音
    editing::setSegmentVibrato (c, 1, Vibrato::Shape::sine);
    CHECK (c.hasVibrato());
    CHECK (c.vibratoAt (0.5, 4.0, 0.5) == 0.0);
    CHECK (c.vibratoAt (2.5, 4.0, 0.5) == 0.0);
    CHECK (std::abs (c.vibratoAt (1.5625, 4.0, 0.5)) > 0.3);

    auto runs = c.getVibratoRuns (4.0);
    REQUIRE (runs.size() == 1);
    CHECK (runs[0].start == 1.0);
    CHECK (runs[0].end == 2.0);

    // 相邻的段连成一段；最后一个节点的段一直到音符结尾
    editing::setSegmentVibrato (c, 2, Vibrato::Shape::sine);
    editing::setSegmentVibrato (c, 3, Vibrato::Shape::sine);
    runs = c.getVibratoRuns (4.0);
    REQUIRE (runs.size() == 1);
    CHECK (runs[0].start == 1.0);
    CHECK (runs[0].end == 4.0);

    PitchCurve::Run run;
    REQUIRE (c.findVibratoRun (3.5, 4.0, run));
    CHECK (run.start == 1.0);
    CHECK_FALSE (c.findVibratoRun (0.5, 4.0, run));

    // 在颤音段里插入节点，两边都保留颤音
    editing::insertCurvePoint (c, 1.5, 2.0);
    CHECK (c.getVibratoRuns (4.0).size() == 1);
    CHECK (c.points[2].vibrato);

    // 去掉中间一段，分成两段
    editing::setSegmentVibrato (c, 2, std::nullopt);
    CHECK (c.getVibratoRuns (4.0).size() == 2);
}

TEST_CASE ("whole-note vibrato replaces segment marks and the other way round", "[vibrato]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0 } };
    editing::setSegmentVibrato (c, 0, Vibrato::Shape::sine);
    CHECK (c.hasVibrato());
    CHECK (c.points.size() == 2); // 只有颤音标记的节点不会被当成"不弯"而清掉

    editing::setWholeNoteVibrato (c, Vibrato::Shape::sine);
    CHECK (c.vibrato.wholeNote);
    CHECK_FALSE (c.hasSegmentVibrato());
    CHECK (c.points.size() == 2); // 节点保留（即使不弯）

    // 去掉颤音后音符变平，节点也保留
    editing::setWholeNoteVibrato (c, std::nullopt);
    editing::setSegmentVibrato (c, 0, Vibrato::Shape::sine);
    editing::setSegmentVibrato (c, 0, std::nullopt);
    CHECK (c.isFlat());
    CHECK (c.points.size() == 2);

    // 整个音符 → 按段：所有段都标上，再去掉这一段
    PitchCurve d;
    d.points = { { 0.0, 0.0 }, { 1.0, 1.0 }, { 2.0, 0.0 } };
    editing::setWholeNoteVibrato (d, Vibrato::Shape::sine);
    editing::setSegmentVibrato (d, 1, std::nullopt);
    CHECK_FALSE (d.vibrato.wholeNote);
    CHECK (d.points[0].vibrato);
    CHECK_FALSE (d.points[1].vibrato);
    CHECK (d.points[2].vibrato);

    editing::removeVibrato (d);
    CHECK_FALSE (d.hasVibrato());
    CHECK (d.vibrato.depth == 0.4); // 参数保留
}

TEST_CASE ("a note with only vibrato bends, gets its own voice and is saved", "[vibrato]")
{
    Project p;
    const auto plain = p.addNote (0.0, 2.0, 60, 0.8f);
    const auto shaky = p.addNote (0.0, 2.0, 64, 0.8f);
    editing::findNote (p, shaky)->curve.vibrato = makeVibrato (0.3, 6.0, false, 0.25);

    CHECK_FALSE (editing::findNote (p, shaky)->curve.isFlat());
    CHECK (editing::findNote (p, shaky)->curve.pointsFlat());
    CHECK_THAT (editing::findNote (p, shaky)->curve.getMaxAbsOffset (2.0), WithinAbs (0.3, 1e-12));

    const auto s = Schedule::compile (p);
    CHECK (s.find (plain)->voice != s.find (shaky)->voice);

    const auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (editing::findNote (*restored, shaky)->curve == editing::findNote (p, shaky)->curve);

    // 按段的颤音也能保存
    auto& curve = editing::findNote (p, shaky)->curve;
    curve.vibrato.wholeNote = false;
    curve.points = { { 0.0, 0.0 }, { 1.0, 1.0, 0.0f, makeVibrato (0.7, 3.0, true, 0.0, false) } };
    const auto restoredSegments = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    CHECK (editing::findNote (*restoredSegments, shaky)->curve == curve);
}

TEST_CASE ("vibrato produces an oscillating pitch wheel", "[vibrato][player]")
{
    Project p;
    const auto id = p.addNote (0.0, 4.0, 60, 1.0f);
    editing::findNote (p, id)->curve.vibrato = makeVibrato (1.0, 2.0, true, 0.0);
    const auto s = Schedule::compile (p);

    SketchPlayer player;
    player.setSampleRate (48000.0);
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);
    player.render (0.0, 2.0, 0.0, 0, 48000, 24000.0, out);

    int minValue = 16383, maxValue = 0, crossings = 0, previous = 8192;
    for (const auto& e : out.get())
    {
        if (! e.isPitchWheel)
            continue;

        minValue = std::min (minValue, e.wheelValue);
        maxValue = std::max (maxValue, e.wheelValue);

        if ((previous < 8192) != (e.wheelValue < 8192))
            ++crossings;

        previous = e.wheelValue;
    }

    // ±1 半音、弯音范围 ±2：大约在 4096..12288 之间摆动；2 拍 × 每拍 2 周期 = 4 个周期
    CHECK (maxValue > 12000);
    CHECK (minValue < 4400);
    CHECK (crossings >= 6);
}

//==============================================================================
TEST_CASE ("scoop slides into the existing curve and leaves the rest alone", "[shapes]")
{
    PitchCurve c;
    editing::applyShape (c, editing::Shape::scoop, 2.0, 2.0);

    REQUIRE (c.points.size() == 2);
    CHECK_THAT (c.evaluate (0.0), WithinAbs (-2.0, 1e-12));
    CHECK_THAT (c.evaluate (0.25), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.evaluate (1.5), WithinAbs (0.0, 1e-12));

    // 先慢后快的段落接上去后，scoop 的终点沿用原来的曲线值
    PitchCurve bent;
    bent.points = { { 0.0, 1.0 }, { 2.0, 1.0 } };
    editing::applyShape (bent, editing::Shape::scoop, 2.0, 2.0);
    CHECK_THAT (bent.evaluate (0.0), WithinAbs (-1.0, 1e-12));
    CHECK_THAT (bent.evaluate (0.25), WithinAbs (1.0, 1e-12));
    CHECK_THAT (bent.evaluate (1.0), WithinAbs (1.0, 1e-12));
}

TEST_CASE ("fall and doit bend only the tail and respect the bend range", "[shapes]")
{
    PitchCurve c;
    editing::applyShape (c, editing::Shape::fall, 2.0, 2.0);
    CHECK_THAT (c.evaluate (1.0), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.evaluate (1.5), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.evaluate (2.0), WithinAbs (-2.0, 1e-12)); // 被弯音范围限制

    PitchCurve wide;
    editing::applyShape (wide, editing::Shape::doit, 1.0, 12.0);
    CHECK_THAT (wide.evaluate (1.0), WithinAbs (3.0, 1e-12));
    CHECK (wide.evaluate (0.75) < 1.5); // 先慢后快
}

TEST_CASE ("head and tail shapes combine", "[shapes]")
{
    PitchCurve c;
    editing::applyShape (c, editing::Shape::scoop, 2.0, 12.0);
    editing::applyShape (c, editing::Shape::fall, 2.0, 12.0);

    CHECK_THAT (c.evaluate (0.0), WithinAbs (-2.0, 1e-12));
    CHECK_THAT (c.evaluate (1.0), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.evaluate (2.0), WithinAbs (-5.0, 1e-12));

    for (std::size_t i = 1; i < c.points.size(); ++i)
        CHECK (c.points[i].time >= c.points[i - 1].time);
}

TEST_CASE ("overshoot goes past the target and settles", "[shapes]")
{
    PitchCurve c;
    editing::applyShape (c, editing::Shape::overshoot, 2.0, 2.0);
    CHECK (c.evaluate (0.0) < 0.0);
    CHECK (c.evaluate (0.12) > 0.0);
    CHECK_THAT (c.evaluate (0.35), WithinAbs (0.0, 1e-12));
}

TEST_CASE ("bend and release returns to the note, clear removes everything", "[shapes]")
{
    PitchCurve c;
    c.vibrato = makeVibrato (0.3, 5.0, false, 0.0);
    editing::applyShape (c, editing::Shape::bendRelease, 4.0, 2.0);

    CHECK_THAT (c.evaluate (1.8), WithinAbs (2.0, 1e-12));
    CHECK_THAT (c.evaluate (3.8), WithinAbs (0.0, 1e-12));
    CHECK (c.vibrato.wholeNote); // 颤音不受影响

    editing::applyShape (c, editing::Shape::clear, 4.0, 2.0);
    CHECK (c.points.empty());
    CHECK_FALSE (c.hasVibrato());
}

TEST_CASE ("a drawn depth envelope shapes the vibrato and matches the old fade-in until edited", "[vibrato]")
{
    constexpr double runLength = 4.0;
    auto v = makeVibrato (0.5, 2.0, true, 1.0);

    // 没画过：由 fade 换算出来的点，求值与原来的渐强完全一致
    const auto implied = v.getEnvelope (runLength);
    REQUIRE (implied.size() == 3);
    CHECK_THAT (implied[1].x, WithinAbs (0.25, 1e-12));

    auto drawn = v;
    drawn.setEnvelope (implied);
    REQUIRE (drawn.hasDrawnEnvelope());
    for (double x = 0.0; x <= runLength; x += 0.07)
        CHECK_THAT (drawn.envelope (x, runLength), WithinAbs (v.envelope (x, runLength), 1e-9));

    // 加一个点，拖到 0.25 半音：那里的深度就是 0.25，整段的深度不变
    const auto index = editing::insertEnvelopePoint (drawn, runLength, 0.75);
    REQUIRE (index == 2);
    editing::moveEnvelopePoint (drawn, runLength, index, 0.75, 0.25);
    CHECK_THAT (drawn.depth, WithinAbs (0.5, 1e-12));
    CHECK_THAT (drawn.envelope (3.0, runLength) * drawn.depth, WithinAbs (0.25, 1e-9));

    // 拖得比整段还深：整段加深，其他点的实际深度不变
    editing::moveEnvelopePoint (drawn, runLength, index, 0.75, 1.0);
    CHECK_THAT (drawn.depth, WithinAbs (1.0, 1e-12));
    CHECK_THAT (drawn.envelope (3.0, runLength) * drawn.depth, WithinAbs (1.0, 1e-9));
    CHECK_THAT (drawn.envelope (1.0, runLength) * drawn.depth, WithinAbs (0.5, 1e-9));

    // 首尾的点不能删、不能左右移
    CHECK_FALSE (editing::removeEnvelopePoint (drawn, runLength, 0));
    editing::moveEnvelopePoint (drawn, runLength, 0, 0.5, 0.0);
    CHECK (drawn.getEnvelope (runLength).front().x == 0.0);
    CHECK (editing::removeEnvelopePoint (drawn, runLength, index));
    CHECK (drawn.getEnvelope (runLength).size() == 3);

    // 保存后读回来，包络不变（音频线程用的固定数组，用不到的位置保持默认值）
    Project p;
    const auto id = p.addNote (0.0, runLength, 60, 0.8f);
    drawn.wholeNote = true;
    editing::findNote (p, id)->curve.vibrato = drawn;
    const auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (editing::findNote (*restored, id)->curve.vibrato == drawn);

    // trill 没有深度包络
    auto trill = drawn;
    trill.setShape (Vibrato::Shape::trill);
    CHECK (trill.getEnvelope (runLength).empty());
    CHECK_FALSE (trill.hasDrawnEnvelope());
}

TEST_CASE ("pulling up vibrato adds a fresh run, and a silent run is still a run until removed", "[vibrato]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0 }, { 2.0, 0.0 } };

    // 相邻段已有颤音（画过包络）：新的一段沿用速度，深度和包络从头开始，不会改到相邻的段
    editing::addVibrato (c, 0, 0.6);
    auto& first = *c.points[0].vibrato;
    first.rate = 7.0;
    first.setEnvelope ({ { 0.0, 1.0 }, { 0.5, 0.2 }, { 1.0, 1.0 } });

    editing::addVibrato (c, 1, 0.0);
    REQUIRE (c.points[1].vibrato.has_value());
    CHECK (c.points[1].vibrato->rate == 7.0);
    CHECK (c.points[1].vibrato->depth == 0.0);
    CHECK_FALSE (c.points[1].vibrato->hasDrawnEnvelope());
    CHECK (c.points[0].vibrato->depth == 0.6);

    // 深度为 0 的段：没有声音，但仍然是一段（可以编辑）
    const auto runs = c.getVibratoRuns (3.0);
    REQUIRE (runs.size() == 2);
    CHECK (runs[1].firstPoint == 1);
    CHECK (c.vibratoAt (1.5, 3.0, 0.5) == 0.0);

    editing::removeSilentVibrato (c);
    CHECK_FALSE (c.points[1].vibrato.has_value());
    CHECK (c.points[0].vibrato.has_value());

    // 整个音符
    PitchCurve whole;
    editing::addVibrato (whole, -1, 0.0);
    CHECK (whole.vibrato.wholeNote);
    CHECK (whole.getVibratoRuns (2.0).size() == 1);
    CHECK_FALSE (whole.hasVibrato());
    editing::removeSilentVibrato (whole);
    CHECK_FALSE (whole.vibrato.wholeNote);

    // 包络插值：点之间直线连接，两端之外保持端点的值
    const std::vector<Vibrato::EnvelopePoint> pts { { 0.0, 0.0 }, { 0.5, 1.0 }, { 1.0, 0.5 } };
    CHECK_THAT (Vibrato::levelAt (pts, 0.25), WithinAbs (0.5, 1e-12));
    CHECK_THAT (Vibrato::levelAt (pts, 0.75), WithinAbs (0.75, 1e-12));
    CHECK (Vibrato::levelAt (pts, 1.5) == 0.5);
    CHECK (Vibrato::levelAt ({}, 0.3) == 1.0);
}
