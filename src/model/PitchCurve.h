#pragma once

#include <vector>

namespace curvable
{

/** 音高曲线上的一个节点。 */
struct CurvePoint
{
    double time = 0.0;       ///< 相对音符起点的位置（拍）
    double semitones = 0.0;  ///< 相对音符本身音高的偏移（半音，可带小数）
    float tension = 0.0f;    ///< 到下一个节点之间的弯曲程度 -1..1（0 = 直线；与 FL 自动化曲线的 tension 类似）

    bool operator== (const CurvePoint&) const = default;
};

/** 一个音符的音高曲线（主曲线）。以后的颤音等调制叠加在它上面。

    - 没有节点：不弯音（偏移恒为 0）。
    - 第一个节点之前取第一个节点的值；最后一个节点之后保持最后一个节点的值。
    - 节点按 time 排序（normalise() 保证）。

    纯 C++，不依赖 JUCE，音频线程上可以直接求值（不分配内存）。
*/
struct PitchCurve
{
    std::vector<CurvePoint> points;

    bool isFlat() const noexcept;

    /** 在 time（相对音符起点，拍）处的偏移（半音）。 */
    double evaluate (double time) const noexcept;

    /** [0, length] 内偏移的最大绝对值（用于检查是否超出弯音范围）。 */
    double getMaxAbsOffset (double length) const noexcept;

    /** 排序、去掉非有限值、把 tension 夹在 -1..1。 */
    void normalise();

    /** 0..1 之间的弯曲形状：tension = 0 为直线，> 0 先慢后快，< 0 先快后慢。 */
    static double shape (double x, float tension) noexcept;

    bool operator== (const PitchCurve&) const = default;
};

} // namespace curvable
