#pragma once

#include "model/Project.h"

#include <vector>

namespace curvable
{

/** 编译后、可直接回放的音符。 */
struct ScheduledNote
{
    NoteId id = 0;
    double start = 0.0;
    double end = 0.0;
    int pitch = 60;
    int velocity = 100; ///< MIDI 力度 1..127
};

/** 从 Project 编译出的不可变回放数据。在后台/消息线程创建，交给音频线程只读使用。 */
struct Schedule
{
    std::vector<ScheduledNote> notes; ///< 按 start 排序（同一时刻按音高）
    std::vector<TriggerCue> cues;
    double length = 0.0;              ///< 内容长度，向上取整到小节

    /** 编译：丢弃无效音符（音高越界、时长 <= 0、非有限值），力度换算为 1..127。 */
    static Schedule compile (const Project&);

    const ScheduledNote* find (NoteId) const noexcept;

    /** 解析 cue 的实际区间：end <= start 时取内容长度；内容为空时退回一小节。 */
    void resolveCue (const TriggerCue&, double& start, double& end) const noexcept;
};

} // namespace curvable
