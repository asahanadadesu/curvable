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
}

HostedInstrument::~HostedInstrument()
{
    releaseEditor();

    std::unique_ptr<juce::AudioPluginInstance> old;
    {
        const juce::ScopedLock sl (audioLock);
        old = std::move (instance);
    }

    if (old != nullptr)
    {
        old->removeListener (&changeWatcher);
        old->releaseResources();
    }
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

    sendChangeMessage();
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
}

void HostedInstrument::release()
{
    const juce::ScopedLock sl (audioLock);

    prepared = false;

    if (instance != nullptr)
        instance->releaseResources();
}

void HostedInstrument::process (juce::AudioBuffer<float>& output, juce::MidiBuffer& midi,
                                juce::AudioPlayHead* playHead, bool isNonRealtime) noexcept
{
    const auto numSamples = output.getNumSamples();
    const juce::ScopedTryLock sl (audioLock);

    if (! sl.isLocked() || instance == nullptr || ! prepared)
    {
        output.clear();
        midi.clear();
        return;
    }

    // 容量足够时不会重新分配内存；宿主给出超过 prepare 时声明的块大小时才会分配
    scratch.setSize (scratchChannelsFor (*instance), numSamples, false, false, true);
    scratch.clear();

    instance->setPlayHead (playHead);

    if (isNonRealtime != lastNonRealtime)
    {
        instance->setNonRealtime (isNonRealtime);
        lastNonRealtime = isNonRealtime;
    }

    instance->processBlock (scratch, midi);
    midi.clear();

    // 主输出总线的声道位于缓冲区最前面；单声道输出复制到两边
    const auto hostedOutputs = instance->getMainBusNumOutputChannels();

    for (int ch = 0; ch < output.getNumChannels(); ++ch)
    {
        if (hostedOutputs <= 0)
            output.clear (ch, 0, numSamples);
        else
            output.copyFrom (ch, 0, scratch, juce::jmin (ch, hostedOutputs - 1), 0, numSamples);
    }
}

} // namespace curvable
