#pragma once

namespace curvable
{

/** 14 位 MIDI 弯音值（0..16383，8192 为中心）。 */
struct PitchBendValue
{
    int value = 8192;
    bool clipped = false; ///< 目标偏移超出弯音范围而被截断
};

/** 把相对音符基准音高的偏移（音分）换算为 14 位弯音值。

    @param cents           音高偏移，单位音分（100 = 一个半音）
    @param rangeSemitones  音源的弯音范围（单向，半音），必须 > 0
*/
PitchBendValue centsToPitchBend (double cents, double rangeSemitones) noexcept;

} // namespace curvable
