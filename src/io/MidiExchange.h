#pragma once

#include "model/Project.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <vector>

namespace curvable::midi
{

/** 导出时音符怎样分到 MIDI 通道上。 */
enum class Layout
{
    channelPerVoice, ///< 每个声部一个通道（1..16），各带自己的弯音；弯音范围 = 工程的弯音范围。每个通道一条音轨
    mpe              ///< MPE（下区）：每个音符轮流占用通道 2..16，弯音范围 48；一条音轨
};

inline constexpr int ticksPerBeat = 960;
inline constexpr int bendStepTicks = 8;       ///< 弯音采样间隔（1/120 拍）
inline constexpr double mpeBendRange = 48.0;  ///< MPE 成员通道的默认弯音范围
inline constexpr int mpeMemberChannels = 15;

struct ExportResult
{
    juce::MidiFile file;
    int channels = 0;    ///< 用到的通道数
    int sharedNotes = 0; ///< 与弯音不同的音符共用了通道的音符数（通道不够用）
    int clippedNotes = 0; ///< 弯音超出弯音范围、被截断的音符数
};

/** 把工程导出成标准 MIDI 文件（格式 1，960 tick / 拍）。bpm 用于以 Hz 计的颤音速度和文件里的速度标记。 */
ExportResult exportProject (const Project&, Layout, double bpm);

struct ImportResult
{
    std::vector<Note> notes; ///< id 为 0；时间相对文件开头（拍），按起点排序
    int bentNotes = 0;       ///< 从弯音读出了曲线的音符数
    juce::String error;      ///< 无法读取时的原因
};

/** 读取 MIDI 文件里的音符，并把每个通道上的弯音换算成音符的曲线。
    弯音范围：文件里用 RPN 0 设置了的按文件；MPE 文件的成员通道默认 48；其余用 defaultBendRange。 */
ImportResult importMidi (const juce::MidiFile&, double defaultBendRange);
ImportResult importMidi (const juce::File&, double defaultBendRange);

/** 简化折线：去掉偏离前后连线不超过 tolerance（半音）的节点。首尾保留。 */
std::vector<CurvePoint> simplify (const std::vector<CurvePoint>&, double tolerance);

bool isMidiFile (const juce::File&);

} // namespace curvable::midi
