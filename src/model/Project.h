#pragma once

#include "model/PitchCurve.h"

#include <cstdint>
#include <vector>

namespace curvable
{

using NoteId = std::uint64_t;

/** 音量、声像包络：沿用 PitchCurve 的节点和每段弯曲（不用颤音）。节点的 semitones 字段存的是值：
    - 音量：相对 0 dB 的分贝数，envelope::volumeMin 及以下为静音，最大 envelope::volumeMax；
      节点之间按推子位置过渡（envelope::volumeAt），不按分贝，这样淡入淡出听起来均匀；
    - 声像：-1（左）..1（右）。
    没有节点时为 0（音量不变、居中）。由 Curvable 直接调整这个音符所在的音源副本的输出，任何音源都能用；
    MPE 模式下所有音符在同一个实例里，不起作用。 */
using Envelope = PitchCurve;

namespace envelope
{
    constexpr double volumeMin = -48.0; ///< 这个值及以下为静音
    constexpr double volumeMax = 6.0;

    /** 分贝 → 增益（volumeMin 及以下为 0）。 */
    float gainFromDecibels (double decibels) noexcept;

    /** 分贝 ↔ 推子位置 0..1（sqrt (增益 / 2)：0 = 静音，0 dB 约 0.71，1 = volumeMax）。 */
    double faderFromDecibels (double decibels) noexcept;
    double decibelsFromFader (double fader) noexcept;

    /** 音量包络在 time 处的分贝数：相邻节点之间按推子位置过渡（含每段的弯曲）。 */
    double volumeAt (const Envelope&, double time) noexcept;

    /** 把节点的值在分贝和推子位置之间换算（切开音量包络时在推子位置上切）。 */
    Envelope toFader (Envelope);
    Envelope fromFader (Envelope);

    /** 夹住节点的值，去掉颤音（包络不用）。 */
    void normaliseVolume (Envelope&);
    void normalisePan (Envelope&);
}

/** Curvable 内部卷帘上的一个音符。时间单位均为拍（四分音符），与宿主速度无关。 */
struct Note
{
    NoteId id = 0;
    double start = 0.0;    ///< 起始位置（拍）
    double length = 1.0;   ///< 时长（拍），必须 > 0
    int pitch = 60;        ///< MIDI 音高 0..127
    float velocity = 0.8f; ///< 0..1
    PitchCurve curve;      ///< 音高曲线（相对本音符）；没有节点即不弯音
    Envelope volume;       ///< 音量包络（分贝，相对 0 dB）
    Envelope pan;          ///< 声像包络（-1..1）

    double getEnd() const noexcept { return start + length; }

    bool operator== (const Note&) const = default;
};

enum class PlaybackMode
{
    automatic, ///< AUTO：跟随宿主时间线
    trigger,   ///< TRIGGER：宿主音符触发播放，按住期间循环
    off        ///< OFF：不播放
};

/** TRIGGER 模式下的一段：按下 key 时从 start 开始播放，到 end 后回到 start 循环。 */
struct TriggerCue
{
    static constexpr int anyKey = -1;

    int key = anyKey;   ///< 触发键（MIDI 音高）；anyKey 表示任意键
    double start = 0.0;
    double end = 0.0;   ///< end <= start 时表示"到内容结尾"（按小节取整）

    bool operator== (const TriggerCue&) const = default;
};

/** 一个 Curvable 实例的全部编辑内容。只在消息线程上修改。 */
struct Project
{
    std::vector<Note> notes;
    std::vector<TriggerCue> cues { TriggerCue {} };
    PlaybackMode mode = PlaybackMode::automatic;
    NoteId nextId = 1;

    /** 弯音范围（单向，半音），必须与音源的设置一致，否则音高会不准。 */
    double bendRange = defaultBendRange;

    /** 插件内播放的循环区（拍）。loopEnd > loopStart 时有循环区；loopOn 决定插件内播放是否在里面循环。 */
    double loopStart = 0.0, loopEnd = 0.0;
    bool loopOn = false;

    bool hasLoop() const noexcept { return loopEnd > loopStart; }

    static constexpr double defaultBendRange = 2.0; ///< 大多数音源的默认值
    static constexpr double minBendRange = 1.0;
    static constexpr double maxBendRange = 96.0;

    /** 添加一个音符并分配 id。 */
    NoteId addNote (double start, double length, int pitch, float velocity);

    /** 最后一个音符的结束位置（拍）；没有音符时为 0。 */
    double getContentEnd() const noexcept;

    /** 测试用的乐句：两小节，含和弦与旋律。 */
    static Project createTestPhrase();
};

/** 向上取整到整小节（默认 4/4）。0 保持为 0。 */
double roundUpToBar (double beats, double beatsPerBar = 4.0) noexcept;

} // namespace curvable
