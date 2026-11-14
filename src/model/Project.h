#pragma once

#include <cstdint>
#include <vector>

namespace curvable
{

using NoteId = std::uint64_t;

/** Curvable 内部卷帘上的一个音符。时间单位均为拍（四分音符），与宿主速度无关。 */
struct Note
{
    NoteId id = 0;
    double start = 0.0;    ///< 起始位置（拍）
    double length = 1.0;   ///< 时长（拍），必须 > 0
    int pitch = 60;        ///< MIDI 音高 0..127
    float velocity = 0.8f; ///< 0..1

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
