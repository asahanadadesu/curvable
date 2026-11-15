#include "io/MidiExchange.h"
#include "model/NoteEditing.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <set>

using namespace curvable;
using Catch::Matchers::WithinAbs;

namespace
{
    /** 写出再读回（经过真正的文件格式）。 */
    juce::MidiFile roundTripBytes (const juce::MidiFile& file)
    {
        juce::MemoryOutputStream out;
        REQUIRE (file.writeTo (out));
        juce::MemoryInputStream in (out.getData(), out.getDataSize(), false);
        juce::MidiFile read;
        REQUIRE (read.readFrom (in));
        return read;
    }

    std::set<int> noteChannels (const juce::MidiFile& file)
    {
        std::set<int> channels;

        for (int t = 0; t < file.getNumTracks(); ++t)
            for (const auto* e : *file.getTrack (t))
                if (e->message.isNoteOn())
                    channels.insert (e->message.getChannel());

        return channels;
    }

    /** 读回的音符与原来逐点比较音高（含曲线）。 */
    double maxPitchError (const Note& original, const Note& imported)
    {
        double worst = 0.0;

        for (double t = 0.0; t < original.length; t += 1.0 / 64.0)
            worst = std::max (worst, std::abs ((original.pitch + original.curve.evaluate (t))
                                               - (imported.pitch + imported.curve.evaluate (t))));

        return worst;
    }
}

TEST_CASE ("plain notes export and import unchanged", "[midi]")
{
    Project p;
    p.addNote (0.0, 1.0, 60, 0.8f);
    p.addNote (1.0, 0.5, 64, 0.5f);
    p.addNote (1.0, 2.0, 67, 1.0f);

    const auto exported = midi::exportProject (p, midi::Layout::channelPerVoice, 120.0);
    CHECK (exported.channels == 1); // 不弯的音符共用一个声部
    CHECK (exported.sharedNotes == 0);

    const auto imported = midi::importMidi (roundTripBytes (exported.file), 2.0);
    REQUIRE (imported.error.isEmpty());
    REQUIRE (imported.notes.size() == 3);
    CHECK (imported.bentNotes == 0);

    for (std::size_t i = 0; i < 3; ++i)
    {
        CHECK (imported.notes[i].start == p.notes[i].start);
        CHECK (imported.notes[i].length == p.notes[i].length);
        CHECK (imported.notes[i].pitch == p.notes[i].pitch);
        CHECK (imported.notes[i].curve.points.empty());
        CHECK_THAT (imported.notes[i].velocity, WithinAbs (p.notes[i].velocity, 0.01));
    }
}

TEST_CASE ("bent notes go to their own channels and come back as curves", "[midi]")
{
    Project p;
    p.bendRange = 12.0;
    const auto a = p.addNote (0.0, 2.0, 60, 0.8f);
    const auto b = p.addNote (0.0, 2.0, 64, 0.8f);
    p.addNote (0.0, 2.0, 67, 0.8f); // 不弯
    editing::findNote (p, a)->curve.points = { { 0.0, 0.0 }, { 1.0, 0.0 }, { 2.0, 5.0 } };
    editing::findNote (p, b)->curve.points = { { 0.0, -2.0, -0.5f }, { 0.5, 0.0 } };

    for (const auto layout : { midi::Layout::channelPerVoice, midi::Layout::mpe })
    {
        const auto exported = midi::exportProject (p, layout, 120.0);
        CHECK (exported.channels == 3);
        CHECK (exported.clippedNotes == 0);

        const auto file = roundTripBytes (exported.file);
        const auto channels = noteChannels (file);
        CHECK (channels.size() == 3);

        if (layout == midi::Layout::mpe)
            CHECK (channels.count (1) == 0); // 主通道不放音符

        // 读回时用一个错误的默认范围：文件里的 RPN 应该优先
        const auto imported = midi::importMidi (file, 2.0);
        REQUIRE (imported.notes.size() == 3);
        CHECK (imported.bentNotes == 2);

        for (const auto& original : p.notes)
        {
            const auto it = std::find_if (imported.notes.begin(), imported.notes.end(),
                                          [&] (const Note& n) { return n.pitch == original.pitch; });
            REQUIRE (it != imported.notes.end());
            CHECK (maxPitchError (original, *it) < 0.08);           // 采样 + 简化的误差
            CHECK (it->curve.points.size() < 40);                    // 简化过，不是每个采样一个节点
        }
    }
}

TEST_CASE ("MPE gives every overlapping note its own member channel", "[midi]")
{
    Project p;

    for (int i = 0; i < 15; ++i)
        p.addNote (0.0, 1.0, 48 + i, 0.8f);

    auto exported = midi::exportProject (p, midi::Layout::mpe, 120.0);
    CHECK (exported.sharedNotes == 0);
    CHECK (noteChannels (exported.file).size() == 15);

    p.addNote (0.5, 1.0, 80, 0.8f); // 第 16 个：只能共用
    exported = midi::exportProject (p, midi::Layout::mpe, 120.0);
    CHECK (exported.sharedNotes == 1);

    // MPE 配置消息（RPN 6）在主通道上
    bool mcm = false;
    int lastMsb = -1, lastLsb = -1;

    for (const auto* e : *exported.file.getTrack (1))
    {
        const auto& m = e->message;

        if (! m.isController() || m.getChannel() != 1)
            continue;

        if (m.getControllerNumber() == 101) lastMsb = m.getControllerValue();
        if (m.getControllerNumber() == 100) lastLsb = m.getControllerValue();
        if (m.getControllerNumber() == 6 && lastMsb == 0 && lastLsb == 6)
            mcm = m.getControllerValue() == 15;
    }

    CHECK (mcm);
}

TEST_CASE ("the wheel is set before the note starts and vibrato is rendered", "[midi]")
{
    Project p;
    p.bendRange = 2.0;
    const auto id = p.addNote (1.0, 2.0, 60, 0.8f);
    auto& c = editing::findNote (p, id)->curve;
    c.points = { { 0.0, -1.0 } };
    c.vibrato.wholeNote = true;
    c.vibrato.depth = 0.5;

    const auto exported = midi::exportProject (p, midi::Layout::channelPerVoice, 120.0);
    const auto& track = *exported.file.getTrack (1);

    // 同一 tick 上，弯音在 note on 之前
    int wheelIndex = -1, onIndex = -1;

    for (int i = 0; i < track.getNumEvents(); ++i)
    {
        const auto& m = track.getEventPointer (i)->message;

        if (m.isPitchWheel() && wheelIndex < 0)
            wheelIndex = i;
        if (m.isNoteOn() && onIndex < 0)
            onIndex = i;
    }

    REQUIRE (wheelIndex >= 0);
    REQUIRE (onIndex >= 0);
    CHECK (wheelIndex < onIndex);
    CHECK (track.getEventPointer (wheelIndex)->message.getTimeStamp() == midi::ticksPerBeat);
    CHECK (track.getEventPointer (wheelIndex)->message.getPitchWheelValue() == 4096); // -1 半音 / 范围 2

    // 颤音：弯音在 -1 上下摆动
    int wheels = 0;
    for (const auto* e : track)
        if (e->message.isPitchWheel())
            ++wheels;

    CHECK (wheels > 100);

    // 最后回到中心
    int lastWheel = 0;
    for (const auto* e : track)
        if (e->message.isPitchWheel())
            lastWheel = e->message.getPitchWheelValue();

    CHECK (lastWheel == 8192);
}

TEST_CASE ("curves outside the bend range are reported", "[midi]")
{
    Project p;
    p.bendRange = 2.0;
    const auto id = p.addNote (0.0, 1.0, 60, 0.8f);
    editing::findNote (p, id)->curve.points = { { 0.0, 0.0 }, { 1.0, 7.0 } };

    CHECK (midi::exportProject (p, midi::Layout::channelPerVoice, 120.0).clippedNotes == 1);
    CHECK (midi::exportProject (p, midi::Layout::mpe, 120.0).clippedNotes == 0); // MPE 的范围是 48
}

TEST_CASE ("files from other programs import with their pitch bends", "[midi]")
{
    // 一个普通的单通道文件：没有 RPN，弯音范围用默认值；弯音是几次跳变
    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::noteOn (3, 62, (juce::uint8) 100), 0.0);
    seq.addEvent (juce::MidiMessage::pitchWheel (3, 8192), 0.0);
    seq.addEvent (juce::MidiMessage::pitchWheel (3, 16383), 240.0); // 1/2 拍处跳到 +2
    seq.addEvent (juce::MidiMessage::noteOff (3, 62), 480.0);
    seq.addEvent (juce::MidiMessage::noteOn (3, 64, (juce::uint8) 64), 480.0); // 弯音保持 +2
    seq.addEvent (juce::MidiMessage::noteOff (3, 64), 960.0);
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (480);
    file.addTrack (seq);

    const auto imported = midi::importMidi (file, 2.0);
    REQUIRE (imported.notes.size() == 2);

    const auto& first = imported.notes[0];
    CHECK (first.start == 0.0);
    CHECK (first.length == 1.0);
    CHECK_THAT (first.curve.evaluate (0.25), WithinAbs (0.0, 1e-9));  // 跳变前保持
    CHECK_THAT (first.curve.evaluate (0.75), WithinAbs (2.0, 1e-9));  // 跳变后

    const auto& second = imported.notes[1];
    CHECK_THAT (second.curve.evaluate (0.0), WithinAbs (2.0, 1e-9));  // 开始时弯音已经在 +2
    CHECK (second.curve.points.size() == 1);

    // SMPTE 时间：拒绝
    juce::MidiFile smpte;
    smpte.setSmpteTimeFormat (25, 40);
    smpte.addTrack (seq);
    CHECK (midi::importMidi (smpte, 2.0).error.isNotEmpty());
}

TEST_CASE ("simplifying keeps corners and drops straight runs", "[midi]")
{
    std::vector<CurvePoint> line;
    for (int i = 0; i <= 100; ++i)
        line.push_back ({ i / 100.0, i < 50 ? 0.0 : (i - 50) / 10.0 });

    const auto simple = midi::simplify (line, 0.01);
    REQUIRE (simple.size() == 3);
    CHECK (simple[1].time == 0.5);
}
