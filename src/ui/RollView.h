#pragma once

#include <algorithm>
#include <cmath>

namespace curvable
{

/** 卷帘的坐标换算（拍 ↔ x，音高 ↔ y）。不依赖 JUCE，便于测试。

    topPitch 是最上面一行的音高，可以带小数（平滑滚动）。
    音高 p 的那一行占据 y ∈ [pitchToY (p), pitchToY (p) + rowHeight)。
*/
struct RollView
{
    double startBeat = 0.0;
    double pixelsPerBeat = 60.0;
    double topPitch = 84.0;
    double rowHeight = 17.0;

    static constexpr double minPixelsPerBeat = 6.0;
    static constexpr double maxPixelsPerBeat = 800.0;
    static constexpr double minRowHeight = 7.0;
    static constexpr double maxRowHeight = 40.0;

    double beatToX (double beat) const noexcept { return (beat - startBeat) * pixelsPerBeat; }
    double xToBeat (double x) const noexcept    { return startBeat + x / pixelsPerBeat; }

    double pitchToY (int pitch) const noexcept  { return (topPitch - pitch) * rowHeight; }
    int yToPitch (double y) const noexcept      { return (int) std::ceil (topPitch - y / rowHeight - 1.0e-9); }

    /** 水平缩放，保持 anchorX 处的拍不动。 */
    void zoomX (double factor, double anchorX) noexcept
    {
        const auto anchorBeat = xToBeat (anchorX);
        pixelsPerBeat = std::clamp (pixelsPerBeat * factor, minPixelsPerBeat, maxPixelsPerBeat);
        startBeat = anchorBeat - anchorX / pixelsPerBeat;
    }

    /** 垂直缩放，保持 anchorY 处的音高不动。 */
    void zoomY (double factor, double anchorY) noexcept
    {
        const auto anchorPitch = topPitch - anchorY / rowHeight;
        rowHeight = std::clamp (rowHeight * factor, minRowHeight, maxRowHeight);
        topPitch = anchorPitch + anchorY / rowHeight;
    }

    /** 限制在有效范围内：不早于 0 拍；音高 0..127 之外不留空白（区域高于全部 128 行时贴顶）。 */
    void clamp (double width, double height) noexcept
    {
        pixelsPerBeat = std::clamp (pixelsPerBeat, minPixelsPerBeat, maxPixelsPerBeat);
        rowHeight = std::clamp (rowHeight, minRowHeight, maxRowHeight);
        startBeat = std::max (0.0, startBeat);

        const auto visibleRows = height / rowHeight;
        const auto highestTop = 127.0;
        const auto lowestTop = std::min (highestTop, visibleRows - 1.0);
        topPitch = std::clamp (topPitch, lowestTop, highestTop);
        (void) width;
    }

    double visibleBeats (double width) const noexcept { return width / pixelsPerBeat; }
};

} // namespace curvable
