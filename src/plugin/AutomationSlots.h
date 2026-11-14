#pragma once

#include "hosting/HostedInstrument.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace curvable
{

/** Curvable 暴露给宿主的一个参数位。宿主里显示的名字随绑定的音源参数变化（例如 "1 Macro 1"）。 */
class SlotParameter final : public juce::AudioParameterFloat
{
public:
    explicit SlotParameter (int slotIndex);

    juce::String getName (int maximumLength) const override;

    /** 0..1 的当前值（AudioParameterFloat 把 getValue 设成了 private）。 */
    float getNormalisedValue() const noexcept { return static_cast<const juce::AudioProcessorParameter&> (*this).getValue(); }

    /** 绑定的音源参数名；空字符串表示未绑定。任意线程可调用。 */
    void setBoundName (const juce::String&);

private:
    const int number;
    mutable juce::SpinLock nameLock;
    juce::String boundName;
};

/** 参数槽：宿主（FL）自动化 Curvable 的参数位，Curvable 再转给被托管音源的某个参数（主实例和所有副本）。

    - Learn：选中一个槽后，用户在音源界面里动的下一个参数就绑定到这个槽。
    - 双向：用户直接拧音源里已绑定的旋钮时，参数位也跟着变（宿主的显示、录制自动化都能看到）。
    - 绑定随工程保存；音源找不到时原样保留。换成别的音源时由界面调用 clearAll。

    线程：applyToHosted 在音频线程；hostedParameterTouched 任意线程；其余在消息线程。
*/
class AutomationSlots
{
public:
    static constexpr int numSlots = 16;

    /** 创建参数并加入 owner（参数归 owner 所有）。 */
    AutomationSlots (juce::AudioProcessor& owner, HostedInstrument&);

    //==============================================================================
    // 音频线程

    /** 把参数位的新值转给音源。在 HostedInstrument::process 之前调用。 */
    void applyToHosted() noexcept;

    //==============================================================================
    // 任意线程

    /** 用户在音源界面里动了参数 index（由 HostedInstrument 回调）。 */
    void hostedParameterTouched (int index, float value) noexcept;

    //==============================================================================
    // 消息线程

    void startLearn (int slot);
    void cancelLearn();
    int getLearningSlot() const noexcept { return learning.load(); }

    void clear (int slot);
    void clearAll();

    /** 绑定的音源参数下标；-1 表示未绑定。 */
    int getBinding (int slot) const noexcept;
    juce::String getBoundName (int slot) const;
    float getValue (int slot) const noexcept;

    /** 定期调用（消息线程）：Learn 完成后解析参数名、通知宿主参数名变化、广播变化。
        返回绑定是否有变化（调用方据此标记工程需要保存）。 */
    bool update();

    juce::ValueTree save() const;
    void restore (const juce::ValueTree&);

    static constexpr auto stateType = "Slots";

    /** 绑定或学习状态变化时发送。 */
    juce::ChangeBroadcaster changed;

private:
    void bind (int slot, int index, float value) noexcept;

    juce::AudioProcessor& owner;
    HostedInstrument& hosted;

    std::array<SlotParameter*, numSlots> params {};
    std::array<std::atomic<int>, numSlots> binding;
    std::array<std::atomic<float>, numSlots> lastSent;
    std::array<juce::String, numSlots> names; ///< 消息线程
    std::atomic<int> learning { -1 };
    std::atomic<bool> dirty { false };        ///< 绑定变了，需要在消息线程上更新名字
};

} // namespace curvable
