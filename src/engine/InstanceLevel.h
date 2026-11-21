#pragma once

#include <algorithm>
#include <array>

namespace curvable
{

/** 一个音源实例在一个音频块里的输出音量和声像（音符的音量、声像包络）。

    块开头是上一块结束时的值；points 里的每个点在它的采样位置上到达新的值，点与点之间线性过渡（不会有咔哒声），
    最后一个点之后保持不变。纯 C++，音频线程上使用，不分配内存。 */
struct InstanceLevel
{
    struct Point
    {
        int sample = 0;
        float gain = 1.0f;
        float pan = 0.0f;
    };

    float gain = 1.0f; ///< 块开头的增益
    float pan = 0.0f;  ///< 块开头的声像（-1 左 .. 1 右）

    static constexpr int maxPoints = 512;
    std::array<Point, maxPoints> points {};
    int numPoints = 0;

    /** 加一个点。位置早于上一个点时按上一个点的位置处理；点满了时替换最后一个点的值。 */
    void add (int sample, float newGain, float newPan) noexcept
    {
        if (numPoints > 0)
            sample = std::max (sample, points[(std::size_t) numPoints - 1].sample);

        if (numPoints == maxPoints)
            --numPoints;

        points[(std::size_t) numPoints++] = { sample, newGain, std::clamp (newPan, -1.0f, 1.0f) };
    }

    /** 整个块都是原样输出。 */
    bool isUnity() const noexcept { return numPoints == 0 && gain == 1.0f && pan == 0.0f; }

    /** 块结束：最后的值成为下一块开头的值。 */
    void finishBlock() noexcept
    {
        if (numPoints > 0)
        {
            gain = points[(std::size_t) numPoints - 1].gain;
            pan = points[(std::size_t) numPoints - 1].pan;
            numPoints = 0;
        }
    }

    /** 立即回到原样（例如声部换了实例）。 */
    void reset() noexcept
    {
        gain = 1.0f;
        pan = 0.0f;
        numPoints = 0;
    }

    /** 第 channel 个输出声道的增益：声像按平衡处理（往右时左声道变小，右声道不变；反之亦然）。 */
    static float channelGain (float gain, float pan, int channel) noexcept
    {
        if (channel == 0) return gain * (pan > 0.0f ? 1.0f - pan : 1.0f);
        if (channel == 1) return gain * (pan < 0.0f ? 1.0f + pan : 1.0f);
        return gain;
    }

    /** 把 source 乘上这一块的音量、声像后加到 dest 上（numSamples 个采样）。 */
    void addTo (float* dest, const float* source, int channel, int numSamples) const noexcept
    {
        auto from = 0;
        auto fromGain = channelGain (gain, pan, channel);

        auto ramp = [&] (int to, float toGain)
        {
            to = std::clamp (to, from, numSamples);
            const auto length = to - from;

            if (length > 0)
            {
                const auto step = (toGain - fromGain) / (float) length;

                for (int i = 0; i < length; ++i)
                    dest[from + i] += source[from + i] * (fromGain + step * (float) i);
            }

            from = to;
            fromGain = toGain;
        };

        for (int i = 0; i < numPoints; ++i)
            ramp (points[(std::size_t) i].sample, channelGain (points[(std::size_t) i].gain, points[(std::size_t) i].pan, channel));

        for (int i = from; i < numSamples; ++i)
            dest[i] += source[i] * fromGain;
    }
};

} // namespace curvable
