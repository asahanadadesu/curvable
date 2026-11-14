#include "hosting/HostedInstrument.h"

namespace curvable
{

namespace
{
    constexpr auto descriptionType = "PLUGIN";
    constexpr auto stateProperty   = "state";
}

//==============================================================================
HostedInstrument::HostedInstrument (HostingContext& ctx)
    : context (ctx)
{
    clones.reserve (maxInstances);
    spareMidi.ensureSize (1024);
    startTimerHz (5);
}

HostedInstrument::~HostedInstrument()
{
    stopTimer();
    releaseEditor();

    std::unique_ptr<juce::AudioPluginInstance> old;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> oldClones;
    {
        const juce::ScopedLock sl (audioLock);
        old = std::move (instance);
        oldClones.swap (clones);
        numInstances = 0;
    }

    if (old != nullptr)
    {
        old->removeListener (&changeWatcher);
        old->releaseResources();
    }

    for (auto& c : oldClones)
        c->releaseResources();
}

namespace
{
    thread_local bool insideProcessBlock = false;
}

bool HostedInstrument::isInsideProcessBlock() noexcept
{
    return insideProcessBlock;
}

void HostedInstrument::noteUserChange() noexcept
{
    // 插件加载或恢复状态后，常常会异步发出一些变化通知，这些不算用户的改动
    if (juce::Time::getMillisecondCounter() >= ignoreChangesUntil.load())
        userChanged = true;
}

//==============================================================================
void HostedInstrument::load (const juce::PluginDescription& desc, LoadCallback onDone)
{
    JUCE_ASSERT_MESSAGE_THREAD

    const auto generation = ++loadGeneration;
    juce::WeakReference<HostedInstrument> weakThis (this);

    context.formatManager.createPluginInstanceAsync (
        desc, sampleRate, blockSize,
        [weakThis, generation, onDone = std::move (onDone)] (std::unique_ptr<juce::AudioPluginInstance> created,
                                                             const juce::String& error)
        {
            auto* self = weakThis.get();

            if (self == nullptr || generation != self->loadGeneration)
                return; // 对象已销毁，或用户已经选了别的插件

            if (created == nullptr)
            {
                self->setError (error.isNotEmpty() ? error : juce::String ("Unknown error"));
            }
            else
            {
                self->missingState = {};
                self->lastError = {};
                self->install (std::move (created), nullptr);
            }

            if (onDone)
                onDone (self->lastError);
        });
}

void HostedInstrument::unload()
{
    JUCE_ASSERT_MESSAGE_THREAD

    ++loadGeneration;
    missingState = {};
    lastError = {};
    install (nullptr, nullptr);
}

bool HostedInstrument::isLoaded() const
{
    return instance != nullptr;
}

juce::String HostedInstrument::getPluginName() const
{
    if (instance != nullptr)
        return instance->getName();

    if (missingState.isValid())
        if (auto xml = missingState.getChildWithName (descriptionType).createXml())
            return xml->getStringAttribute ("name");

    return {};
}

juce::String HostedInstrument::getLoadedIdentifier() const
{
    return instance != nullptr ? instance->getPluginDescription().createIdentifierString() : juce::String();
}

std::optional<juce::PluginDescription> HostedInstrument::getLoadedDescription() const
{
    if (instance == nullptr)
        return std::nullopt;

    return instance->getPluginDescription();
}

//==============================================================================
juce::AudioProcessorEditor* HostedInstrument::getOrCreateEditor()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (editor != nullptr || instance == nullptr)
        return editor.get();

    editor.reset (instance->createEditorAndMakeActive());

    // 没有自带界面的插件：用 JUCE 的通用参数界面代替
    if (editor == nullptr)
        editor = std::make_unique<juce::GenericAudioProcessorEditor> (*instance);

    return editor.get();
}

void HostedInstrument::releaseEditor()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (editor == nullptr)
        return;

    if (instance != nullptr)
        instance->editorBeingDeleted (editor.get());

    editor = nullptr; // 编辑器必须先于插件实例销毁
}

//==============================================================================
juce::ValueTree HostedInstrument::saveState() const
{
    if (instance == nullptr)
        return missingState.createCopy();

    juce::ValueTree tree (stateType);

    if (auto xml = instance->getPluginDescription().createXml())
        tree.appendChild (juce::ValueTree::fromXml (*xml), nullptr);

    juce::MemoryBlock data;
    instance->getStateInformation (data);
    tree.setProperty (stateProperty, data.toBase64Encoding(), nullptr);

    return tree;
}

void HostedInstrument::restoreState (const juce::ValueTree& tree)
{
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        restoreNow (tree);
        return;
    }

    juce::WeakReference<HostedInstrument> weakThis (this);
    juce::MessageManager::callAsync ([weakThis, copy = tree.createCopy()]
    {
        if (auto* self = weakThis.get())
            self->restoreNow (copy);
    });
}

void HostedInstrument::restoreNow (const juce::ValueTree& tree)
{
    JUCE_ASSERT_MESSAGE_THREAD

    ++loadGeneration;
    lastError = {};
    missingState = {};

    if (! tree.isValid() || ! tree.hasType (stateType))
    {
        install (nullptr, nullptr);
        return;
    }

    juce::PluginDescription stored;
    const auto descXml = tree.getChildWithName (descriptionType).createXml();

    if (descXml == nullptr || ! stored.loadFromXml (*descXml))
    {
        install (nullptr, nullptr);
        setError ("The saved synth information is unreadable.");
        return;
    }

    juce::MemoryBlock data;
    data.fromBase64Encoding (tree.getProperty (stateProperty).toString());

    // 先按保存的路径加载；失败时在本机已扫描的插件里找同一个插件
    juce::String error;
    auto created = context.formatManager.createPluginInstance (stored, sampleRate, blockSize, error);

    if (created == nullptr)
        if (const auto match = context.findKnownMatch (stored))
            created = context.formatManager.createPluginInstance (*match, sampleRate, blockSize, error);

    if (created == nullptr)
    {
        install (nullptr, nullptr);
        missingState = tree.createCopy();
        setError ("Could not load \"" + stored.name + "\": " + (error.isNotEmpty() ? error : juce::String ("not found")));
        return;
    }

    install (std::move (created), &data);
}

void HostedInstrument::setError (const juce::String& message)
{
    lastError = message;
    sendChangeMessage();
}

//==============================================================================
void HostedInstrument::install (std::unique_ptr<juce::AudioPluginInstance> newInstance,
                                const juce::MemoryBlock* stateToRestore)
{
    JUCE_ASSERT_MESSAGE_THREAD

    releaseEditor();

    // 旧音源的副本作废（包括正在创建中的）
    ++cloneGeneration;
    creatingClone = false;
    cloneError = {};
    cloneRetryAt = 0;
    surplusSince = 0;
    fullSyncPending = false;

    juce::AudioBuffer<float> newScratch;
    double preparedRate = 0.0;
    int preparedBlock = 0;

    if (newInstance != nullptr)
    {
        configureBuses (*newInstance);

        if (stateToRestore != nullptr && stateToRestore->getSize() > 0)
            newInstance->setStateInformation (stateToRestore->getData(), (int) stateToRestore->getSize());

        preparedRate = sampleRate;
        preparedBlock = blockSize;
        newInstance->setRateAndBufferSizeDetails (preparedRate, preparedBlock);

        if (prepared)
            newInstance->prepareToPlay (preparedRate, preparedBlock);

        newScratch.setSize (scratchChannelsFor (*newInstance), preparedBlock);
    }

    std::unique_ptr<juce::AudioPluginInstance> old;
    std::vector<std::unique_ptr<juce::AudioPluginInstance>> oldClones;
    {
        const juce::ScopedLock sl (audioLock);

        // 加载期间宿主可能改了采样率或缓冲大小
        if (newInstance != nullptr && prepared
            && (preparedRate != sampleRate || preparedBlock != blockSize))
        {
            newInstance->releaseResources();
            newInstance->setRateAndBufferSizeDetails (sampleRate, blockSize);
            newInstance->prepareToPlay (sampleRate, blockSize);
            newScratch.setSize (scratchChannelsFor (*newInstance), blockSize);
        }

        old = std::move (instance);
        instance = std::move (newInstance);
        oldClones.swap (clones);
        numInstances = instance != nullptr ? 1 : 0;

        for (auto& a : activity)
            a.reset();
        std::swap (scratch, newScratch);
        lastNonRealtime = false;

        latencySamples = instance != nullptr ? instance->getLatencySamples() : 0;
        tailSeconds = instance != nullptr ? instance->getTailLengthSeconds() : 0.0;
    }

    ignoreChangesUntil = juce::Time::getMillisecondCounter() + 1500;

    if (instance != nullptr)
        instance->addListener (&changeWatcher);

    if (old != nullptr)
    {
        old->removeListener (&changeWatcher);
        old->releaseResources(); // old 在离开作用域时于消息线程上销毁
    }

    for (auto& c : oldClones)
        c->releaseResources();

    sendChangeMessage();
    createNextClone();
}

//==============================================================================
void HostedInstrument::setWantedInstances (int count)
{
    JUCE_ASSERT_MESSAGE_THREAD

    count = juce::jlimit (1, maxInstances, count);

    if (count == wantedInstances)
        return;

    wantedInstances = count;
    createNextClone();
    sendChangeMessage();
}

void HostedInstrument::createNextClone()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (creatingClone || instance == nullptr || 1 + (int) clones.size() >= wantedInstances)
        return;

    if (cloneRetryAt != 0 && juce::Time::getMillisecondCounter() < cloneRetryAt)
        return; // 刚失败过，等一会儿再试（由 timerCallback 重新调用）

    creatingClone = true;
    const auto generation = cloneGeneration;
    juce::WeakReference<HostedInstrument> weakThis (this);

    // 一次只创建一个：大型音源创建要几百毫秒，逐个来不会让界面卡住太久
    context.formatManager.createPluginInstanceAsync (
        instance->getPluginDescription(), sampleRate, blockSize,
        [weakThis, generation] (std::unique_ptr<juce::AudioPluginInstance> created, const juce::String& error)
        {
            auto* self = weakThis.get();

            if (self == nullptr || generation != self->cloneGeneration || self->instance == nullptr)
                return; // 已销毁，或者换了音源

            self->creatingClone = false;

            if (created == nullptr)
            {
                // 创建失败：记下原因，10 秒后再试；在此之前缺实例的声部暂时不弯
                self->cloneError = error.isNotEmpty() ? error : juce::String ("unknown error");
                self->cloneRetryAt = juce::Time::getMillisecondCounter() + 10000;
                self->sendChangeMessage();
                return;
            }

            self->cloneError = {};
            self->cloneRetryAt = 0;

            configureBuses (*created);

            juce::MemoryBlock state;
            self->instance->getStateInformation (state);

            if (state.getSize() > 0)
                created->setStateInformation (state.getData(), (int) state.getSize());

            const auto rate = self->sampleRate;
            const auto block = self->blockSize;
            self->prepareInstance (*created, rate, block);

            {
                const juce::ScopedLock sl (self->audioLock);

                // 创建期间宿主可能改了采样率或缓冲大小
                if (self->prepared && (rate != self->sampleRate || block != self->blockSize))
                {
                    created->releaseResources();
                    self->prepareInstance (*created, self->sampleRate, self->blockSize);
                }

                self->clones.push_back (std::move (created));
                self->numInstances = 1 + (int) self->clones.size();
                self->activity[self->clones.size()].reset();
            }

            self->sendChangeMessage();
            self->createNextClone();
        });
}

void HostedInstrument::removeExtraClones()
{
    JUCE_ASSERT_MESSAGE_THREAD

    std::vector<std::unique_ptr<juce::AudioPluginInstance>> removed;
    {
        const juce::ScopedLock sl (audioLock);

        while (1 + (int) clones.size() > wantedInstances && ! clones.empty())
        {
            removed.push_back (std::move (clones.back()));
            clones.pop_back();
        }

        numInstances = instance != nullptr ? 1 + (int) clones.size() : 0;
    }

    for (auto& c : removed)
        c->releaseResources();

    if (! removed.empty())
        sendChangeMessage();
}

void HostedInstrument::syncClones()
{
    JUCE_ASSERT_MESSAGE_THREAD

    if (instance == nullptr || clones.empty())
        return;

    juce::MemoryBlock state;
    instance->getStateInformation (state);

    if (state.getSize() == 0)
        return;

    for (auto& c : clones)
        c->setStateInformation (state.getData(), (int) state.getSize());
}

juce::String HostedInstrument::getParameterName (int index) const
{
    if (instance == nullptr)
        return {};

    const auto& params = instance->getParameters();
    return juce::isPositiveAndBelow (index, params.size()) ? params[index]->getName (64) : juce::String();
}

bool HostedInstrument::setParameterValue (int index, float value) noexcept
{
    const juce::ScopedTryLock sl (audioLock);

    if (! sl.isLocked())
        return false;

    auto set = [&] (juce::AudioPluginInstance& plugin)
    {
        const auto& params = plugin.getParameters();

        if (juce::isPositiveAndBelow (index, params.size()))
            params[index]->setValue (value);
    };

    if (instance != nullptr)
        set (*instance);

    for (auto& c : clones)
        set (*c);

    return true;
}

void HostedInstrument::forwardParameter (int index, float value) noexcept
{
    // 可能在任意线程上（包括主实例的音频回调里）；副本列表只在持有 audioLock 时改变
    const juce::ScopedLock sl (audioLock);

    for (auto& c : clones)
    {
        const auto& params = c->getParameters();

        if (juce::isPositiveAndBelow (index, params.size()))
            params[index]->setValue (value);
    }
}

void HostedInstrument::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();

    // 换预设等变化停下来 0.4 秒后，整体复制一次
    if (fullSyncPending && now - lastStateChange.load() > 400)
    {
        fullSyncPending = false;
        syncClones();
    }

    // 失败后的重试
    if (cloneRetryAt != 0 && now >= cloneRetryAt)
    {
        cloneRetryAt = 0;
        createNextClone();
    }

    // 多余的副本 5 秒后才销毁（编辑时声部数会来回变）
    if (instance != nullptr && 1 + (int) clones.size() > wantedInstances)
    {
        if (surplusSince == 0)
            surplusSince = now;
        else if (now - surplusSince > 5000)
        {
            surplusSince = 0;
            removeExtraClones();
        }
    }
    else
    {
        surplusSince = 0;
    }
}

void HostedInstrument::prepareInstance (juce::AudioPluginInstance& plugin, double rate, int block)
{
    plugin.setRateAndBufferSizeDetails (rate, block);

    if (prepared)
        plugin.prepareToPlay (rate, block);
}

juce::AudioPluginInstance* HostedInstrument::getInstanceUnlocked (int index) const noexcept
{
    if (index == 0)
        return instance.get();

    return juce::isPositiveAndBelow (index - 1, (int) clones.size()) ? clones[(size_t) index - 1].get() : nullptr;
}

void HostedInstrument::configureBuses (juce::AudioPluginInstance& plugin)
{
    // 理想布局：无输入，主输出立体声，其余输出关闭
    auto layout = plugin.getBusesLayout();

    for (auto& in : layout.inputBuses)
        in = juce::AudioChannelSet::disabled();

    for (int i = 0; i < layout.outputBuses.size(); ++i)
        layout.outputBuses.getReference (i) = i == 0 ? juce::AudioChannelSet::stereo()
                                                     : juce::AudioChannelSet::disabled();

    if (plugin.setBusesLayout (layout))
        return;

    // 退一步：只要求主输出为立体声
    auto fallback = plugin.getBusesLayout();

    if (! fallback.outputBuses.isEmpty())
    {
        fallback.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        plugin.setBusesLayout (fallback); // 仍不支持就保持插件默认布局
    }
}

int HostedInstrument::scratchChannelsFor (const juce::AudioPluginInstance& plugin)
{
    return juce::jmax (1, plugin.getTotalNumInputChannels(), plugin.getTotalNumOutputChannels());
}

//==============================================================================
void HostedInstrument::prepare (double newSampleRate, int maxBlockSize)
{
    const juce::ScopedLock sl (audioLock);

    sampleRate = newSampleRate;
    blockSize = maxBlockSize;
    prepared = true;

    if (instance != nullptr)
    {
        instance->setRateAndBufferSizeDetails (sampleRate, blockSize);
        instance->prepareToPlay (sampleRate, blockSize);
        scratch.setSize (scratchChannelsFor (*instance), blockSize);
        latencySamples = instance->getLatencySamples();
        tailSeconds = instance->getTailLengthSeconds();
    }

    for (auto& c : clones)
    {
        c->setRateAndBufferSizeDetails (sampleRate, blockSize);
        c->prepareToPlay (sampleRate, blockSize);
    }

    spareMidi.ensureSize (1024);
}

void HostedInstrument::release()
{
    const juce::ScopedLock sl (audioLock);

    prepared = false;

    if (instance != nullptr)
        instance->releaseResources();

    for (auto& c : clones)
        c->releaseResources();
}

bool HostedInstrument::trackMidi (Activity& a, const juce::MidiBuffer& midi) noexcept
{
    bool started = false;

    for (const auto metadata : midi)
    {
        const auto m = metadata.getMessage();

        if (m.isNoteOn())
        {
            a.held.set ((size_t) m.getNoteNumber());
            started = true;
        }
        else if (m.isNoteOff())
        {
            a.held.reset ((size_t) m.getNoteNumber());
        }
        else if (m.isSustainPedalOn())
        {
            a.sustain = true;
        }
        else if (m.isSustainPedalOff())
        {
            a.sustain = false;
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            a.held.reset();
        }
    }

    return started;
}

void HostedInstrument::process (juce::AudioBuffer<float>& output, juce::MidiBuffer* midi, int numMidi,
                                juce::AudioPlayHead* playHead, bool isNonRealtime) noexcept
{
    const auto numSamples = output.getNumSamples();
    const juce::ScopedTryLock sl (audioLock);

    auto clearMidi = [&]
    {
        for (int i = 0; i < numMidi; ++i)
            midi[i].clear();
    };

    if (! sl.isLocked() || instance == nullptr || ! prepared)
    {
        output.clear();
        clearMidi();
        return;
    }

    const auto count = 1 + (int) clones.size();
    const auto nonRealtimeChanged = isNonRealtime != lastNonRealtime;
    lastNonRealtime = isNonRealtime;

    const auto blockSeconds = numSamples / juce::jmax (1.0, sampleRate);
    int awake = 0;
    bool anyOutput = false; // 是否已有实例写入了 output

    for (int i = 0; i < count; ++i)
    {
        auto* plugin = getInstanceUnlocked (i);
        auto& events = i < numMidi ? midi[i] : spareMidi;
        auto& state = activity[(size_t) i];

        const auto started = trackMidi (state, events);
        const bool holding = state.held.any() || state.sustain;

        if (started || holding)
        {
            state.silentSeconds = 0.0;
            state.asleep = false;
        }

        // 暂停中、这一块也没有任何 MIDI：跳过运算。有 MIDI（弯音、CC 等）时照常交给音源，
        // 处理完如果仍然静音会立即回到暂停（计时没有清零）
        if (state.asleep && events.isEmpty())
            continue;

        state.asleep = false;
        ++awake;

        // 容量足够时不会重新分配内存；宿主给出超过 prepare 时声明的块大小时才会分配
        scratch.setSize (scratchChannelsFor (*plugin), numSamples, false, false, true);
        scratch.clear();

        plugin->setPlayHead (playHead);

        if (nonRealtimeChanged)
            plugin->setNonRealtime (isNonRealtime);

        {
            const juce::ScopedValueSetter<bool> svs (insideProcessBlock, true);
            plugin->processBlock (scratch, events);
        }
        events.clear(); // 音源可能写入了它自己输出的 MIDI

        // 空闲计时：没有按住的音符、输出也静音了，累计够久就暂停（主实例与导出时除外）
        if (i > 0 && ! isNonRealtime && ! holding && scratch.getMagnitude (0, numSamples) < silenceLevel)
        {
            state.silentSeconds += blockSeconds;

            if (state.silentSeconds >= idleSeconds)
                state.asleep = true;
        }
        else
        {
            state.silentSeconds = 0.0;
        }

        // 主输出总线的声道位于缓冲区最前面；单声道输出复制到两边。各实例的输出相加
        const auto hostedOutputs = plugin->getMainBusNumOutputChannels();

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
        {
            if (hostedOutputs <= 0)
            {
                if (! anyOutput)
                    output.clear (ch, 0, numSamples);
            }
            else if (! anyOutput)
            {
                output.copyFrom (ch, 0, scratch, juce::jmin (ch, hostedOutputs - 1), 0, numSamples);
            }
            else
            {
                output.addFrom (ch, 0, scratch, juce::jmin (ch, hostedOutputs - 1), 0, numSamples);
            }
        }

        anyOutput = true;
    }

    if (! anyOutput)
        output.clear();

    numAwake = awake;
    clearMidi();
}

} // namespace curvable
