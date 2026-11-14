#include "ui/SynthBrowser.h"

namespace curvable
{

namespace
{
    const auto panelColour  = juce::Colour (0xff171a21);
    const auto rowColour    = juce::Colour (0xff1f232c);
    const auto accentColour = juce::Colour (0xff5ad1a0);

    constexpr int rowHeight = 34;

    bool matchesQuery (const SynthEntry& entry, const juce::StringArray& tokens)
    {
        const auto haystack = entry.name + " " + entry.vendor;

        for (const auto& token : tokens)
            if (! haystack.containsIgnoreCase (token))
                return false;

        return true;
    }
}

SynthBrowser::SynthBrowser()
{
    search.setTextToShowWhenEmpty ("Type to search synths...", juce::Colours::white.withAlpha (0.35f));
    search.setFont (juce::FontOptions (16.0f));
    search.setIndents (10, 0);
    search.setColour (juce::TextEditor::backgroundColourId, rowColour);
    search.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    search.setColour (juce::TextEditor::focusedOutlineColourId, accentColour.withAlpha (0.6f));
    search.onTextChange = [this] { applyFilter(); };
    search.onReturnKey  = [this] { choose (list.getSelectedRow()); };
    search.onEscapeKey  = [this] { if (onDismiss) onDismiss(); };
    search.addKeyListener (this);

    list.setRowHeight (rowHeight);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setOutlineThickness (0);
    list.addKeyListener (this);

    browseButton.onClick  = [this] { if (onBrowseFile) onBrowseFile(); };
    managerButton.onClick = [this] { if (onPluginManager) onPluginManager(); };
    unloadButton.onClick  = [this] { if (onUnload) onUnload(); };
    closeButton.onClick   = [this] { if (onDismiss) onDismiss(); };

    for (auto* c : std::initializer_list<juce::Component*> { &search, &list, &browseButton, &managerButton, &unloadButton, &closeButton })
        addAndMakeVisible (c);
}

SynthBrowser::~SynthBrowser()
{
    search.removeKeyListener (this);
    list.removeKeyListener (this);
}

void SynthBrowser::setEntries (juce::Array<SynthEntry> entries, const juce::String& current, bool canUnload)
{
    allEntries = std::move (entries);
    currentKey = current;
    unloadButton.setEnabled (canUnload);
    applyFilter();
}

void SynthBrowser::focusSearch()
{
    search.clear();
    applyFilter();
    search.grabKeyboardFocus();
}

//==============================================================================
void SynthBrowser::paint (juce::Graphics& g)
{
    g.fillAll (panelColour);

    if (filtered.isEmpty())
    {
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (allEntries.isEmpty()
                              ? "No synths found.\nDrag a .vst3 file here, browse for one, or scan in the plugin manager."
                              : "No synth matches \"" + search.getText() + "\".",
                          list.getBounds().reduced (20), juce::Justification::centred, 3);
    }
}

void SynthBrowser::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    search.setBounds (area.removeFromTop (36));
    area.removeFromTop (8);

    auto buttons = area.removeFromBottom (30);
    area.removeFromBottom (8);

    browseButton.setBounds (buttons.removeFromLeft (140));
    buttons.removeFromLeft (8);
    managerButton.setBounds (buttons.removeFromLeft (140));
    closeButton.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    unloadButton.setBounds (buttons.removeFromRight (120));

    list.setBounds (area);
}

//==============================================================================
int SynthBrowser::getNumRows()
{
    return filtered.size();
}

void SynthBrowser::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    if (! juce::isPositiveAndBelow (row, filtered.size()))
        return;

    const auto& entry = filtered.getReference (row);
    auto bounds = juce::Rectangle<int> (width, height).reduced (2, 1);

    g.setColour (selected ? accentColour.withAlpha (0.22f) : rowColour);
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    bounds.reduce (12, 0);

    // 右侧标签
    juce::StringArray tags;
    if (entry.getKey() == currentKey) tags.add ("loaded");
    if (entry.recent)                 tags.add ("recent");

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (accentColour.withAlpha (0.8f));
    g.drawText (tags.joinIntoString ("  "), bounds.removeFromRight (120), juce::Justification::centredRight);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    const auto nameWidth = juce::jmin (bounds.getWidth(),
                                       juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), entry.name) + 16);
    g.drawText (entry.name, bounds.removeFromLeft (nameWidth), juce::Justification::centredLeft, true);

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (entry.vendor, bounds, juce::Justification::centredLeft, true);
}

void SynthBrowser::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    choose (row);
}

void SynthBrowser::returnKeyPressed (int row)
{
    choose (row);
}

bool SynthBrowser::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    // 在搜索框里也能用上下键选择
    const auto rows = filtered.size();

    if (rows == 0)
        return false;

    const auto current = juce::jmax (0, list.getSelectedRow());

    if (key == juce::KeyPress::downKey)
    {
        list.selectRow (juce::jmin (rows - 1, current + 1));
        return true;
    }

    if (key == juce::KeyPress::upKey)
    {
        list.selectRow (juce::jmax (0, current - 1));
        return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (onDismiss)
            onDismiss();
        return true;
    }

    return false;
}

//==============================================================================
void SynthBrowser::applyFilter()
{
    const auto tokens = juce::StringArray::fromTokens (search.getText().trim(), " ", "");

    filtered.clearQuick();

    for (const auto& entry : allEntries)
        if (matchesQuery (entry, tokens))
            filtered.add (entry);

    list.updateContent();
    list.selectRow (filtered.isEmpty() ? -1 : 0);
    list.scrollToEnsureRowIsOnscreen (0);
    repaint();
}

void SynthBrowser::choose (int row)
{
    if (! juce::isPositiveAndBelow (row, filtered.size()) || onChoose == nullptr)
        return;

    const auto entry = filtered[row]; // 复制一份：回调可能会刷新列表
    onChoose (entry);
}

} // namespace curvable
