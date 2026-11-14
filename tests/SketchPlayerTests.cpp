#include "engine/SketchPlayer.h"

#include <catch2/catch_test_macros.hpp>

using namespace curvable;

namespace
{
    constexpr double spb = 100.0; // 每拍 100 个采样，便于计算

    Schedule makeSchedule (std::initializer_list<Note> notes)
    {
        Project p;
        for (auto n : notes)
            p.addNote (n.start, n.length, n.pitch, n.velocity);
        return Schedule::compile (p);
    }
}

TEST_CASE ("render emits note on and off inside the block at the right samples", "[player]")
{
    const auto s = makeSchedule ({ { 0, 0.5, 0.25, 60, 1.0f } });
    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);

    player.render (0.0, 1.0, 0.0, 0, 100, spb, out);

    REQUIRE (out.get().size() == 2);
    CHECK (out.get()[0] == MidiNoteEvent { 50, true, 60, 127 });
    CHECK (out.get()[1] == MidiNoteEvent { 75, false, 60, 0 });
    CHECK_FALSE (player.hasActiveNotes());
}

TEST_CASE ("a note spanning blocks ends in a later block", "[player]")
{
    const auto s = makeSchedule ({ { 0, 0.5, 1.0, 60, 1.0f } });
    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);

    player.render (0.0, 1.0, 0.0, 0, 100, spb, out);
    REQUIRE (out.get().size() == 1);
    CHECK (player.hasActiveNotes());

    out.clear();
    player.render (1.0, 2.0, 0.0, 0, 100, spb, out);
    REQUIRE (out.get().size() == 1);
    CHECK (out.get()[0] == MidiNoteEvent { 50, false, 60, 0 });
}

TEST_CASE ("back-to-back notes of the same pitch: off comes before on", "[player]")
{
    const auto s = makeSchedule ({ { 0, 0.0, 0.5, 60, 1.0f }, { 0, 0.5, 0.5, 60, 1.0f } });
    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);

    player.render (0.0, 2.0, 0.0, 0, 200, spb, out);

    const auto& e = out.get();
    REQUIRE (e.size() == 4);
    CHECK (e[0].noteOn);
    CHECK (e[1] == MidiNoteEvent { 50, false, 60, 0 });
    CHECK (e[2] == MidiNoteEvent { 50, true, 60, 127 });
    CHECK (e[3] == MidiNoteEvent { 100, false, 60, 0 });
}

TEST_CASE ("overlapping notes of the same pitch don't cut each other off", "[player]")
{
    // 第二个音在第一个结束前开始：重新触发；第一个结束时不发 note off，第二个结束时才发
    const auto s = makeSchedule ({ { 0, 0.0, 1.0, 60, 1.0f }, { 0, 0.5, 1.0, 60, 1.0f } });
    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);

    player.render (0.0, 2.0, 0.0, 0, 200, spb, out);

    const auto& e = out.get();
    REQUIRE (e.size() == 4);
    CHECK (e[0] == MidiNoteEvent { 0, true, 60, 127 });
    CHECK (e[1] == MidiNoteEvent { 50, false, 60, 0 }); // 重新触发
    CHECK (e[2] == MidiNoteEvent { 50, true, 60, 127 });
    CHECK (e[3] == MidiNoteEvent { 150, false, 60, 0 });
}

TEST_CASE ("start chases notes that span the position", "[player]")
{
    const auto s = makeSchedule ({ { 0, 0.0, 4.0, 48, 1.0f }, { 0, 2.0, 1.0, 60, 1.0f }, { 0, 1.0, 0.5, 72, 1.0f } });
    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);

    player.start (2.0, 7, out);

    // 48 跨越位置 2 → 补发；60 正好从 2 开始 → 留给 render；72 已结束
    REQUIRE (out.get().size() == 1);
    CHECK (out.get()[0] == MidiNoteEvent { 7, true, 48, 127 });
}

TEST_CASE ("setSchedule stops removed notes and keeps the rest", "[player]")
{
    Project p;
    const auto keep = p.addNote (0.0, 4.0, 48, 1.0f);
    p.addNote (0.0, 4.0, 60, 1.0f);
    const auto before = Schedule::compile (p);

    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&before, 0.0, 0, out);
    player.render (0.0, 1.0, 0.0, 0, 100, spb, out);
    REQUIRE (out.get().size() == 2);

    // 删除 60，并把 48 缩短到 2 拍
    Project q;
    q.notes = { p.notes[0] };
    q.notes[0].length = 2.0;
    REQUIRE (q.notes[0].id == keep);
    const auto after = Schedule::compile (q);

    out.clear();
    player.setSchedule (&after, 1.0, 3, out);
    REQUIRE (out.get().size() == 1);
    CHECK (out.get()[0] == MidiNoteEvent { 3, false, 60, 0 });

    out.clear();
    player.render (1.0, 3.0, 0.0, 0, 200, spb, out);
    REQUIRE (out.get().size() == 1);
    CHECK (out.get()[0] == MidiNoteEvent { 100, false, 48, 0 }); // 新的结束时间
}

TEST_CASE ("stopAll silences everything once", "[player]")
{
    const auto s = makeSchedule ({ { 0, 0.0, 4.0, 48, 1.0f }, { 0, 0.0, 4.0, 52, 1.0f } });
    SketchPlayer player;
    MidiEventList out;
    player.setSchedule (&s, 0.0, 0, out);
    player.render (0.0, 1.0, 0.0, 0, 100, spb, out);

    out.clear();
    player.stopAll (10, out);
    CHECK (out.get().size() == 2);
    CHECK_FALSE (player.hasActiveNotes());

    out.clear();
    player.stopAll (20, out);
    CHECK (out.get().empty());
}
