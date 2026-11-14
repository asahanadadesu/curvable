#pragma once

#include "engine/SketchPlayer.h"

namespace curvable
{

/** 一个音频块开始时的宿主走带信息。 */
struct HostTransport
{
    bool isPlaying = false;
    bool hasPosition = false; ///< 宿主是否提供了 PPQ 位置
    double ppq = 0.0;         ///< 块起点的位置（拍）
    double bpm = 120.0;
};

/** 宿主发来的音符（TRIGGER 模式下用作触发）。 */
struct HostNote
{
    int sampleOffset = 0;
    bool isOn = false;
    int pitch = 60;
};

/** 回放引擎：根据播放模式和宿主走带，决定 Schedule 的哪一段在什么时候播放。

    - AUTO：跟随宿主时间线。检测到播放头跳转（包括宿主循环回到起点）时停止所有音符，
      再从新位置开始，并补发跨越该位置的音符；宿主停止时停止所有音符。
    - TRIGGER：宿主的 note on 从对应 cue 的起点开始播放，播放到 cue 终点后回到起点循环；
      对应的 note off 停止播放。新的触发会替换正在播放的触发。不依赖宿主是否在播放。
    - OFF：不播放。

    纯 C++，只在音频线程上调用 process（prepare 除外），不分配内存。
*/
class PlaybackEngine
{
public:
    void prepare (double sampleRate);

    /** @param schedule  当前的 Schedule（可能每块都相同）；指针变化时视为内容更新 */
    void process (PlaybackMode mode, const Schedule* schedule, const HostTransport& transport,
                  int numSamples, const HostNote* hostNotes, int numHostNotes, MidiEventList& out);

    /** 停止一切（例如宿主调用 releaseResources 前）。 */
    void reset (MidiEventList& out);

    /** 测试与界面用：TRIGGER 模式下是否正在播放、当前位置。 */
    bool isTriggerActive() const noexcept { return trigger.active; }
    double getTriggerPosition() const noexcept { return trigger.position; }

private:
    void processAuto (const HostTransport&, int numSamples, double samplesPerBeat, MidiEventList&);
    void processTrigger (int numSamples, double samplesPerBeat, const HostNote*, int numHostNotes, MidiEventList&);
    void advanceTrigger (int fromSample, int toSample, double samplesPerBeat, MidiEventList&);
    void startTrigger (int pitch, int sampleOffset, MidiEventList&);
    void stopEverything (int sampleOffset, MidiEventList&);

    double sampleRate = 44100.0;
    SketchPlayer player;
    const Schedule* schedule = nullptr;
    PlaybackMode currentMode = PlaybackMode::off;

    // AUTO
    bool autoRunning = false;
    double expectedPpq = 0.0;

    // TRIGGER
    struct
    {
        bool active = false;
        int pitch = -1;
        double cueStart = 0.0;
        double cueEnd = 0.0;
        double position = 0.0;
    } trigger;
};

} // namespace curvable
