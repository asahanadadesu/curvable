#pragma once

#include "engine/Schedule.h"

#include <vector>

namespace curvable
{

/** 引擎输出的一个 MIDI 事件：音符开 / 关，或弯音（isPitchWheel）。 */
struct MidiNoteEvent
{
    int sampleOffset = 0; ///< 在当前音频块内的位置
    bool noteOn = false;
    int pitch = 60;
    int velocity = 0;     ///< note off 时为 0
    bool isPitchWheel = false;
    int wheelValue = 8192; ///< 14 位弯音值（isPitchWheel 时有效）
    int voice = 0;         ///< 声部（发给哪个音源实例）

    static MidiNoteEvent pitchWheel (int sampleOffset, int value, int voice = 0) noexcept
    {
        return { sampleOffset, false, 0, 0, true, value, voice };
    }

    bool operator== (const MidiNoteEvent&) const = default;
};

/** 输出缓冲：预先分配容量，音频线程上追加时不分配内存；超出容量的事件会被丢弃。 */
class MidiEventList
{
public:
    explicit MidiEventList (std::size_t capacity = 4096) { events.reserve (capacity); }

    void clear() noexcept { events.clear(); }
    void add (const MidiNoteEvent& e) noexcept
    {
        if (events.size() < events.capacity())
            events.push_back (e);
        else
            ++dropped;
    }

    void add (MidiNoteEvent e, int voice) noexcept
    {
        e.voice = voice;
        if (events.size() < events.capacity())
            events.push_back (e);
        else
            ++dropped;
    }

    const std::vector<MidiNoteEvent>& get() const noexcept { return events; }
    std::size_t getNumDropped() const noexcept { return dropped; }

private:
    std::vector<MidiNoteEvent> events;
    std::size_t dropped = 0;
};

/** 把 Schedule 里的音符按"拍"区间转换为 MIDI 事件，并记录哪些音符正在发声。

    只在音频线程上使用（prepare 除外），不分配内存。
    同一音高的音符重叠时：后一个会重新触发（先 off 再 on），只有最后一个结束时才真正发送 note off，
    避免早结束的音符把仍在响的同音高音符切断。

    每个 SketchPlayer 只负责一个声部（setVoice），只播放分配到这个声部的音符，输出的事件都标上这个声部。

    弯音：弯音轮跟随本声部最近开始、仍在发声的音符的曲线（同一声部里重叠的音符弯音相同，见 VoiceAllocator）。
    - 音符开始时，先在同一采样上发送它起点处的弯音值，再发 note on（滑入的音不会先响一下原音高）。
    - 发声期间每 bendInterval 个采样求值一次，值变化时才发送。
    - 音符结束后保持最后的值（释放尾音不会跳回原音高），直到下一个音符开始；停止播放时回到中心。
*/
class SketchPlayer
{
public:
    SketchPlayer();

    /** 用于以 Hz 计的颤音速率。 */
    void setSampleRate (double sr) noexcept { sampleRate = sr > 0.0 ? sr : 44100.0; }

    /** 只播放分配到这个声部的音符。 */
    void setVoice (int v) noexcept { voice = v; }
    int getVoice() const noexcept { return voice; }

    /** 换用新的 Schedule。正在发声、但在新 Schedule 里已不存在或音高已改变的音符立即停止；
        其余音符沿用，并采用新的结束时间（新的结束时间已过则立即停止）。 */
    void setSchedule (const Schedule*, double currentBeat, int sampleOffset, MidiEventList& out);

    /** 回放 [from, to) 拍区间。

        @param sampleAtFrom  from 对应的块内采样位置（可以带小数，例如 TRIGGER 循环点）
        @param blockStart    事件位置的下限（含）
        @param blockEnd      事件位置的上限（不含）

        拍 b 上的事件落在"不早于 b 的第一个采样"上（带少量容差，抵消浮点累积误差）。 */
    void render (double from, double to, double sampleAtFrom, int blockStart, int blockEnd,
                 double samplesPerBeat, MidiEventList& out);

    /** 精确采样时间 → 采样序号（不早于该时间的第一个采样，带容差）。 */
    static int toSampleIndex (double exactSample) noexcept;

    /** 从 beat 处开始播放：补发跨越该位置、正在持续的音符（chase）。起点正好在 beat 的音符由 render 处理。 */
    void start (double beat, int sampleOffset, MidiEventList& out);

    /** 停止所有正在发声的音符。 */
    void stopAll (int sampleOffset, MidiEventList& out);

    bool hasActiveNotes() const noexcept { return ! active.empty(); }

    /** 音源实例的弯音状态不确定时（例如换了实例）：立即发送一次中心值，并且下一次求值总会发送。 */
    void resetWheel (int sampleOffset, MidiEventList& out);

    /** 弯音求值的间隔（采样）。 */
    static constexpr int bendInterval = 32;

private:
    struct Active
    {
        NoteId id;
        int pitch;
        double end;
        const ScheduledNote* note; ///< 指向当前 Schedule 中的数据（用于求弯音）
        std::uint64_t order;       ///< 开始的先后
    };

    struct Action
    {
        double beat;
        bool isStart;
        const ScheduledNote* note; // isStart 时有效
        NoteId id;
        int pitch;
    };

    void activate (const ScheduledNote&, double beat, int sampleOffset, MidiEventList& out);
    void deactivate (std::size_t activeIndex, int sampleOffset, MidiEventList& out);

    const Active* findWheelOwner() const noexcept;
    void sendWheel (int value, int sampleOffset, MidiEventList& out);
    int wheelValueAt (const ScheduledNote&, double beat) const noexcept;

    /** 在 [beatFrom, beatTo) 对应的采样上按间隔更新弯音。 */
    void renderBends (double beatFrom, double beatTo, double from, double sampleAtFrom,
                      int blockStart, int blockEnd, double samplesPerBeat, MidiEventList& out);

    const Schedule* schedule = nullptr;
    std::vector<Active> active;
    std::vector<Action> actions;
    int pitchCount[128] {};

    int voice = 0;
    double sampleRate = 44100.0;
    double secondsPerBeat = 0.5; ///< 最近一次 render 时的速度
    NoteId wheelOwner = 0;
    int lastWheel = 8192;
    std::uint64_t activationCounter = 0;
};

} // namespace curvable
