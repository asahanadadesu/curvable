#include "ui/SlotsPanel.h"
#include "i18n/Translations.h"
#include "ui/CurvableLookAndFeel.h"

namespace curvable
{

namespace
{
    constexpr int headerHeight = 86;
    constexpr int rowHeight = 40;
}

//==============================================================================
struct SlotsPanel::Row final : public juce::Component
{
    Row (SlotsPanel& o, int i) : owner (o), slot (i)
    {
        CurvableLookAndFeel::setStyle (learnButton, "chip");
        learnButton.setWantsKeyboardFocus (false);
        learnButton.onClick = [this]
        {
            auto& s = owner.slots;

            if (s.getLearningSlot() == slot)
                s.cancelLearn();
            else
                s.startLearn (slot);
        };
        addAndMakeVisible (learnButton);

        CurvableLookAndFeel::setStyle (clearButton, "round");
        clearButton.setWantsKeyboardFocus (false);
        clearButton.onClick = [this] { owner.slots.clear (slot); };
        addAndMakeVisible (clearButton);
    }

    void refresh()
    {
        const auto& s = owner.slots;
        const bool learning = s.getLearningSlot() == slot;
        const bool bound = s.getBinding (slot) >= 0;

        learnButton.setButtonText (learning ? i18n::tr ("Move a knob") : i18n::tr ("Learn"));
        learnButton.setTooltip (i18n::tr ("Click, then move a knob in the synth to link it to this slot"));
        clearButton.setTooltip (i18n::tr ("Unlink"));
        learnButton.setToggleState (learning, juce::dontSendNotification);
        learnButton.setEnabled (owner.hosted.isLoaded());
        clearButton.setVisible (bound);
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 2);
        clearButton.setBounds (r.removeFromRight (34));
        learnButton.setBounds (r.removeFromRight (104));
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        const auto& s = owner.slots;
        auto r = getLocalBounds().withTrimmedRight (140).toFloat();

        // 编号
        g.setColour (p.muted);
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String (slot + 1), r.removeFromLeft (24.0f), juce::Justification::centred);

        const bool bound = s.getBinding (slot) >= 0;
        auto nameArea = r.reduced (4.0f, 6.0f);

        g.setColour (bound ? p.text : p.muted.withAlpha (0.7f));
        g.setFont (juce::FontOptions (13.5f, bound ? juce::Font::bold : juce::Font::plain));
        g.drawText (bound ? s.getBoundName (slot) : i18n::tr ("Empty"),
                    nameArea.removeFromTop (nameArea.getHeight() - 6.0f), juce::Justification::centredLeft, true);

        // 当前值：细条
        if (bound)
        {
            const auto bar = nameArea.withHeight (3.0f).withY (nameArea.getBottom() - 3.0f);
            g.setColour (p.line);
            g.fillRoundedRectangle (bar, 1.5f);
            g.setColour (p.accent);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * s.getValue (slot)), 1.5f);
        }
    }

    SlotsPanel& owner;
    const int slot;
    juce::TextButton learnButton { "Learn" };
    juce::TextButton clearButton { juce::String::fromUTF8 ("\xc3\x97") };
    float shownValue = -1.0f;
};

//==============================================================================
SlotsPanel::SlotsPanel (AutomationSlots& s, HostedInstrument& h, const Palette& p)
    : slots (s), hosted (h), palette (p)
{
    CurvableLookAndFeel::setStyle (closeButton, "pill");
    closeButton.setWantsKeyboardFocus (false);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (closeButton);

    for (int i = 0; i < AutomationSlots::numSlots; ++i)
    {
        rows.push_back (std::make_unique<Row> (*this, i));
        rowHolder.addAndMakeVisible (*rows.back());
    }

    viewport.setViewedComponent (&rowHolder, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    addAndMakeVisible (viewport);

    slots.changed.addChangeListener (this);
    hosted.addChangeListener (this);
    refreshRows();
    startTimerHz (15);
}

SlotsPanel::~SlotsPanel()
{
    slots.changed.removeChangeListener (this);
    hosted.removeChangeListener (this);
}

void SlotsPanel::updateTexts()
{
    closeButton.setButtonText (i18n::tr ("Close"));
    refreshRows();
    repaint();
}

void SlotsPanel::paletteChanged()
{
    repaint();

    for (auto& r : rows)
        r->repaint();
}

void SlotsPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshRows();
}

void SlotsPanel::refreshRows()
{
    for (auto& r : rows)
        r->refresh();

    repaint();
}

void SlotsPanel::timerCallback()
{
    // 自动化播放时值在变，只重画值变了的行
    for (auto& r : rows)
    {
        const auto v = slots.getBinding (r->slot) >= 0 ? slots.getValue (r->slot) : -1.0f;

        if (v != r->shownValue)
        {
            r->shownValue = v;
            r->repaint();
        }
    }
}

void SlotsPanel::paint (juce::Graphics& g)
{
    g.fillAll (palette.bg);

    // 与音源界面之间的分隔
    g.setColour (palette.lineStrong);
    g.fillRect (0, 0, 1, getHeight());

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (18, 12);
    auto title = header.removeFromTop (28);

    g.setColour (palette.text);
    g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    g.drawFittedText (i18n::tr ("Automation slots"), title.withTrimmedRight (96), juce::Justification::centredLeft, 1, 0.8f);

    g.setColour (palette.muted);
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (hosted.isLoaded() ? i18n::tr ("Automate these in FL. Learn links a slot to a synth knob; all copies follow.")
                                        : i18n::tr ("Load a synth to link its knobs to these slots."),
                      header, juce::Justification::topLeft, 3, 0.95f);
}

void SlotsPanel::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight);
    closeButton.setBounds (header.reduced (12, 10).removeFromTop (36).removeFromRight (84));

    viewport.setBounds (area.reduced (8, 0).withTrimmedBottom (8));
    rowHolder.setSize (viewport.getMaximumVisibleWidth(), rowHeight * (int) rows.size());

    for (int i = 0; i < (int) rows.size(); ++i)
        rows[(size_t) i]->setBounds (0, i * rowHeight, rowHolder.getWidth() - 4, rowHeight);
}

} // namespace curvable
