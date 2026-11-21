#include "engine/Schedule.h"
#include "engine/VoiceAllocator.h"
#include "model/NoteEditing.h"
#include "model/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace curvable;
using namespace curvable::editing;
using Catch::Matchers::WithinAbs;

namespace
{
    Envelope fadeOut (double length) { Envelope e; e.points = { { 0.0, 0.0 }, { length, envelope::volumeMin } }; return e; }
}

TEST_CASE ("volume converts to gain, with silence at the bottom of the range", "[envelope]")
{
    CHECK (envelope::gainFromDecibels (0.0) == 1.0f);
    CHECK_THAT (envelope::gainFromDecibels (-6.0), WithinAbs (0.501, 1e-3));
    CHECK (envelope::gainFromDecibels (envelope::volumeMin) == 0.0f);
    CHECK (envelope::gainFromDecibels (-100.0) == 0.0f);
    CHECK_THAT (envelope::gainFromDecibels (100.0), WithinAbs (envelope::gainFromDecibels (envelope::volumeMax), 1e-6));

    Envelope pan;
    pan.points = { { 0.0, -3.0 }, { 1.0, 0.5 } };
    pan.points[0].vibrato = Vibrato {};
    envelope::normalisePan (pan);
    CHECK (pan.points[0].semitones == -1.0);
    CHECK_FALSE (pan.points[0].vibrato.has_value());
}

TEST_CASE ("notes with different volume or pan need their own synth copy", "[envelope][voices]")
{
    Project p;
    const auto a = p.addNote (0.0, 2.0, 60, 0.8f);
    const auto b = p.addNote (0.0, 2.0, 64, 0.8f);
    const auto c = p.addNote (0.0, 2.0, 67, 0.8f);

    // 都不变：共用一个声部
    CHECK (Schedule::compile (p).voiceCount == 1);

    // 一个音符淡出：它要自己的声部；另外两个仍然共用
    findNote (p, a)->volume = fadeOut (2.0);
    auto s = Schedule::compile (p);
    CHECK (s.voiceCount == 2);
    CHECK (s.find (a)->voice != s.find (b)->voice);
    CHECK (s.find (b)->voice == s.find (c)->voice);

    // 同时开始、包络相同（整个和弦一起淡出）：又可以共用
    findNote (p, b)->volume = fadeOut (2.0);
    findNote (p, c)->volume = fadeOut (2.0);
    CHECK (Schedule::compile (p).voiceCount == 1);

    // 声像不同也要分开
    findNote (p, c)->pan.points = { { 0.0, 1.0 } };
    s = Schedule::compile (p);
    CHECK (s.find (c)->voice != s.find (a)->voice);
}

TEST_CASE ("trimming, slicing and gliding keep envelopes where they were in time", "[envelope][editing]")
{
    Project original;
    const auto id = original.addNote (1.0, 2.0, 60, 0.8f);
    findNote (original, id)->volume = fadeOut (2.0);
    const auto before = findNote (original, id)->volume;

    // 拖左端缩短：剩下部分的音量不变
    auto trimmed = original;
    trimNoteStarts (trimmed, original, { id }, 0.5, minNoteLength);
    const auto* t = findNote (trimmed, id);
    CHECK_THAT (envelope::volumeAt (t->volume, 1.0), WithinAbs (envelope::volumeAt (before, 1.5), 1e-9));

    // 切开：两半在每个时刻的音量都不变；没有声像包络的仍然没有
    auto sliced = original;
    const auto right = *sliceNotes (sliced, { id }, 2.0).begin();
    CHECK_THAT (envelope::volumeAt (findNote (sliced, id)->volume, 0.5), WithinAbs (envelope::volumeAt (before, 0.5), 1e-9));
    CHECK_THAT (envelope::volumeAt (findNote (sliced, right)->volume, 0.5), WithinAbs (envelope::volumeAt (before, 1.5), 1e-9));
    CHECK (findNote (sliced, right)->pan.points.empty());

    // 滑音合并：前一个音符的淡出留在原处，后一个（没有包络）从它的起点回到 0 dB
    Project chain;
    const auto first = chain.addNote (0.0, 1.0, 60, 0.8f);
    const auto second = chain.addNote (1.0, 1.0, 62, 0.8f);
    findNote (chain, first)->volume = fadeOut (1.0);
    REQUIRE (glideNotes (chain, { first, second }).error == GlideResult::Error::none);
    const auto& merged = findNote (chain, first)->volume;
    CHECK_THAT (envelope::volumeAt (merged, 0.5), WithinAbs (envelope::volumeAt (fadeOut (1.0), 0.5), 1e-9));
    CHECK_THAT (envelope::volumeAt (merged, 1.5), WithinAbs (0.0, 1e-9));
    CHECK (findNote (chain, first)->pan.points.empty());
}

TEST_CASE ("volume moves evenly by fader position between points", "[envelope]")
{
    // 0 dB 到静音的直线：中间是推子位置的一半（-12 dB 左右），不是按分贝的 -24 dB
    const auto e = fadeOut (2.0);
    CHECK_THAT (envelope::volumeAt (e, 1.0), WithinAbs (20.0 * std::log10 (0.25), 1e-6));
    CHECK (envelope::volumeAt (e, 0.0) == 0.0);
    CHECK (envelope::volumeAt (e, 2.0) == envelope::volumeMin);
    CHECK (envelope::volumeAt ({}, 1.0) == 0.0);

    for (const auto db : { envelope::volumeMin, -20.0, 0.0, envelope::volumeMax })
        CHECK_THAT (envelope::decibelsFromFader (envelope::faderFromDecibels (db)), WithinAbs (db, 1e-9));

    // 推子位置随时间均匀变化
    const auto f0 = envelope::faderFromDecibels (envelope::volumeAt (e, 0.5));
    const auto f1 = envelope::faderFromDecibels (envelope::volumeAt (e, 1.0));
    const auto f2 = envelope::faderFromDecibels (envelope::volumeAt (e, 1.5));
    CHECK_THAT (f0 - f1, WithinAbs (f1 - f2, 1e-9));
}

TEST_CASE ("envelopes are saved with the project", "[envelope][serializer]")
{
    Project p;
    const auto id = p.addNote (0.0, 2.0, 60, 0.8f);
    findNote (p, id)->volume = fadeOut (2.0);
    findNote (p, id)->pan.points = { { 0.0, -0.5 }, { 2.0, 0.5, 0.3f } };

    const auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (findNote (*restored, id)->volume == findNote (p, id)->volume);
    CHECK (findNote (*restored, id)->pan == findNote (p, id)->pan);
    CHECK (findNote (*restored, id)->curve.points.empty()); // 包络的节点不会混进音高曲线
}

//==============================================================================
#include "TestHelpers.h"
#include "engine/InstanceLevel.h"

TEST_CASE ("a note's volume envelope plays as level changes on its voice and holds after the note", "[envelope][engine]")
{
    Project p;
    const auto plain = p.addNote (0.0, 4.0, 48, 0.8f);
    const auto faded = p.addNote (0.0, 2.0, 60, 0.8f);
    findNote (p, faded)->volume = fadeOut (2.0);

    test::EngineRunner r (Schedule::compile (p));
    r.playContinuously (0.0, test::beatToSample (4.0), { 333 });

    const auto voice = r.schedule.find (faded)->voice;
    REQUIRE (voice != r.schedule.find (plain)->voice);

    // 只有淡出的声部有音量变化；从 0 dB 开始，一路变小，音符结束时到静音
    REQUIRE_FALSE (r.levels.empty());
    for (const auto& [sample, e] : r.levels)
        CHECK (e.voice == voice);

    float previous = 2.0f;
    for (const auto& [sample, e] : r.levels)
    {
        CHECK (e.gain <= previous + 1.0e-6f);
        previous = e.gain;
    }

    CHECK (r.levels.back().second.gain == 0.0f);

    // 音符结束后不再变化（释放尾音保持静音），也就是最后一个事件不晚于音符结束
    CHECK (r.levels.back().first <= test::beatToSample (2.0) + SketchPlayer::bendInterval);
}

TEST_CASE ("level points ramp smoothly and pan by balance", "[envelope][mix]")
{
    InstanceLevel level;
    std::vector<float> source (100, 1.0f), left (100, 0.0f), right (100, 0.0f);

    // 没有变化：原样输出
    REQUIRE (level.isUnity());
    level.addTo (left.data(), source.data(), 0, 100);
    CHECK (left[50] == 1.0f);

    // 在第 50 个采样到达一半的音量、往右一半：左声道逐渐变到 0.25，右声道逐渐变到 0.5，之后保持
    std::fill (left.begin(), left.end(), 0.0f);
    level.add (50, 0.5f, 0.5f);
    level.addTo (left.data(), source.data(), 0, 100);
    level.addTo (right.data(), source.data(), 1, 100);

    CHECK (left[0] == 1.0f);
    CHECK_THAT (left[25], WithinAbs (0.625, 1e-5));
    CHECK_THAT (left[50], WithinAbs (0.25, 1e-6));
    CHECK_THAT (left[99], WithinAbs (0.25, 1e-6));
    CHECK_THAT (right[99], WithinAbs (0.5, 1e-6));

    for (int i = 1; i < 100; ++i)
        CHECK (left[(size_t) i] <= left[(size_t) i - 1] + 1.0e-6f); // 没有跳变

    // 下一块从最后的值开始
    level.finishBlock();
    CHECK (level.gain == 0.5f);
    CHECK (level.pan == 0.5f);
    CHECK (level.numPoints == 0);
}

TEST_CASE ("ready-made volume shapes and stretching to other notes", "[envelope][editing]")
{
    Envelope e;
    applyVolumeShape (e, VolumeShape::fadeIn, 2.0);
    CHECK (envelope::gainFromDecibels (envelope::volumeAt (e, 0.0)) == 0.0f);
    CHECK (envelope::volumeAt (e, 2.0) == 0.0);
    CHECK (envelope::volumeAt (e, 1.0) < 0.0);

    applyVolumeShape (e, VolumeShape::swell, 2.0);
    CHECK (envelope::volumeAt (e, 1.0) == 0.0);
    CHECK (envelope::volumeAt (e, 0.0) < -12.0);

    const auto stretched = stretchEnvelope (e, 2.0, 4.0);
    CHECK_THAT (stretched.evaluate (2.0), WithinAbs (e.evaluate (1.0), 1e-9));
    CHECK_THAT (stretched.evaluate (1.0), WithinAbs (e.evaluate (0.5), 1e-9));
}
