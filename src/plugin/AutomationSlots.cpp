#include "plugin/AutomationSlots.h"

namespace curvable
{

namespace
{
    constexpr auto slotType = "Slot";
}

//==============================================================================
SlotParameter::SlotParameter (int slotIndex)
    : juce::AudioParameterFloat (juce::ParameterID { "slot" + juce::String (slotIndex + 1), 1 },
                                 "Slot " + juce::String (slotIndex + 1),
                                 juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
      number (slotIndex + 1)
{
}

juce::String SlotParameter::getName (int maximumLength) const
{
    juce::String text;
    {
        const juce::SpinLock::ScopedLockType sl (nameLock);
        text = boundName.isNotEmpty() ? juce::String (number) + " " + boundName : "Slot " + juce::String (number);
    }

    return text.substring (0, maximumLength);
}

void SlotParameter::setBoundName (const juce::String& newName)
{
    const juce::SpinLock::ScopedLockType sl (nameLock);
    boundName = newName;
}

//==============================================================================
AutomationSlots::AutomationSlots (juce::AudioProcessor& o, HostedInstrument& h)
    : owner (o), hosted (h)
{
    for (int i = 0; i < numSlots; ++i)
    {
        auto param = std::make_unique<SlotParameter> (i);
        params[(size_t) i] = param.get();
        owner.addParameter (param.release());
        binding[(size_t) i] = -1;
        lastSent[(size_t) i] = 0.0f;
    }
}

//==============================================================================
void AutomationSlots::applyToHosted() noexcept
{
    for (int i = 0; i < numSlots; ++i)
    {
        const auto index = binding[(size_t) i].load();

        if (index < 0)
            continue;

        const auto value = params[(size_t) i]->getNormalisedValue();

        if (value != lastSent[(size_t) i].load() && hosted.setParameterValue (index, value))
            lastSent[(size_t) i] = value;
    }
}

void AutomationSlots::hostedParameterTouched (int index, float value) noexcept
{
    if (const auto slot = learning.exchange (-1); slot >= 0)
    {
        bind (slot, index, value);
        return;
    }

    // 用户直接拧了已绑定的旋钮：参数位跟着变。先更新参数位，再记下已发送的值，
    // 这样音频线程不会把旧值发回去
    for (int i = 0; i < numSlots; ++i)
    {
        if (binding[(size_t) i].load() == index)
        {
            params[(size_t) i]->setValueNotifyingHost (value);
            lastSent[(size_t) i] = value;
        }
    }
}

void AutomationSlots::bind (int slot, int index, float value) noexcept
{
    // 一个音源参数只绑定到一个槽
    for (int i = 0; i < numSlots; ++i)
        if (i != slot && binding[(size_t) i].load() == index)
            binding[(size_t) i] = -1;

    binding[(size_t) slot] = index;
    params[(size_t) slot]->setValueNotifyingHost (value);
    lastSent[(size_t) slot] = value;
    dirty = true;
}

//==============================================================================
void AutomationSlots::startLearn (int slot)
{
    if (juce::isPositiveAndBelow (slot, numSlots))
    {
        learning = slot;
        changed.sendChangeMessage();
    }
}

void AutomationSlots::cancelLearn()
{
    learning = -1;
    changed.sendChangeMessage();
}

void AutomationSlots::clear (int slot)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    binding[(size_t) slot] = -1;

    if (learning.load() == slot)
        learning = -1;

    dirty = true; // 名字、宿主显示和"需要保存"由 update() 在定时器里处理
    changed.sendChangeMessage();
}

void AutomationSlots::clearAll()
{
    for (int i = 0; i < numSlots; ++i)
        binding[(size_t) i] = -1;

    learning = -1;
    dirty = true; // 名字、宿主显示和"需要保存"由 update() 在定时器里处理
    changed.sendChangeMessage();
}

int AutomationSlots::getBinding (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, numSlots) ? binding[(size_t) slot].load() : -1;
}

juce::String AutomationSlots::getBoundName (int slot) const
{
    return juce::isPositiveAndBelow (slot, numSlots) ? names[(size_t) slot] : juce::String();
}

float AutomationSlots::getValue (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, numSlots) ? params[(size_t) slot]->getNormalisedValue() : 0.0f;
}

bool AutomationSlots::update()
{
    if (! dirty.exchange (false))
        return false;

    for (int i = 0; i < numSlots; ++i)
    {
        const auto index = binding[(size_t) i].load();

        if (index < 0)
            names[(size_t) i] = {};
        else if (hosted.isLoaded())
            names[(size_t) i] = hosted.getParameterName (index); // 音源找不到时保留保存的名字

        params[(size_t) i]->setBoundName (names[(size_t) i]);
    }

    // 让宿主刷新参数名（VST3：kParamTitlesChanged）
    owner.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
    changed.sendChangeMessage();
    return true;
}

//==============================================================================
juce::ValueTree AutomationSlots::save() const
{
    juce::ValueTree tree (stateType);

    for (int i = 0; i < numSlots; ++i)
    {
        if (const auto index = binding[(size_t) i].load(); index >= 0)
        {
            juce::ValueTree slot (slotType);
            slot.setProperty ("slot", i, nullptr);
            slot.setProperty ("param", index, nullptr);
            slot.setProperty ("name", names[(size_t) i], nullptr);
            slot.setProperty ("value", params[(size_t) i]->getNormalisedValue(), nullptr);
            tree.appendChild (slot, nullptr);
        }
    }

    return tree;
}

void AutomationSlots::restore (const juce::ValueTree& tree)
{
    learning = -1;

    for (int i = 0; i < numSlots; ++i)
    {
        binding[(size_t) i] = -1;
        names[(size_t) i] = {};
    }

    for (const auto& slot : tree)
    {
        if (! slot.hasType (slotType))
            continue;

        const int i = slot.getProperty ("slot", -1);
        const int index = slot.getProperty ("param", -1);

        if (juce::isPositiveAndBelow (i, numSlots) && index >= 0)
        {
            binding[(size_t) i] = index;
            names[(size_t) i] = slot.getProperty ("name").toString();

            // 恢复参数位的值；它与音源里保存的旋钮值一致，所以记为"已发送"，不会一恢复就推给音源
            const auto value = juce::jlimit (0.0f, 1.0f, (float) (double) slot.getProperty ("value", 0.0));
            params[(size_t) i]->setValueNotifyingHost (value);
            lastSent[(size_t) i] = value;
        }
    }

    for (int i = 0; i < numSlots; ++i)
        params[(size_t) i]->setBoundName (names[(size_t) i]);

    owner.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withParameterInfoChanged (true));
    changed.sendChangeMessage();
}

} // namespace curvable
