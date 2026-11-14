#include "engine/Schedule.h"
#include "model/Project.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <limits>

using namespace curvable;
using Catch::Matchers::WithinAbs;

TEST_CASE ("addNote assigns increasing unique ids", "[project]")
{
    Project p;
    const auto a = p.addNote (0, 1, 60, 0.5f);
    const auto b = p.addNote (1, 1, 62, 0.5f);

    CHECK (a != b);
    CHECK (b > a);
    CHECK (p.notes.size() == 2);
    CHECK (p.nextId > b);
}

TEST_CASE ("content end and bar rounding", "[project]")
{
    Project p;
    CHECK (p.getContentEnd() == 0.0);

    p.addNote (2.0, 1.5, 60, 0.5f);
    CHECK_THAT (p.getContentEnd(), WithinAbs (3.5, 1e-12));

    CHECK (roundUpToBar (0.0) == 0.0);
    CHECK (roundUpToBar (3.5) == 4.0);
    CHECK (roundUpToBar (4.0) == 4.0);
    CHECK (roundUpToBar (8.0 + 1e-12) == 8.0); // 浮点误差不应多出一小节
    CHECK (roundUpToBar (8.01) == 12.0);
}

TEST_CASE ("test phrase is two bars with chords and melody", "[project]")
{
    const auto p = Project::createTestPhrase();
    CHECK (p.notes.size() == 22);
    CHECK (roundUpToBar (p.getContentEnd()) == 8.0);
}

TEST_CASE ("schedule compile sorts, converts velocity and drops invalid notes", "[schedule]")
{
    Project p;
    p.addNote (2.0, 1.0, 64, 1.0f);
    p.addNote (0.0, 1.0, 60, 0.0f);   // 力度 0 → 至少为 1
    p.addNote (1.0, 0.0, 62, 0.5f);   // 时长为 0：丢弃
    p.addNote (1.0, 1.0, 200, 0.5f);  // 音高越界：丢弃
    p.addNote (-1.0, 1.0, 61, 0.5f);  // 负起点：丢弃
    p.addNote (std::numeric_limits<double>::quiet_NaN(), 1.0, 63, 0.5f);

    const auto s = Schedule::compile (p);

    REQUIRE (s.notes.size() == 2);
    CHECK (s.notes[0].pitch == 60);
    CHECK (s.notes[0].velocity == 1);
    CHECK (s.notes[1].pitch == 64);
    CHECK (s.notes[1].velocity == 127);
    CHECK (s.notes[1].end == 3.0);
    CHECK (s.length == 4.0);
}

TEST_CASE ("cue resolution", "[schedule]")
{
    Project p;
    p.addNote (0.0, 6.0, 60, 0.5f);
    const auto s = Schedule::compile (p);

    double start = 0, end = 0;

    s.resolveCue ({ TriggerCue::anyKey, 0.0, 0.0 }, start, end); // 到内容结尾
    CHECK (start == 0.0);
    CHECK (end == 8.0);

    s.resolveCue ({ 60, 2.0, 3.0 }, start, end);
    CHECK (start == 2.0);
    CHECK (end == 3.0);

    const auto empty = Schedule::compile (Project {});
    empty.resolveCue ({}, start, end); // 空内容：退回一小节
    CHECK (end - start == 4.0);
}
