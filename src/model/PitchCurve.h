#pragma once

#include <optional>
#include <vector>

namespace curvable
{

/** 颤音 / trill（与相邻音快速交替）的参数。

    两种用法：
    - 整个音符：PitchCurve::vibrato，wholeNote 为 true 时作用于整个音符；
    - 按段：每个节点可以带自己的一组参数（CurvePoint::vibrato），作用于从这个节点到下一个节点的一段。
      相邻、参数完全相同的段连成一段，相位连续；参数不同的段各自起振、各自收尾。 */
struct Vibrato
{
    enum class Shape { sine, trill };

    Shape shape = Shape::sine; ///< sine：正弦颤音；trill：在本音与相邻音之间交替
    bool wholeNote = false;    ///< 只用于 PitchCurve::vibrato：整个音符都有（这时不看节点上的颤音）
    double depth = 0.4;        ///< sine：峰值（半音，≥ 0）；trill：相邻音的音程（半音，可为负 = 下方的音）
    double rate = 5.5;      ///< sync 为 false 时单位是 Hz；为 true 时是每拍的周期数（2 = 八分音符）
    bool sync = false;
    double fade = 0.5;      ///< 每段颤音开头从 0 渐强到完整深度所用的时间（拍）

    /** 一个周期有几拍。 */
    double beatsPerCycle (double secondsPerBeat) const noexcept;

    /** 一段颤音里，距这段开头 x 拍处的包络（0..1）：开头渐强（trill 不渐强），结尾很快收住（不会在段尾跳变）。 */
    double envelope (double x, double runLength) const noexcept;

    /** 一段颤音里，距这段开头 x 拍处的偏移（半音）。从相位 0 开始，起振时不会跳变。 */
    double evaluate (double x, double runLength, double secondsPerBeat) const noexcept;

    void normalise();

    /** 换成另一种形状，参数换成那种形状的默认值（渐强保留）。 */
    void setShape (Shape);

    /** trill 的波形（0..1）：前半周期在本音，后半周期在相邻音，切换处有很短的过渡。 */
    static double trillWave (double phase) noexcept;

    bool operator== (const Vibrato&) const = default;
};

/** 音高曲线上的一个节点。 */
struct CurvePoint
{
    double time = 0.0;       ///< 相对音符起点的位置（拍）
    double semitones = 0.0;  ///< 相对音符本身音高的偏移（半音，可带小数）
    float tension = 0.0f;    ///< 到下一个节点之间的弯曲程度 -1..1（0 = 直线；与 FL 自动化曲线的 tension 类似）
    std::optional<Vibrato> vibrato; ///< 从这个节点到下一个节点（最后一个节点：到音符结尾）这一段的颤音 / trill；没有时为空

    bool operator== (const CurvePoint&) const = default;
};

/** 一个音符的音高曲线：主曲线（节点）+ 叠加的颤音。

    - 没有节点：主曲线不弯（偏移恒为 0）。
    - 第一个节点之前取第一个节点的值；最后一个节点之后保持最后一个节点的值。
    - 节点按 time 排序（normalise() 保证）。

    纯 C++，不依赖 JUCE，音频线程上可以直接求值（不分配内存）。
*/
struct PitchCurve
{
    std::vector<CurvePoint> points;
    Vibrato vibrato;

    /** 一段连续的颤音（相对音符起点，拍）。 */
    struct Run
    {
        double start = 0.0, end = 0.0;
        Vibrato settings;   ///< 这一段的参数
        int firstPoint = -1, lastPoint = -1; ///< 组成这一段的节点（起点节点的下标）；整个音符的颤音为 -1

        double length() const noexcept { return end - start; }
        bool isWholeNote() const noexcept { return firstPoint < 0; }
    };

    /** 完全不弯（主曲线全为 0，也没有颤音）。 */
    bool isFlat() const noexcept;

    /** 主曲线全为 0（不看颤音）。 */
    bool pointsFlat() const noexcept;

    /** 有没有颤音（整个音符，或者至少一段）。 */
    bool hasVibrato() const noexcept;

    /** 有没有节点带颤音。 */
    bool hasSegmentVibrato() const noexcept;

    /** 有没有某种形状的颤音（整个音符或任意一段）。 */
    bool hasVibratoShape (Vibrato::Shape) const noexcept;

    /** 所有生效的颤音里，最短的一个周期（拍）；没有颤音时为 0。不分配内存（界面每个像素都会调用）。 */
    double minBeatsPerCycle (double secondsPerBeat) const noexcept;

    /** 主曲线在 time（相对音符起点，拍）处的偏移（半音），不含颤音。 */
    double evaluate (double time) const noexcept;

    /** 主曲线 + 颤音。length 是音符长度（拍）。 */
    double evaluate (double time, double length, double secondsPerBeat) const noexcept
    {
        return evaluate (time) + vibratoAt (time, length, secondsPerBeat);
    }

    /** 颤音在 time 处的偏移（半音）；不在颤音段里时为 0。 */
    double vibratoAt (double time, double length, double secondsPerBeat) const noexcept;

    /** time 所在的颤音段；不在颤音段里时返回 false。不分配内存。 */
    bool findVibratoRun (double time, double length, Run& run) const noexcept;

    /** 所有颤音段（界面用）。 */
    std::vector<Run> getVibratoRuns (double length) const;

    /** time 所在的段（起点节点的下标）；在第一个节点之前时为 -1。 */
    int segmentAt (double time) const noexcept;

    /** [0, length] 内偏移的最大绝对值（含颤音深度，用于检查是否超出弯音范围）。 */
    double getMaxAbsOffset (double length) const noexcept;

    /** 排序、去掉非有限值、把 tension 夹在 -1..1，颤音参数夹在合理范围内（节点上的参数 wholeNote 一律为 false）。 */
    void normalise();

    /** 0..1 之间的弯曲形状：tension = 0 为直线，> 0 先慢后快，< 0 先快后慢。 */
    static double shape (double x, float tension) noexcept;

    bool operator== (const PitchCurve&) const = default;
};

} // namespace curvable
