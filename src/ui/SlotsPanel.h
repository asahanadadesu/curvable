#pragma once

#include "plugin/AutomationSlots.h"
#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace curvable
{

/** Synth 页右侧的参数槽面板：16 行，每行显示绑定的音源参数和当前值，带 Learn 和清除按钮。 */
class SlotsPanel final : public juce::Component,
                         private juce::ChangeListener,
                         private juce::Timer
{
public:
    SlotsPanel (AutomationSlots&, HostedInstrument&, const Palette&);
    ~SlotsPanel() override;

    std::function<void()> onClose;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** 配色改变后调用。 */
    void paletteChanged();

    /** 界面语言改变后重设文字。 */
    void updateTexts();

    static constexpr int preferredWidth = 270;

private:
    struct Row;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void refreshRows();

    AutomationSlots& slots;
    HostedInstrument& hosted;
    const Palette& palette;

    juce::TextButton closeButton { "Close" };
    juce::Viewport viewport;
    juce::Component rowHolder;
    std::vector<std::unique_ptr<Row>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotsPanel)
};

} // namespace curvable
