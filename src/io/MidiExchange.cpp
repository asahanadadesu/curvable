#include "io/MidiExchange.h"
#include "engine/PitchBend.h"
#include "engine/Schedule.h"
#include "model/NoteEditing.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace curvable::midi
{

namespace
{
    int toTicks (double beats) { return (int) std::lround (beats * ticksPerBeat); }

    /** 同一时刻的事件顺序：先 note off，再控制器 / RPN，再弯音，最后 note on（滑入的音先到位再发声）。 */
    struct Event
    {
        int tick = 0;
        int order = 0;
        juce::MidiMessage message;
    };

    enum Order { offOrder = 0, controlOrder = 1, wheelOrder = 2, onOrder = 3 };

    void addRpn (std::vector<Event>& events, int channel, int msb, int lsb, int valueMsb, int valueLsb)
    {
        auto cc = [&] (int number, int value)
        {
            events.push_back ({ 0, controlOrder, juce::MidiMessage::controllerEvent (channel, number, value) });
        };

        cc (101, msb);
        cc (100, lsb);
        cc (6, valueMsb);
        cc (38, valueLsb);
        cc (101, 127); // RPN null：避免之后的数据输入误改
        cc (100, 127);
    }

    void addBendRange (std::vector<Event>& events, int channel, double range)
    {
        const auto semitones = juce::jlimit (0, 127, (int) std::floor (range));
        const auto cents = juce::jlimit (0, 99, (int) std::lround ((range - std::floor (range)) * 100.0));
        addRpn (events, channel, 0, 0, semitones, cents);
    }

    struct OutNote
    {
        const ScheduledNote* note = nullptr;
        int channel = 1;
        int offTick = 0;
    };

    /** 一个通道上的音符和弯音。弯音跟随这个通道上最近开始的音符（与回放时相同）。 */
    void writeChannel (std::vector<Event>& events, std::vector<OutNote>& notes, double range, double spb, int& clipped)
    {
        std::sort (notes.begin(), notes.end(), [] (const OutNote& a, const OutNote& b) { return a.note->start < b.note->start; });

        int wheel = 8192;
        double lastEnd = 0.0;

        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            const auto& n = *notes[i].note;
            const auto channel = notes[i].channel;
            const auto startTick = toTicks (n.start);
            auto endTick = std::max (startTick + 1, toTicks (n.end));

            // 同一通道上同音高的音符重叠：前一个在后一个开始时结束，避免后一个的 note off 被提前匹配
            for (std::size_t j = i + 1; j < notes.size(); ++j)
                if (notes[j].note->pitch == n.pitch && toTicks (notes[j].note->start) < endTick)
                {
                    endTick = std::max (startTick + 1, toTicks (notes[j].note->start));
                    break;
                }

            notes[i].offTick = endTick;
            events.push_back ({ startTick, onOrder, juce::MidiMessage::noteOn (channel, n.pitch, (juce::uint8) n.velocity) });
            events.push_back ({ endTick, offOrder, juce::MidiMessage::noteOff (channel, n.pitch) });

            // 这个音符掌管弯音的区间：到它结束，或者到下一个音符开始
            auto ownerEnd = toTicks (n.end);
            if (i + 1 < notes.size())
                ownerEnd = std::min (ownerEnd, toTicks (notes[i + 1].note->start));

            const auto length = n.end - n.start;
            bool noteClipped = false;

            auto sample = [&] (int tick)
            {
                const auto t = juce::jlimit (0.0, length, (double) tick / ticksPerBeat - n.start);
                const auto bend = centsToPitchBend (n.curve.evaluate (t, length, spb) * 100.0, range);
                noteClipped = noteClipped || bend.clipped;

                if (bend.value != wheel)
                {
                    wheel = bend.value;
                    events.push_back ({ tick, wheelOrder, juce::MidiMessage::pitchWheel (channel, wheel) });
                }
            };

            sample (startTick);

            if (! n.curve.isFlat())
            {
                for (auto tick = startTick + bendStepTicks; tick < ownerEnd; tick += bendStepTicks)
                    sample (tick);

                // 曲线的终点（例如 fall 正好在音符结尾到达）
                if (ownerEnd == toTicks (n.end) && ownerEnd - 1 > startTick)
                    sample (ownerEnd - 1);
            }

            if (noteClipped)
                ++clipped;

            lastEnd = std::max (lastEnd, n.end);
        }

        // 全部结束后（留出释放的时间）回到中心
        if (wheel != 8192 && ! notes.empty())
            events.push_back ({ toTicks (lastEnd + Schedule::releaseGuard), wheelOrder,
                                juce::MidiMessage::pitchWheel (notes.front().channel, 8192) });
    }

    juce::MidiMessageSequence toSequence (std::vector<Event>& events, const juce::String& name)
    {
        std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b)
        {
            return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
        });

        juce::MidiMessageSequence seq;

        if (name.isNotEmpty())
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, name), 0.0);

        for (auto& e : events)
            seq.addEvent (e.message, (double) e.tick);

        seq.updateMatchedPairs();
        return seq;
    }
}

//==============================================================================
ExportResult exportProject (const Project& project, Layout layout, double bpm)
{
    bpm = std::isfinite (bpm) && bpm > 0.0 ? bpm : 120.0;
    const auto spb = 60.0 / bpm;
    const auto schedule = Schedule::compile (project);

    ExportResult result;
    result.file.setTicksPerQuarterNote (ticksPerBeat);

    juce::MidiMessageSequence tempo;
    tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60.0e6 / bpm)), 0.0);
    tempo.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    result.file.addTrack (tempo);

    if (layout == Layout::channelPerVoice)
    {
        std::map<int, std::vector<OutNote>> byChannel;

        for (const auto& n : schedule.notes)
        {
            byChannel[juce::jlimit (0, 15, n.voice) + 1].push_back ({ &n, juce::jlimit (0, 15, n.voice) + 1 });

            if (n.overflow)
                ++result.sharedNotes;
        }

        for (auto& [channel, notes] : byChannel)
        {
            std::vector<Event> events;
            addBendRange (events, channel, schedule.bendRange);
            writeChannel (events, notes, schedule.bendRange, spb, result.clippedNotes);
            result.file.addTrack (toSequence (events, "Curvable voice " + juce::String (channel)));
        }

        result.channels = (int) byChannel.size();
        return result;
    }

    // MPE：每个音符占一个成员通道，优先用空闲最久的通道（释放尾音不会被下一个音符的弯音带走）
    std::array<double, mpeMemberChannels> freeAt {};
    freeAt.fill (-1.0e9);
    std::array<std::vector<OutNote>, mpeMemberChannels> byChannel;

    for (const auto& n : schedule.notes)
    {
        const auto index = (int) (std::min_element (freeAt.begin(), freeAt.end()) - freeAt.begin());

        if (freeAt[(size_t) index] > n.start + 1.0e-9)
            ++result.sharedNotes;

        freeAt[(size_t) index] = std::max (freeAt[(size_t) index], n.end);
        byChannel[(size_t) index].push_back ({ &n, index + 2 });
    }

    std::vector<Event> events;
    addRpn (events, 1, 0, 6, mpeMemberChannels, 0); // MPE Configuration Message：下区，15 个成员通道

    for (int i = 0; i < mpeMemberChannels; ++i)
    {
        if (byChannel[(size_t) i].empty())
            continue;

        addBendRange (events, i + 2, mpeBendRange);
        writeChannel (events, byChannel[(size_t) i], mpeBendRange, spb, result.clippedNotes);
        ++result.channels;
    }

    result.file.addTrack (toSequence (events, "Curvable MPE"));
    return result;
}

//==============================================================================
std::vector<CurvePoint> simplify (const std::vector<CurvePoint>& points, double tolerance)
{
    if (points.size() <= 2)
        return points;

    std::vector<bool> keep (points.size(), false);
    keep.front() = keep.back() = true;

    // Douglas–Peucker，距离按竖直方向（半音）计算
    std::vector<std::pair<std::size_t, std::size_t>> stack { { 0, points.size() - 1 } };

    while (! stack.empty())
    {
        const auto [a, b] = stack.back();
        stack.pop_back();

        const auto& pa = points[a];
        const auto& pb = points[b];
        double worst = -1.0;
        std::size_t worstIndex = a;

        for (auto i = a + 1; i < b; ++i)
        {
            const auto span = pb.time - pa.time;
            const auto expected = span > 0.0 ? pa.semitones + (pb.semitones - pa.semitones) * (points[i].time - pa.time) / span
                                             : pa.semitones;
            const auto d = std::abs (points[i].semitones - expected);

            if (d > worst)
            {
                worst = d;
                worstIndex = i;
            }
        }

        if (worst > tolerance)
        {
            keep[worstIndex] = true;
            stack.push_back ({ a, worstIndex });
            stack.push_back ({ worstIndex, b });
        }
    }

    std::vector<CurvePoint> result;

    for (std::size_t i = 0; i < points.size(); ++i)
        if (keep[i])
            result.push_back (points[i]);

    return result;
}

//==============================================================================
ImportResult importMidi (const juce::MidiFile& source, double defaultBendRange)
{
    ImportResult result;
    const auto tpq = source.getTimeFormat();

    if (tpq <= 0)
    {
        result.error = "This MIDI file uses SMPTE timing, which is not supported.";
        return result;
    }

    juce::MidiMessageSequence all;

    for (int i = 0; i < source.getNumTracks(); ++i)
        all.addSequence (*source.getTrack (i), 0.0);

    all.updateMatchedPairs();

    // 每个通道的弯音范围（RPN 0）和弯音变化
    struct Wheel { double beat; double semitones; };
    std::array<double, 17> range {};
    std::array<bool, 17> rangeSet {};
    std::array<int, 17> rpnMsb {}, rpnLsb {};
    std::array<std::vector<Wheel>, 17> wheels;
    range.fill (defaultBendRange);
    rpnMsb.fill (127);
    rpnLsb.fill (127);

    struct RawNote { double start, end; int pitch, velocity, channel; };
    std::vector<RawNote> raw;

    for (int i = 0; i < all.getNumEvents(); ++i)
    {
        const auto* holder = all.getEventPointer (i);
        const auto& m = holder->message;
        const auto beat = m.getTimeStamp() / tpq;
        const auto ch = m.getChannel();

        if (ch < 1 || ch > 16)
            continue;

        if (m.isController())
        {
            const auto number = m.getControllerNumber();
            const auto value = m.getControllerValue();

            if (number == 101) rpnMsb[(size_t) ch] = value;
            if (number == 100) rpnLsb[(size_t) ch] = value;

            if ((number == 6 || number == 38) && rpnMsb[(size_t) ch] == 0)
            {
                if (rpnLsb[(size_t) ch] == 0)
                {
                    // 弯音范围：CC 6 = 半音，CC 38 = 音分
                    const auto whole = number == 6 ? (double) value : std::floor (range[(size_t) ch]);
                    const auto cents = number == 38 ? value / 100.0 : 0.0;
                    range[(size_t) ch] = std::max (0.01, whole + cents);
                    rangeSet[(size_t) ch] = true;
                }
                else if (rpnLsb[(size_t) ch] == 6 && number == 6 && (ch == 1 || ch == 16) && value > 0)
                {
                    // MPE Configuration Message：成员通道默认弯音范围 48
                    const auto first = ch == 1 ? 2 : 16 - value;
                    const auto last = ch == 1 ? 1 + value : 15;

                    for (int c = juce::jmax (1, first); c <= juce::jmin (16, last); ++c)
                        if (! rangeSet[(size_t) c])
                            range[(size_t) c] = mpeBendRange;
                }
            }
        }
        else if (m.isPitchWheel())
        {
            const auto v = m.getPitchWheelValue() - 8192;
            const auto normalised = v >= 0 ? v / 8191.0 : v / 8192.0;
            wheels[(size_t) ch].push_back ({ beat, normalised * range[(size_t) ch] });
        }
        else if (m.isNoteOn())
        {
            double end = beat + 0.25; // 没有 note off 的音符给一个默认长度

            if (holder->noteOffObject != nullptr)
                end = holder->noteOffObject->message.getTimeStamp() / tpq;

            raw.push_back ({ beat, std::max (end, beat + editing::minNoteLength), m.getNoteNumber(), m.getVelocity(), ch });
        }
    }

    // 弯音 → 曲线：弯音是阶梯式的，相隔较远的两次变化之间补一个"保持"节点，再简化
    for (const auto& r : raw)
    {
        Note n;
        n.start = r.start;
        n.length = r.end - r.start;
        n.pitch = juce::jlimit (0, 127, r.pitch);
        n.velocity = juce::jlimit (1, 127, r.velocity) / 127.0f;

        const auto& list = wheels[(size_t) r.channel];
        double current = 0.0;
        std::size_t i = 0;

        while (i < list.size() && list[i].beat <= r.start + 1.0e-9)
            current = list[i++].semitones;

        std::vector<CurvePoint> points { { 0.0, current } };
        constexpr double holdGap = 1.0 / 32.0;

        for (; i < list.size() && list[i].beat < r.end - 1.0e-9; ++i)
        {
            const auto t = list[i].beat - r.start;

            if (t - points.back().time > holdGap)
                points.push_back ({ t, current });

            current = list[i].semitones;
            points.push_back ({ t, current });
        }

        if (points.size() > 1 && n.length - points.back().time > 1.0e-9)
            points.push_back ({ n.length, current });

        const auto bent = std::any_of (points.begin(), points.end(), [] (const CurvePoint& p) { return std::abs (p.semitones) > 1.0e-3; });

        if (bent)
        {
            n.curve.points = simplify (points, 0.03);

            // 只剩一个水平的终点：去掉，第一个节点就够了
            if (n.curve.points.size() == 2 && std::abs (n.curve.points[0].semitones - n.curve.points[1].semitones) < 1.0e-9)
                n.curve.points.pop_back();

            n.curve.normalise();
            ++result.bentNotes;
        }

        result.notes.push_back (std::move (n));
    }

    std::stable_sort (result.notes.begin(), result.notes.end(), [] (const Note& a, const Note& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });

    if (result.notes.empty())
        result.error = "There are no notes in this MIDI file.";

    return result;
}

ImportResult importMidi (const juce::File& file, double defaultBendRange)
{
    juce::FileInputStream stream (file);
    juce::MidiFile midi;

    if (! stream.openedOk() || ! midi.readFrom (stream))
    {
        ImportResult result;
        result.error = "Could not read " + file.getFileName() + " as a MIDI file.";
        return result;
    }

    return importMidi (midi, defaultBendRange);
}

bool isMidiFile (const juce::File& file)
{
    return file.hasFileExtension ("mid;midi;smf");
}

} // namespace curvable::midi
