// 托管冒烟测试：用真实的音源插件检验 HostedInstrument。
//
//   curvable_hosting_smoke <插件文件路径，例如 .vst3>
//
// 检查项：异步加载 → 弹奏一个音符有声音 → 保存状态 → 用状态恢复到新实例 → 仍然有声音 → 卸载。
// 不针对任何特定音源；任何能用初始音色发声的乐器插件都应通过。

#include "hosting/HostedInstrument.h"
#include "hosting/HostingContext.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <iostream>

using namespace curvable;

namespace
{
    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 512;

    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        std::cout << (condition ? "  [ OK ] " : "  [FAIL] ") << what << std::endl;

        if (! condition)
            ++failures;
    }

    /** 弹奏中央 C 约 0.5 秒，返回输出的峰值电平。 */
    float playNoteAndMeasure (HostedInstrument& hosted)
    {
        juce::AudioBuffer<float> buffer (2, testBlockSize);
        float peak = 0.0f;
        const int numBlocks = (int) (0.5 * testSampleRate / testBlockSize);

        for (int i = 0; i < numBlocks; ++i)
        {
            juce::MidiBuffer midi;

            if (i == 0)
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            if (i == numBlocks - 1)
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);

            hosted.process (buffer, midi, nullptr, true);
            peak = juce::jmax (peak, buffer.getMagnitude (0, testBlockSize));
        }

        return peak;
    }

    template <typename Predicate>
    bool waitFor (Predicate&& done, int timeoutMs)
    {
        const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;

        while (! done() && juce::Time::getMillisecondCounter() < end)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

        return done();
    }
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "usage: curvable_hosting_smoke <plugin-file>" << std::endl;
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String pluginPath = juce::CharPointer_UTF8 (argv[1]);
    std::cout << "Plugin: " << pluginPath << std::endl;

    HostingContext context;

    // 1. 读取插件描述（与拖拽加载走同一条路径）
    const juce::File pluginFile (pluginPath);
    const auto instruments = context.addPluginFile (pluginFile);
    const auto* instrument = instruments.isEmpty() ? nullptr : &instruments.getReference (0);

    check (instrument != nullptr, "plugin file contains an instrument");

    if (instrument == nullptr)
        return 1;

    check (context.addPluginFile (pluginFile).size() == instruments.size(), "reading the same file again works");

    // 音源列表（免扫描）：应包含刚读取的插件；若本机装有 FL Studio，也列出它数据库里的音源
    {
        const auto catalog = context.getSynthCatalog();
        int fromFL = 0;
        bool containsTested = false;

        for (const auto& entry : catalog)
        {
            fromFL += entry.fromFLStudio ? 1 : 0;
            containsTested = containsTested || (entry.description.has_value()
                                                && entry.description->createIdentifierString() == instrument->createIdentifierString());
        }

        std::cout << "  catalog: " << catalog.size() << " synths (" << fromFL << " known to FL Studio)" << std::endl;
        check (containsTested, "synth catalog contains the plugin");

        // FL 浏览器里的 .fst（若存在）应能换算回插件文件
        const auto fst = HostingContext::getFLPluginDatabaseFolder().getChildFile ("Installed/Generators/VST3")
                             .getChildFile (instrument->name + ".fst");

        if (fst.existsAsFile())
            check (context.addPluginFile (context.resolveDroppedFile (fst)).size() > 0, "FL .fst resolves to the plugin");
    }

    // 拖入 VST3 包内部的文件时，应定位到包本身
    if (pluginFile.isDirectory())
    {
        const auto inner = pluginFile.getChildFile ("Contents").findChildFiles (juce::File::findFiles, true, "*.vst3");

        if (! inner.isEmpty())
            check (context.addPluginFile (inner.getFirst()).size() == instruments.size(), "file inside the bundle resolves to the bundle");
    }

    std::cout << "  -> " << instrument->name << " (" << instrument->manufacturerName << ", "
              << instrument->pluginFormatName << ")" << std::endl;

    juce::ValueTree savedState;

    // 2. 异步加载并发声
    {
        HostedInstrument hosted (context);
        hosted.prepare (testSampleRate, testBlockSize);

        bool done = false;
        juce::String loadError;
        hosted.load (*instrument, [&] (const juce::String& error) { loadError = error; done = true; });

        check (waitFor ([&] { return done; }, 30000), "async load finished within 30 s");
        check (loadError.isEmpty() && hosted.isLoaded(), "plugin loaded" + (loadError.isNotEmpty() ? " (" + loadError + ")" : juce::String()));

        if (! hosted.isLoaded())
            return 1;

        const auto peak = playNoteAndMeasure (hosted);
        check (peak > 1.0e-4f, "note produces audio (peak " + juce::String (peak, 5) + ")");

        // 3. 保存状态
        savedState = hosted.saveState();
        check (savedState.isValid() && savedState.getProperty ("state").toString().isNotEmpty(), "state saved");

        hosted.release();
    }

    // 4. 恢复到新实例
    {
        HostedInstrument restored (context);
        restored.prepare (testSampleRate, testBlockSize);
        restored.restoreState (savedState); // 消息线程上同步加载

        check (restored.isLoaded() && ! restored.isMissing(), "state restored into a new instance");
        check (restored.getPluginName() == instrument->name, "restored plugin has the same name");

        if (restored.isLoaded())
        {
            const auto peak = playNoteAndMeasure (restored);
            check (peak > 1.0e-4f, "restored instance produces audio (peak " + juce::String (peak, 5) + ")");
        }

        // 5. 卸载
        restored.unload();
        check (! restored.isLoaded(), "unloaded");

        juce::AudioBuffer<float> buffer (2, testBlockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        restored.process (buffer, midi, nullptr, true);
        check (buffer.getMagnitude (0, testBlockSize) == 0.0f, "silence after unload");
    }

    // 6. 找不到插件时保留原状态
    {
        auto bogus = savedState.createCopy();
        auto desc = bogus.getChildWithName ("PLUGIN");
        desc.setProperty ("file", "curvable-nonexistent-plugin.vst3", nullptr);
        desc.setProperty ("uniqueId", "deadbeef", nullptr);
        desc.setProperty ("uid", "deadbeef", nullptr);

        HostedInstrument missing (context);
        missing.restoreState (bogus);
        check (! missing.isLoaded() && missing.isMissing(), "missing plugin is reported as missing");
        check (missing.saveState().isEquivalentTo (bogus), "missing plugin state is preserved on save");
    }

    std::cout << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
