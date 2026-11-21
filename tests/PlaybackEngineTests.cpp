#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

#include <random>

using namespace curvable;
using namespace curvable::test;

namespace
{
    Schedule phrase() { return Schedule::compile (Project::createTestPhrase()); }

    Schedule single (double start, double length, int pitch = 60)
    {
        Project p;
        p.addNote (start, length, pitch, 1.0f);
        return Schedule::compile (p);
    }
}

//==============================================================================
TEST_CASE ("AUTO: notes land on the exact sample of their beat", "[engine][auto]")
{
    EngineRunner r (single (1.0, 0.5));
    r.playContinuously (0.0, beatToSample (2.0), { 512 });

    REQUIRE (r.events.size() == 2);
    CHECK (r.events[0] == AbsEvent { beatToSample (1.0), true, 60 });
    CHECK (r.events[1] == AbsEvent { beatToSample (1.5), false, 60 });
}

TEST_CASE ("AUTO: output doesn't depend on how the host slices blocks", "[engine][auto]")
{
    // FL 会把音频切成大小不一的小块；结果必须与固定块大小完全相同
    EngineRunner fixed (phrase());
    fixed.playContinuously (0.0, beatToSample (8.0), { 512 });

    std::mt19937 rng (1234);
    std::uniform_int_distribution<int> size (1, 700);
    std::vector<int> sizes;
    for (int i = 0; i < 1000; ++i)
        sizes.push_back (size (rng));

    EngineRunner sliced (phrase());
    sliced.playContinuously (0.0, beatToSample (8.0), sizes);

    CHECK (sliced.events.size() == fixed.events.size());

    // 允许 1 个采样的取整差异
    for (std::size_t i = 0; i < std::min (fixed.events.size(), sliced.events.size()); ++i)
    {
        CHECK (sliced.events[i].noteOn == fixed.events[i].noteOn);
        CHECK (sliced.events[i].pitch == fixed.events[i].pitch);
        CHECK (std::abs (sliced.events[i].sample - fixed.events[i].sample) <= 1);
    }
}

TEST_CASE ("AUTO: the full test phrase plays every note once and leaves nothing hanging", "[engine][auto]")
{
    EngineRunner r (phrase());
    r.playContinuously (0.0, beatToSample (9.0), { 441 });

    int ons = 0;
    for (const auto& e : r.events)
        ons += e.noteOn ? 1 : 0;

    CHECK (ons == 22);
    CHECK (r.soundingPitches().empty());
    CHECK (r.isChronological());
}

TEST_CASE ("AUTO: stopping the host silences sounding notes", "[engine][auto]")
{
    EngineRunner r (single (0.0, 4.0, 48));
    r.playContinuously (0.0, beatToSample (1.0), { 512 });
    REQUIRE (r.soundingPitches() == std::vector<int> { 48 });

    r.block (512, { false, true, 1.0, 120.0 });
    CHECK (r.soundingPitches().empty());

    // 停止后不再产生事件
    const auto count = r.events.size();
    r.block (512, { false, true, 1.0, 120.0 });
    CHECK (r.events.size() == count);
}

TEST_CASE ("AUTO: jumping the playhead chases notes at the new position", "[engine][auto]")
{
    Project p;
    p.addNote (0.0, 1.0, 60, 1.0f);
    p.addNote (4.0, 4.0, 48, 1.0f);
    EngineRunner r (Schedule::compile (p));

    r.playContinuously (0.0, beatToSample (0.5), { 512 }); // 60 在响
    REQUIRE (r.soundingPitches() == std::vector<int> { 60 });

    const auto jumpAt = r.position;
    r.block (512, { true, true, 6.0, 120.0 }); // 跳到第 6 拍：60 停止，48 补发

    CHECK (r.events[r.events.size() - 2] == AbsEvent { jumpAt, false, 60 });
    CHECK (r.events.back() == AbsEvent { jumpAt, true, 48 });
    CHECK (r.soundingPitches() == std::vector<int> { 48 });
}

TEST_CASE ("AUTO: host loop back to the start restarts cleanly", "[engine][auto]")
{
    // 音符一直持续到循环终点：回到起点时必须先停止，再重新开始
    EngineRunner r (single (0.0, 8.0, 48));

    r.playContinuously (0.0, beatToSample (8.0), { 480 });
    REQUIRE (r.onsFor (48).size() == 1);

    r.playContinuously (0.0, beatToSample (1.0), { 480 }); // 宿主循环回到 0

    CHECK (r.onsFor (48).size() == 2);
    CHECK (r.soundingPitches() == std::vector<int> { 48 });
    CHECK (r.isChronological());

    // 两次 note on 之间必须有一次 note off
    int offsBetween = 0;
    const auto first = r.onsFor (48)[0].sample, second = r.onsFor (48)[1].sample;
    for (const auto& e : r.events)
        if (! e.noteOn && e.pitch == 48 && e.sample >= first && e.sample <= second)
            ++offsBetween;
    CHECK (offsBetween == 1);
}

TEST_CASE ("AUTO: host notes are ignored by the engine", "[engine][auto]")
{
    EngineRunner r (single (10.0, 1.0));
    r.block (512, { true, true, 0.0, 120.0 }, PlaybackMode::automatic, { { 0, true, 64 } });
    CHECK (r.events.empty());
}

TEST_CASE ("AUTO: tempo changes keep notes on their beats", "[engine][auto]")
{
    EngineRunner r (single (2.0, 1.0));
    // 前 1 拍 120 BPM（24000 采样），之后 60 BPM（每拍 48000 采样）
    r.block (24000, { true, true, 0.0, 120.0 });

    double ppq = 1.0;
    for (int i = 0; i < 100; ++i)
    {
        r.block (960, { true, true, ppq, 60.0 });
        ppq += 960.0 / 48000.0;
    }

    REQUIRE (! r.onsFor (60).empty());
    CHECK (r.onsFor (60)[0].sample == 24000 + 48000);
}

//==============================================================================
TEST_CASE ("TRIGGER: a host note starts playback from the cue start at that sample", "[engine][trigger]")
{
    EngineRunner r (single (0.0, 0.5));
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 100, true, 36 } });

    REQUIRE (! r.events.empty());
    CHECK (r.events[0] == AbsEvent { 100, true, 60 });
    CHECK (r.engine.isTriggerActive());
}

TEST_CASE ("TRIGGER: loops while held and stops on note off", "[engine][trigger]")
{
    EngineRunner r (single (0.0, 0.5)); // 内容一小节（4 拍）
    const auto bar = beatToSample (4.0);

    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, true, 36 } });

    for (long long done = 512; done < bar * 2 + 1000; done += 512)
        r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger);

    const auto ons = r.onsFor (60);
    REQUIRE (ons.size() == 3); // 0、1 小节、2 小节
    CHECK (ons[1].sample == bar);
    CHECK (ons[2].sample == bar * 2);

    // 松开触发音符：停止
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 10, false, 36 } });
    CHECK_FALSE (r.engine.isTriggerActive());
    CHECK (r.soundingPitches().empty());
}

TEST_CASE ("TRIGGER: releasing the trigger mid-note silences it", "[engine][trigger]")
{
    EngineRunner r (single (0.0, 4.0, 48));
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, true, 36 } });
    REQUIRE (r.soundingPitches() == std::vector<int> { 48 });

    // 其他键的 note off 不影响
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 5, false, 37 } });
    CHECK (r.soundingPitches() == std::vector<int> { 48 });

    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 5, false, 36 } });
    CHECK (r.soundingPitches().empty());
    CHECK (r.events.back() == AbsEvent { 1024 + 5, false, 48 });
}

TEST_CASE ("TRIGGER: cues with their own keys play their own sections", "[engine][trigger]")
{
    Project p;
    p.addNote (0.0, 1.0, 60, 1.0f);
    p.addNote (4.0, 1.0, 67, 1.0f);
    p.cues = { { 36, 0.0, 4.0 }, { 38, 4.0, 8.0 } };
    EngineRunner r (Schedule::compile (p));

    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, true, 38 } });
    REQUIRE (! r.events.empty());
    CHECK (r.events[0] == AbsEvent { 0, true, 67 });

    // 没有对应 cue、也没有"任意键" cue 的键：忽略
    const auto count = r.events.size();
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, true, 40 } });
    CHECK (r.events.size() == count);
}

TEST_CASE ("TRIGGER: a new trigger replaces the running one", "[engine][trigger]")
{
    EngineRunner r (single (0.0, 4.0, 48));
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, true, 36 } });
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 20, true, 37 } });

    // 先停止，再从头开始
    const auto& e = r.events;
    REQUIRE (e.size() == 3);
    CHECK (e[1] == AbsEvent { 512 + 20, false, 48 });
    CHECK (e[2] == AbsEvent { 512 + 20, true, 48 });

    // 旧触发键的 note off 不再起作用
    r.block (512, { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, false, 36 } });
    CHECK (r.soundingPitches() == std::vector<int> { 48 });
}

TEST_CASE ("TRIGGER: output doesn't depend on block slicing", "[engine][trigger]")
{
    auto run = [] (const std::vector<int>& sizes)
    {
        EngineRunner r (phrase());
        r.block (sizes[0], { false, false, 0.0, 120.0 }, PlaybackMode::trigger, { { 0, true, 36 } });

        long long done = sizes[0];
        std::size_t i = 1;
        while (done < beatToSample (20.0))
        {
            const auto n = sizes[i++ % sizes.size()];
            r.block (n, { false, false, 0.0, 120.0 }, PlaybackMode::trigger);
            done += n;
        }
        return r.events;
    };

    const auto fixed = run ({ 512 });
    const auto sliced = run ({ 37, 512, 1, 300, 999, 64 });

    REQUIRE (fixed.size() == sliced.size());
    for (std::size_t i = 0; i < fixed.size(); ++i)
    {
        CHECK (fixed[i].noteOn == sliced[i].noteOn);
        CHECK (fixed[i].pitch == sliced[i].pitch);
        CHECK (std::abs (fixed[i].sample - sliced[i].sample) <= 1);
    }
}

//==============================================================================
TEST_CASE ("switching mode silences everything", "[engine]")
{
    EngineRunner r (single (0.0, 4.0, 48));
    r.block (512, { true, true, 0.0, 120.0 }, PlaybackMode::automatic);
    REQUIRE (r.soundingPitches() == std::vector<int> { 48 });

    r.block (512, { true, true, 512 / EngineRunner::samplesPerBeat, 120.0 }, PlaybackMode::off);
    CHECK (r.soundingPitches().empty());
}

TEST_CASE ("editing while playing stops removed notes", "[engine]")
{
    Project p;
    p.addNote (0.0, 4.0, 48, 1.0f);
    const auto removed = p.addNote (0.0, 4.0, 60, 1.0f);

    EngineRunner r (Schedule::compile (p));
    r.playContinuously (0.0, 512, { 512 });
    REQUIRE (r.soundingPitches().size() == 2);

    std::erase_if (p.notes, [&] (const Note& n) { return n.id == removed; });
    // 引擎以指针变化识别内容更新（与 ScheduleExchange 的行为一致）
    Schedule updated = Schedule::compile (p);
    MidiEventList out;
    r.engine.process (PlaybackMode::automatic, &updated, { true, true, 512 / EngineRunner::samplesPerBeat, 120.0 },
                      512, nullptr, 0, out);

    bool stopped60 = false;
    for (const auto& e : out.get())
        stopped60 = stopped60 || (! e.noteOn && e.pitch == 60);

    CHECK (stopped60);
}

TEST_CASE ("missing schedule or tempo doesn't crash", "[engine]")
{
    PlaybackEngine engine;
    engine.prepare (44100.0);
    MidiEventList out;
    HostNote on { 0, true, 60 };

    engine.process (PlaybackMode::automatic, nullptr, { true, true, 0.0, 0.0 }, 512, nullptr, 0, out);
    engine.process (PlaybackMode::trigger, nullptr, { false, false, 0.0, -5.0 }, 512, &on, 1, out);
    engine.process (PlaybackMode::trigger, nullptr, {}, 0, nullptr, 0, out);
    CHECK (out.get().empty());
}

//==============================================================================
TEST_CASE ("internal playback plays with the host stopped, to the end of the content", "[engine][internal]")
{
    EngineRunner r (single (1.0, 0.5));
    r.engine.startInternal (0.0, 0.0, 0.0);

    // 宿主停着：一样从 0 拍开始播，音符落在准确的采样上
    for (int i = 0; i < 400; ++i)
        r.block (480, { false, true, 0.0, EngineRunner::bpm });

    REQUIRE (r.events.size() == 2);
    CHECK (r.events[0] == AbsEvent { beatToSample (1.0), true, 60 });
    CHECK (r.events[1] == AbsEvent { beatToSample (1.5), false, 60 });

    // 内容是一小节：播完就停
    CHECK_FALSE (r.engine.isInternalActive());
}

TEST_CASE ("internal playback loops inside the loop region with sample accuracy", "[engine][internal]")
{
    EngineRunner r (single (0.5, 0.25));
    r.engine.startInternal (3.0, 0.0, 1.0); // 起点不在循环区里：从循环区开头开始

    std::mt19937 rng (7);
    std::uniform_int_distribution<int> size (1, 700);
    long long done = 0;

    while (done < beatToSample (3.0))
    {
        const auto n = size (rng);
        r.block (n, { false, false, 0.0, EngineRunner::bpm }, PlaybackMode::off);
        done += n;
    }

    // 每一拍（循环一次）响一次，位置准确到 1 个采样
    const auto ons = r.onsFor (60);
    REQUIRE (ons.size() == 3);

    for (int i = 0; i < 3; ++i)
        CHECK (std::abs (ons[(size_t) i].sample - beatToSample (i + 0.5)) <= 1);

    CHECK (r.engine.isInternalActive());
}

TEST_CASE ("internal playback steps aside when the host takes over", "[engine][internal]")
{
    SECTION ("AUTO: the host starts playing")
    {
        EngineRunner r (single (0.0, 4.0, 64));
        r.engine.startInternal (0.0, 0.0, 0.0);
        r.block (480, { false, true, 0.0, EngineRunner::bpm });
        REQUIRE (r.engine.isInternalActive());
        REQUIRE (r.soundingPitches() == std::vector<int> { 64 });

        r.block (480, { true, true, 2.0, EngineRunner::bpm });
        CHECK_FALSE (r.engine.isInternalActive());
        CHECK (r.onsFor (64).size() == 2); // 停下插件内播放，再从宿主位置补发
        CHECK (r.isChronological());
    }

    SECTION ("TRIGGER: a trigger note arrives")
    {
        EngineRunner r (single (0.0, 4.0, 64));
        r.engine.startInternal (0.0, 0.0, 0.0);
        r.block (480, { false, false, 0.0, EngineRunner::bpm }, PlaybackMode::trigger);
        REQUIRE (r.engine.isInternalActive());

        r.block (480, { false, false, 0.0, EngineRunner::bpm }, PlaybackMode::trigger, { { 10, true, 60 } });
        CHECK_FALSE (r.engine.isInternalActive());
        CHECK (r.engine.isTriggerActive());
    }

    SECTION ("stopping leaves nothing hanging")
    {
        EngineRunner r (single (0.0, 4.0, 64));
        r.engine.startInternal (0.0, 0.0, 0.0);
        r.block (480, { false, true, 0.0, EngineRunner::bpm });

        MidiEventList out;
        r.engine.stopInternal (out);
        for (const auto& e : out.get())
            if (! e.isPitchWheel && ! e.isLevel)
                r.events.push_back ({ r.position + e.sampleOffset, e.noteOn, e.pitch });

        CHECK (r.soundingPitches().empty());
        CHECK_FALSE (r.engine.isInternalActive());
    }
}
