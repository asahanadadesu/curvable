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
    PitchCurve curve;   ///< 已 normalise
    int voice = 0;      ///< 分配到的声部（音源实例），见 VoiceAllocator
    bool overflow = false; ///< 声部不够用，与弯音不同的音符共用了声部（重叠期间弯音会互相影响）
    Envelope volume;    ///< 已夹住范围
    Envelope pan;

    /** 不弯音、音量和声像都不变：这样的音符可以随便共用一个音源实例。 */
    bool isPlain() const noexcept { return curve.isFlat() && volume.isFlat() && pan.isFlat(); }

    /** 弯音、音量、声像的曲线都一样。 */
    bool sameShape (const ScheduledNote& o) const noexcept { return curve == o.curve && volume == o.volume && pan == o.pan; }
};

/** 从 Project 编译出的不可变回放数据。在后台/消息线程创建，交给音频线程只读使用。 */
struct Schedule
{
    std::vector<ScheduledNote> notes; ///< 按 start 排序（同一时刻按音高）
    std::vector<TriggerCue> cues;
    double length = 0.0;              ///< 内容长度，向上取整到小节
    double bendRange = Project::defaultBendRange;
    int voiceCount = 1;    ///< 需要的声部（音源实例）数
    int overflowNotes = 0; ///< 超出声部上限、被迫共用声部的音符数

    static constexpr int maxVoices = 16;         ///< 声部（实例）上限
    static constexpr double releaseGuard = 1.0;  ///< 释放尾音保护时长（拍）

    /** 编译：丢弃无效音符（音高越界、时长 <= 0、非有限值），力度换算为 1..127。 */
    static Schedule compile (const Project&);

    const ScheduledNote* find (NoteId) const noexcept;

    /** 解析 cue 的实际区间：end <= start 时取内容长度；内容为空时退回一小节。 */
    void resolveCue (const TriggerCue&, double& start, double& end) const noexcept;
};

} // namespace curvable
