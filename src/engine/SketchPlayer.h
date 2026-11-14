#pragma once

#include "engine/Schedule.h"

#include <vector>

namespace curvable
{

/** 引擎输出的一个 MIDI 音符事件。 */
struct MidiNoteEvent
{
    int sampleOffset = 0; ///< 在当前音频块内的位置
    bool noteOn = false;
    int pitch = 60;
    int velocity = 0;     ///< note off 时为 0

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
*/
class SketchPlayer
{
public:
    SketchPlayer();

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

private:
    struct Active
    {
        NoteId id;
        int pitch;
        double end;
    };

    struct Action
    {
        double beat;
        bool isStart;
        const ScheduledNote* note; // isStart 时有效
        NoteId id;
        int pitch;
    };

    void activate (const ScheduledNote&, int sampleOffset, MidiEventList& out);
    void deactivate (std::size_t activeIndex, int sampleOffset, MidiEventList& out);

    const Schedule* schedule = nullptr;
    std::vector<Active> active;
    std::vector<Action> actions;
    int pitchCount[128] {};
};

} // namespace curvable
