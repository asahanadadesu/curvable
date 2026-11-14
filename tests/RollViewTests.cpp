#include "ui/RollView.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace curvable;
using Catch::Matchers::WithinAbs;

TEST_CASE ("beat and pitch coordinates round-trip", "[rollview]")
{
    RollView v;
    v.startBeat = 2.0;
    v.pixelsPerBeat = 50.0;
    v.topPitch = 72.0;
    v.rowHeight = 10.0;

    CHECK_THAT (v.beatToX (4.0), WithinAbs (100.0, 1e-12));
    CHECK_THAT (v.xToBeat (100.0), WithinAbs (4.0, 1e-12));

    // 音高 p 的行占据 [pitchToY (p), pitchToY (p) + rowHeight)
    CHECK_THAT (v.pitchToY (72), WithinAbs (0.0, 1e-12));
    CHECK_THAT (v.pitchToY (70), WithinAbs (20.0, 1e-12));
    CHECK (v.yToPitch (0.0) == 72);
    CHECK (v.yToPitch (9.9) == 72);
    CHECK (v.yToPitch (10.0) == 71);
    CHECK (v.yToPitch (25.0) == 70);
}

TEST_CASE ("zoom keeps the anchor fixed", "[rollview]")
{
    RollView v;
    v.startBeat = 3.0;

    const auto beatBefore = v.xToBeat (240.0);
    v.zoomX (2.0, 240.0);
    CHECK_THAT (v.xToBeat (240.0), WithinAbs (beatBefore, 1e-9));
    CHECK_THAT (v.pixelsPerBeat, WithinAbs (120.0, 1e-12));

    v.zoomX (1000.0, 0.0);
    CHECK (v.pixelsPerBeat == RollView::maxPixelsPerBeat);

    const auto pitchBefore = v.topPitch - 85.0 / v.rowHeight;
    v.zoomY (1.5, 85.0);
    CHECK_THAT (v.topPitch - 85.0 / v.rowHeight, WithinAbs (pitchBefore, 1e-9));
}

TEST_CASE ("clamp keeps the view inside the valid range", "[rollview]")
{
    RollView v;
    v.startBeat = -4.0;
    v.topPitch = 200.0;
    v.clamp (800.0, 400.0);
    CHECK (v.startBeat == 0.0);
    CHECK (v.topPitch == 127.0);

    // 不留出音高 0 以下的空白
    v.topPitch = 3.0;
    v.rowHeight = 10.0;
    v.clamp (800.0, 400.0);
    CHECK_THAT (v.topPitch, WithinAbs (39.0, 1e-12));

    // 区域比 128 行还高时贴顶
    v.rowHeight = 7.0;
    v.clamp (800.0, 2000.0);
    CHECK (v.topPitch == 127.0);
}
