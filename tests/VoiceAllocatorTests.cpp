#include "engine/PlaybackEngine.h"
#include "engine/VoiceAllocator.h"
#include "model/NoteEditing.h"

#include <catch2/catch_test_macros.hpp>

#include <set>

using namespace curvable;

namespace
{
    PitchCurve slide (double semitones)
    {
        PitchCurve c;
        c.points = { { 0.0, 0.0 }, { 1.0, semitones } };
        return c;
    }

    NoteId add (Project& p, double start, double length, int pitch, PitchCurve curve = {})
    {
        const auto id = p.addNote (start, length, pitch, 0.8f);
        editing::findNote (p, id)->curve = std::move (curve);
        return id;
    }

    int voiceOf (const Schedule& s, NoteId id)
    {
        const auto* n = s.find (id);
        REQUIRE (n != nullptr);
        return n->voice;
    }
}

TEST_CASE ("notes without bends all share the first voice", "[voices]")
{
    Project p;
    for (int i = 0; i < 8; ++i)
        add (p, 0.0, 4.0, 48 + i * 3); // 八个同时发声的音

    const auto s = Schedule::compile (p);
    CHECK (s.voiceCount == 1);
    for (const auto& n : s.notes)
        CHECK (n.voice == 0);
}

TEST_CASE ("a four-note chord with two opposite slides needs three voices", "[voices]")
{
    Project p;
    const auto a = add (p, 0.0, 4.0, 48);
    const auto b = add (p, 0.0, 4.0, 52);
    const auto up = add (p, 0.0, 4.0, 55, slide (2.0));
    const auto down = add (p, 0.0, 4.0, 60, slide (-2.0));

    const auto s = Schedule::compile (p);
    CHECK (s.voiceCount == 3);
    CHECK (s.overflowNotes == 0);
    CHECK (voiceOf (s, a) == voiceOf (s, b));
    CHECK (voiceOf (s, up) != voiceOf (s, down));
    CHECK (voiceOf (s, up) != voiceOf (s, a));
    CHECK (voiceOf (s, down) != voiceOf (s, a));
}

TEST_CASE ("chord notes moving in parallel share a voice", "[voices]")
{
    Project p;
    const auto a = add (p, 0.0, 2.0, 60, slide (2.0));
    const auto b = add (p, 0.0, 2.0, 64, slide (2.0));
    const auto c = add (p, 0.0, 2.0, 67, slide (2.0));

    const auto s = Schedule::compile (p);
    CHECK (s.voiceCount == 1);
    CHECK (voiceOf (s, a) == voiceOf (s, b));
    CHECK (voiceOf (s, b) == voiceOf (s, c));
}

TEST_CASE ("same curve but different start times cannot share while overlapping", "[voices]")
{
    Project p;
    const auto a = add (p, 0.0, 2.0, 60, slide (2.0));
    const auto b = add (p, 1.0, 2.0, 64, slide (2.0));

    const auto s = Schedule::compile (p);
    CHECK (voiceOf (s, a) != voiceOf (s, b));
}

TEST_CASE ("the release guard keeps a bent note's tail on its own voice", "[voices]")
{
    Project p;
    const auto bent = add (p, 0.0, 1.0, 60, slide (-1.0));
    const auto soon = add (p, 1.5, 1.0, 62);   // 在保护时长内开始
    const auto later = add (p, 2.5, 1.0, 64);  // 保护时长已过

    const auto s = Schedule::compile (p);
    CHECK (voiceOf (s, bent) == 0);
    CHECK (voiceOf (s, soon) == 1);
    CHECK (voiceOf (s, later) == 0);  // 回到最小的声部
    CHECK (s.voiceCount == 2);
}

TEST_CASE ("a bent lead after a flat chord reuses the first voice once the chord is over", "[voices]")
{
    Project p;
    add (p, 0.0, 1.0, 48);
    add (p, 0.0, 1.0, 52);
    const auto lead = add (p, 4.0, 1.0, 72, slide (2.0));

    const auto s = Schedule::compile (p);
    CHECK (voiceOf (s, lead) == 0);
    CHECK (s.voiceCount == 1);
}

TEST_CASE ("more conflicting notes than the limit are counted as overflow", "[voices]")
{
    Project p;
    for (int i = 0; i < Schedule::maxVoices + 3; ++i)
        add (p, 0.0, 2.0, 40 + i, slide (0.1 * (i + 1))); // 每个音的曲线都不同

    const auto s = Schedule::compile (p);
    CHECK (s.voiceCount == Schedule::maxVoices);
    CHECK (s.overflowNotes == 3);

    int flagged = 0;
    for (const auto& n : s.notes)
    {
        CHECK ((n.voice >= 0 && n.voice < Schedule::maxVoices));
        flagged += n.overflow ? 1 : 0;
    }
    // 3 个被迫共用的音符，加上各自声部里原有的那个音符（重叠期间弯音被新音符接管）
    CHECK (flagged == 6);
}

TEST_CASE ("each voice gets its own notes and pitch wheel events", "[voices][engine]")
{
    Project p;
    const auto flat = add (p, 0.0, 2.0, 48);
    const auto up = add (p, 0.0, 2.0, 55, slide (2.0));
    const auto down = add (p, 0.0, 2.0, 60, slide (-2.0));
    const auto s = Schedule::compile (p);

    PlaybackEngine engine;
    engine.prepare (48000.0);
    MidiEventList out;
    engine.process (PlaybackMode::automatic, &s, { true, true, 0.0, 120.0 }, 48000, nullptr, 0, out);

    std::set<int> wheelVoices;

    for (const auto& e : out.get())
    {
        if (e.isLevel)
            continue;

        if (e.isPitchWheel)
        {
            wheelVoices.insert (e.voice);
            continue;
        }

        if (e.noteOn)
        {
            const auto expected = e.pitch == 48 ? voiceOf (s, flat) : e.pitch == 55 ? voiceOf (s, up) : voiceOf (s, down);
            CHECK (e.voice == expected);
        }
    }

    // 只有弯音的两个声部会发弯音，不弯的声部不发
    CHECK (wheelVoices == std::set<int> { voiceOf (s, up), voiceOf (s, down) });
}

TEST_CASE ("resyncing voices stops notes, recentres every wheel and re-chases", "[voices][engine]")
{
    Project p;
    add (p, 0.0, 4.0, 60);
    const auto s = Schedule::compile (p);

    PlaybackEngine engine;
    engine.prepare (48000.0);
    MidiEventList out;
    engine.process (PlaybackMode::automatic, &s, { true, true, 0.0, 120.0 }, 512, nullptr, 0, out);

    out.clear();
    engine.stopVoices (out);
    REQUIRE (out.get().size() == 1);
    CHECK_FALSE (out.get()[0].noteOn);
    CHECK_FALSE (out.get()[0].isPitchWheel);

    // 每个声部都会发一次中心值，即使它认为自己的弯音本来就在中心
    out.clear();
    engine.resyncVoices (out);
    std::set<int> recentred;
    for (const auto& e : out.get())
        if (e.isPitchWheel && e.wheelValue == 8192)
            recentred.insert (e.voice);
    CHECK ((int) recentred.size() == Schedule::maxVoices);

    out.clear();
    engine.process (PlaybackMode::automatic, &s, { true, true, 512.0 / 24000.0, 120.0 }, 512, nullptr, 0, out);
    REQUIRE_FALSE (out.get().empty());
    CHECK (out.get()[0].noteOn); // 补发
}

TEST_CASE ("three different slides plus a flat note need four voices", "[voices]")
{
    for (int flatPitch : { 40, 57, 70 }) // 不弯的音在和弦的最低、中间、最高
    {
        Project p;
        add (p, 0.0, 4.0, 48, slide (2.0));
        add (p, 0.0, 4.0, 55, slide (-2.0));
        add (p, 0.0, 4.0, 64, slide (5.0));
        const auto flat = add (p, 0.0, 4.0, flatPitch);

        const auto s = Schedule::compile (p);
        CHECK (s.voiceCount == 4);

        for (const auto& n : s.notes)
            if (n.id != flat)
                CHECK (n.voice != voiceOf (s, flat));
    }
}
