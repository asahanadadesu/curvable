#include "model/ProjectSerializer.h"

#include <catch2/catch_test_macros.hpp>

using namespace curvable;

TEST_CASE ("project survives a round trip through XML", "[serializer]")
{
    auto p = Project::createTestPhrase();
    p.mode = PlaybackMode::trigger;
    p.cues = { { 36, 0.0, 4.0 }, { 38, 4.0, 8.0 } };
    p.notes[3].id = 0x7fffffffffffull; // 大 id 也要原样保存
    p.nextId = 0x800000000000ull;

    const auto xml = ProjectSerializer::toValueTree (p).createXml();
    REQUIRE (xml != nullptr);

    const auto restored = ProjectSerializer::fromValueTree (juce::ValueTree::fromXml (*xml));
    REQUIRE (restored.has_value());

    CHECK (restored->mode == PlaybackMode::trigger);
    CHECK (restored->nextId == p.nextId);
    REQUIRE (restored->notes.size() == p.notes.size());

    for (std::size_t i = 0; i < p.notes.size(); ++i)
    {
        CHECK (restored->notes[i].id == p.notes[i].id);
        CHECK (restored->notes[i].start == p.notes[i].start);
        CHECK (restored->notes[i].length == p.notes[i].length);
        CHECK (restored->notes[i].pitch == p.notes[i].pitch);
        CHECK (restored->notes[i].velocity == p.notes[i].velocity);
    }

    REQUIRE (restored->cues.size() == 2);
    CHECK (restored->cues[1].key == 38);
    CHECK (restored->cues[1].start == 4.0);
    CHECK (restored->cues[1].end == 8.0);
}

TEST_CASE ("missing values fall back to defaults and ids are repaired", "[serializer]")
{
    juce::ValueTree tree (ProjectSerializer::type);
    tree.appendChild (juce::ValueTree ("Note").setProperty ("id", "5", nullptr).setProperty ("pitch", 64, nullptr), nullptr);
    tree.appendChild (juce::ValueTree ("Note").setProperty ("id", "5", nullptr), nullptr); // 重复 id
    tree.appendChild (juce::ValueTree ("Note"), nullptr);                                 // 没有 id
    tree.appendChild (juce::ValueTree ("SomethingFromTheFuture"), nullptr);                // 未知节点

    const auto p = ProjectSerializer::fromValueTree (tree);
    REQUIRE (p.has_value());

    CHECK (p->mode == PlaybackMode::automatic);
    REQUIRE (p->notes.size() == 3);
    CHECK (p->notes[0].pitch == 64);
    CHECK (p->notes[1].length == 1.0);
    CHECK (p->notes[0].id != p->notes[1].id);
    CHECK (p->notes[1].id != p->notes[2].id);
    CHECK (p->notes[2].id != 0);
    CHECK (p->nextId > std::max ({ p->notes[0].id, p->notes[1].id, p->notes[2].id }));
    REQUIRE (p->cues.size() == 1); // 默认一个"任意键" cue
    CHECK (p->cues[0].key == TriggerCue::anyKey);
}

TEST_CASE ("wrong type or newer version is rejected", "[serializer]")
{
    CHECK_FALSE (ProjectSerializer::fromValueTree (juce::ValueTree ("Nope")).has_value());

    juce::ValueTree future (ProjectSerializer::type);
    future.setProperty ("version", ProjectSerializer::currentVersion + 1, nullptr);
    CHECK_FALSE (ProjectSerializer::fromValueTree (future).has_value());
}

TEST_CASE ("the loop region is saved with the project", "[serializer]")
{
    Project p;
    p.loopStart = 4.0;
    p.loopEnd = 12.0;
    p.loopOn = true;

    auto restored = ProjectSerializer::fromValueTree (ProjectSerializer::toValueTree (p));
    REQUIRE (restored.has_value());
    CHECK (restored->hasLoop());
    CHECK (restored->loopStart == 4.0);
    CHECK (restored->loopEnd == 12.0);
    CHECK (restored->loopOn);

    // 没有循环区的旧工程、无效的值：没有循环区
    auto tree = ProjectSerializer::toValueTree (Project {});
    tree.setProperty ("loopStart", 8.0, nullptr);
    tree.setProperty ("loopEnd", 2.0, nullptr);
    restored = ProjectSerializer::fromValueTree (tree);
    REQUIRE (restored.has_value());
    CHECK_FALSE (restored->hasLoop());
    CHECK_FALSE (restored->loopOn);
}
