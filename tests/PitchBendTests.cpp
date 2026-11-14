#include "engine/PitchBend.h"

#include <catch2/catch_test_macros.hpp>

using curvable::centsToPitchBend;

TEST_CASE ("zero offset gives centre value", "[pitchbend]")
{
    const auto pb = centsToPitchBend (0.0, 2.0);
    CHECK (pb.value == 8192);
    CHECK_FALSE (pb.clipped);
}

TEST_CASE ("full range maps to the extremes", "[pitchbend]")
{
    CHECK (centsToPitchBend (200.0, 2.0).value == 16383);
    CHECK (centsToPitchBend (-200.0, 2.0).value == 0);
    CHECK (centsToPitchBend (4800.0, 48.0).value == 16383);
    CHECK_FALSE (centsToPitchBend (200.0, 2.0).clipped);
}

TEST_CASE ("half range offset", "[pitchbend]")
{
    CHECK (centsToPitchBend (100.0, 2.0).value == 8192 + 4096); // round (0.5 * 8191)
    CHECK (centsToPitchBend (-100.0, 2.0).value == 8192 - 4096);
}

TEST_CASE ("out of range offsets are clipped and flagged", "[pitchbend]")
{
    const auto up = centsToPitchBend (250.0, 2.0);
    CHECK (up.value == 16383);
    CHECK (up.clipped);

    const auto down = centsToPitchBend (-1000.0, 2.0);
    CHECK (down.value == 0);
    CHECK (down.clipped);
}

TEST_CASE ("invalid range returns centre value", "[pitchbend]")
{
    CHECK (centsToPitchBend (100.0, 0.0).value == 8192);
    CHECK (centsToPitchBend (100.0, -2.0).value == 8192);
}
