#include "model/CurvePresetLibrary.h"
#include "model/NoteEditing.h"
#include "model/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using namespace curvable;
using namespace curvable::editing;
using Catch::Matchers::WithinAbs;

namespace
{
    PitchCurve wholeNoteTrill (double interval)
    {
        PitchCurve c;
        c.vibrato.setShape (Vibrato::Shape::trill);
        c.vibrato.wholeNote = true;
        c.vibrato.depth = interval;
        c.normalise();
        return c;
    }
}

//==============================================================================
TEST_CASE ("trill alternates between the note and its neighbour", "[trill]")
{
    const auto c = wholeNoteTrill (2.0);
    const double spb = 0.5;

    // 默认：每拍 4 个来回，一个周期 1/4 拍：前半在本音，后半在上方全音
    CHECK (c.vibrato.sync);
    CHECK_THAT (c.vibrato.beatsPerCycle (spb), WithinAbs (0.25, 1e-12));
    CHECK_THAT (c.vibratoAt (0.05, 4.0, spb), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.vibratoAt (0.20, 4.0, spb), WithinAbs (2.0, 1e-12));
    CHECK_THAT (c.vibratoAt (0.30, 4.0, spb), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.vibratoAt (2.45, 4.0, spb), WithinAbs (2.0, 1e-12));

    // 没有渐强：第一次切换就是完整的音程（颤音的 fade 设置不影响 trill）
    CHECK (c.vibrato.fade > 0.0);
    CHECK_THAT (c.vibratoAt (0.18, 4.0, spb), WithinAbs (2.0, 1e-12));

    // 下方的音：音程为负
    const auto lower = wholeNoteTrill (-1.0);
    CHECK_THAT (lower.vibratoAt (0.20, 4.0, spb), WithinAbs (-1.0, 1e-12));
    CHECK_THAT (lower.getMaxAbsOffset (4.0), WithinAbs (1.0, 1e-12));
}

TEST_CASE ("trill switches smoothly and ends back on the note", "[trill]")
{
    const auto c = wholeNoteTrill (2.0);
    const double spb = 0.5, length = 2.0;

    // 采样间隔很小时，相邻值之差有限（没有瞬间跳变）
    double previous = c.vibratoAt (0.0, length, spb);
    double worst = 0.0;

    for (double t = 0.0; t <= length; t += 1.0 / 4096.0)
    {
        const auto v = c.vibratoAt (t, length, spb);
        worst = std::max (worst, std::abs (v - previous));
        previous = v;
    }

    CHECK (worst < 0.2);
    CHECK (worst > 0.0);
    CHECK_THAT (c.vibratoAt (length - 1.0e-6, length, spb), WithinAbs (0.0, 1e-3));

    for (double phase = 0.0; phase < 1.0; phase += 0.01)
    {
        CHECK (Vibrato::trillWave (phase) >= 0.0);
        CHECK (Vibrato::trillWave (phase) <= 1.0);
    }
}

TEST_CASE ("switching shape resets the settings to that shape's defaults", "[trill]")
{
    Vibrato v;
    v.depth = 1.3;
    v.fade = 0.25;

    v.setShape (Vibrato::Shape::trill);
    CHECK (v.depth == 2.0);
    CHECK (v.sync);
    CHECK (v.rate == 4.0);
    CHECK (v.fade == 0.25); // 渐强保留（切回颤音时沿用）

    v.depth = -3.0;
    v.normalise();
    CHECK (v.depth == -3.0); // trill 允许负的音程

    v.setShape (Vibrato::Shape::sine);
    CHECK (v.depth == Vibrato {}.depth);
    CHECK_FALSE (v.sync);

    v.depth = -0.5;
    v.normalise();
    CHECK (v.depth == 0.5); // 颤音深度没有正负
}

TEST_CASE ("trill on segments only plays there", "[trill]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0 }, { 2.0, 0.0 } };
    setSegmentVibrato (c, 1, Vibrato::Shape::trill);

    CHECK (c.hasVibrato());
    CHECK_THAT (c.vibratoAt (0.7, 3.0, 0.5), WithinAbs (0.0, 1e-12));
    CHECK_THAT (c.vibratoAt (1.2, 3.0, 0.5), WithinAbs (2.0, 1e-12));
    CHECK_THAT (c.vibratoAt (2.2, 3.0, 0.5), WithinAbs (0.0, 1e-12));
}

TEST_CASE ("trill settings survive saving", "[trill][serializer]")
{
    Project p;
    const auto id = p.addNote (0.0, 2.0, 60, 0.8f);
    auto& c = findNote (p, id)->curve;
    c = wholeNoteTrill (-2.0);
    c.vibrato.rate = 6.0;

    const auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (restored->notes == p.notes);
    CHECK (findNote (*restored, id)->curve.vibrato.shape == Vibrato::Shape::trill);

    // 旧工程（没有 shape 属性）读出来是普通颤音
    PitchCurve sine;
    sine.vibrato.wholeNote = true;
    juce::ValueTree parent ("Note");
    ProjectSerializer::writeCurve (sine, parent);
    CHECK_FALSE (parent.getChildWithName ("Vibrato").hasProperty ("shape"));
    CHECK (ProjectSerializer::readCurve (parent).vibrato.shape == Vibrato::Shape::sine);
}

//==============================================================================
TEST_CASE ("presets stretch to the note length but keep the vibrato speed", "[presets]")
{
    PitchCurve preset;
    Vibrato v;
    v.depth = 0.6;
    v.rate = 6.0;
    preset.points = { { 0.0, -2.0, 0.5f }, { 0.5, 0.0 }, { 1.5, 0.0, 0.0f, v } };

    PitchCurve target;
    target.points = { { 0.0, 5.0 } };

    applyPreset (target, preset, 2.0, 4.0);

    REQUIRE (target.points.size() == 3);
    CHECK (target.points[1].time == 1.0);
    CHECK (target.points[2].time == 3.0);
    CHECK (target.points[0].tension == 0.5f);
    REQUIRE (target.points[2].vibrato);
    CHECK (target.points[2].vibrato->rate == 6.0);
    CHECK_THAT (target.evaluate (0.0), WithinAbs (-2.0, 1e-12));
    CHECK_THAT (target.evaluate (1.0), WithinAbs (preset.evaluate (0.5), 1e-12));

    // 长度无效时原样套用
    PitchCurve same;
    applyPreset (same, preset, 0.0, 4.0);
    CHECK (same.points == preset.points);
}

TEST_CASE ("the preset library saves, replaces, lists and deletes", "[presets]")
{
    const juce::TemporaryFile temp ("xml");
    const CurvePresetLibrary library (temp.getFile());

    CHECK (library.load().empty()); // 文件还不存在

    CurvePreset scoop { "Scoop up", {}, 1.0 };
    scoop.curve.points = { { 0.0, -2.0, -0.4f }, { 0.25, 0.0 } };

    CurvePreset trill { "Trill", wholeNoteTrill (1.0), 2.0 };

    REQUIRE (library.save (trill));
    REQUIRE (library.save (scoop));
    CHECK_FALSE (library.save ({ "   ", {}, 1.0 }));     // 名称不能为空
    CHECK_FALSE (library.save ({ "Bad", {}, 0.0 }));     // 长度必须为正

    auto presets = library.load();
    REQUIRE (presets.size() == 2);
    CHECK (presets[0].name == "Scoop up"); // 按名称排序
    CHECK (presets[0].curve == scoop.curve);
    CHECK (presets[1].curve == trill.curve);
    CHECK (presets[1].length == 2.0);
    CHECK (library.contains ("Trill"));

    // 同名替换
    scoop.length = 3.0;
    REQUIRE (library.save (scoop));
    presets = library.load();
    REQUIRE (presets.size() == 2);
    CHECK (presets[0].length == 3.0);

    CHECK (library.remove ("Trill"));
    CHECK_FALSE (library.remove ("Trill"));
    REQUIRE (library.load().size() == 1);

    // 损坏的文件：当作空库，不崩溃
    REQUIRE (temp.getFile().replaceWithText ("<CurvePresets><Preset"));
    CHECK (library.load().empty());
}

TEST_CASE ("trill speed snaps to the length of each alternation", "[trill]")
{
    Vibrato v;
    CHECK (snapSyncRate (v, 3.9) == 4.0);

    v.setShape (Vibrato::Shape::trill);
    CHECK (juce::String (syncRateName (v)) == "1/32"); // 每拍 4 个来回 = 每个三十二分音符换一次
    CHECK (snapSyncRate (v, 1.0) == 1.0);               // 每拍 2 次交替 = 1/8
    CHECK (snapSyncRate (v, 1.4) == 1.5);               // 1/8T

    // 关掉再打开跟随曲速：听起来的速度不变（120 BPM）
    setVibratoSync (v, false, 0.5);
    CHECK (v.rate == 8.0); // Hz
    setVibratoSync (v, true, 0.5);
    CHECK (v.rate == 4.0);
}

//==============================================================================
TEST_CASE ("each segment keeps its own vibrato or trill settings", "[vibrato][segments]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0 }, { 2.0, 0.0 }, { 3.0, 0.0 } };

    setSegmentVibrato (c, 0, Vibrato::Shape::sine);
    setSegmentVibrato (c, 2, Vibrato::Shape::trill);
    CHECK (c.hasVibratoShape (Vibrato::Shape::sine));
    CHECK (c.hasVibratoShape (Vibrato::Shape::trill));

    auto runs = c.getVibratoRuns (4.0);
    REQUIRE (runs.size() == 2);
    CHECK (runs[0].settings.shape == Vibrato::Shape::sine);
    CHECK (runs[1].settings.shape == Vibrato::Shape::trill);
    CHECK (runs[1].firstPoint == 2);

    // 改其中一段，另一段不受影响
    auto deeper = runs[0].settings;
    deeper.depth = 1.5;
    setRunVibrato (c, runs[0], deeper);
    CHECK (c.points[0].vibrato->depth == 1.5);
    CHECK (c.points[2].vibrato->depth == 2.0);
    CHECK_THAT (c.getMaxAbsOffset (4.0), WithinAbs (2.0, 1e-12));

    // 相邻的段：加上时沿用旁边那一段的参数，于是连成一段（相位连续）
    setSegmentVibrato (c, 1, Vibrato::Shape::sine);
    CHECK (c.points[1].vibrato->depth == 1.5);
    runs = c.getVibratoRuns (4.0);
    REQUIRE (runs.size() == 2);
    CHECK (runs[0].start == 0.0);
    CHECK (runs[0].end == 2.0);

    // 参数不同的相邻段是两段，各自起振（交界处不跳变：前一段收住、后一段从 0 开始）
    auto other = *c.points[1].vibrato;
    other.rate = 3.0;
    c.points[1].vibrato = other;
    runs = c.getVibratoRuns (4.0);
    REQUIRE (runs.size() == 3);
    CHECK (std::abs (c.vibratoAt (1.0, 4.0, 0.5)) < 1e-9);
    CHECK (std::abs (c.vibratoAt (1.0 - 1e-9, 4.0, 0.5)) < 1e-3);

    // 把一段换成另一种形状只影响这一段
    setSegmentVibrato (c, 0, Vibrato::Shape::trill);
    CHECK (c.points[0].vibrato->shape == Vibrato::Shape::trill);
    CHECK (c.points[1].vibrato->shape == Vibrato::Shape::sine);
    CHECK (c.points[0].vibrato->depth == 2.0); // 沿用这个音符上已有的 trill
}

TEST_CASE ("whole-note vibrato and segments convert into each other with their settings", "[vibrato][segments]")
{
    PitchCurve c;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0 } };
    setSegmentVibrato (c, 1, Vibrato::Shape::trill);
    c.points[1].vibrato->depth = -1.0;

    // 整个音符加 trill：沿用这个音符上已有的 trill 参数，各段的颤音去掉
    setWholeNoteVibrato (c, Vibrato::Shape::trill);
    CHECK (c.vibrato.wholeNote);
    CHECK (c.vibrato.depth == -1.0);
    CHECK_FALSE (c.hasSegmentVibrato());

    // 再改成按段：每一段带上整个音符的参数
    setSegmentVibrato (c, 0, std::nullopt);
    CHECK_FALSE (c.vibrato.wholeNote);
    CHECK_FALSE (c.points[0].vibrato);
    REQUIRE (c.points[1].vibrato);
    CHECK (c.points[1].vibrato->depth == -1.0);
    CHECK_FALSE (c.points[1].vibrato->wholeNote);

    // 整个音符的颤音：改参数用 setRunVibrato，同步速度用 forEachVibrato
    setWholeNoteVibrato (c, Vibrato::Shape::sine);
    auto runs = c.getVibratoRuns (2.0);
    REQUIRE (runs.size() == 1);
    CHECK (runs[0].isWholeNote());
    auto v = runs[0].settings;
    v.depth = 0.9;
    setRunVibrato (c, runs[0], v);
    CHECK (c.vibrato.wholeNote);
    CHECK (c.vibrato.depth == 0.9);

    forEachVibrato (c, [] (Vibrato& x) { setVibratoSync (x, true, 0.5); });
    CHECK (c.vibrato.sync);
}

TEST_CASE ("per-segment settings are saved, and older saves still load", "[vibrato][segments][serializer]")
{
    Project p;
    const auto id = p.addNote (0.0, 4.0, 60, 0.8f);
    auto& c = findNote (p, id)->curve;
    c.points = { { 0.0, 0.0 }, { 1.0, 0.0 }, { 2.0, 1.0 } };
    setSegmentVibrato (c, 0, Vibrato::Shape::sine);
    setSegmentVibrato (c, 2, Vibrato::Shape::trill);
    c.points[0].vibrato->depth = 0.8;
    c.points[2].vibrato->rate = 2.0;

    const auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (findNote (*restored, id)->curve == c);

    // 早期格式：节点上只有 vib 标记，参数在音符的 Vibrato 里
    juce::ValueTree note ("Note");
    juce::ValueTree a ("Point"), b ("Point");
    a.setProperty ("t", 0.0, nullptr);
    a.setProperty ("st", 0.0, nullptr);
    a.setProperty ("vib", true, nullptr);
    b.setProperty ("t", 1.0, nullptr);
    b.setProperty ("st", 2.0, nullptr);
    juce::ValueTree vib ("Vibrato");
    vib.setProperty ("depth", 0.7, nullptr);
    vib.setProperty ("rate", 4.0, nullptr);
    note.appendChild (a, nullptr);
    note.appendChild (b, nullptr);
    note.appendChild (vib, nullptr);

    const auto legacy = ProjectSerializer::readCurve (note);
    REQUIRE (legacy.points.size() == 2);
    REQUIRE (legacy.points[0].vibrato);
    CHECK (legacy.points[0].vibrato->depth == 0.7);
    CHECK (legacy.points[0].vibrato->rate == 4.0);
    CHECK_FALSE (legacy.points[1].vibrato);
}

TEST_CASE ("gliding keeps each note's own vibrato settings", "[vibrato][segments][glide]")
{
    Project p;
    const auto a = p.addNote (0.0, 1.0, 60, 0.8f);
    const auto b = p.addNote (1.0, 1.0, 62, 0.8f);
    setWholeNoteVibrato (findNote (p, a)->curve, Vibrato::Shape::sine);
    setWholeNoteVibrato (findNote (p, b)->curve, Vibrato::Shape::trill);

    REQUIRE (glideNotes (p, { a, b }).error == GlideResult::Error::none);
    const auto runs = p.notes.front().curve.getVibratoRuns (2.0);
    REQUIRE (runs.size() == 2);
    CHECK (runs[0].settings.shape == Vibrato::Shape::sine);
    CHECK (runs[1].settings.shape == Vibrato::Shape::trill);
}
