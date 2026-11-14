#include "ui/CurvableLookAndFeel.h"

namespace curvable
{

namespace
{
    juce::String styleOf (const juce::Button& b) { return b.getProperties()["style"].toString(); }
}

CurvableLookAndFeel::CurvableLookAndFeel (const Palette& p)
{
    setPalette (p);
}

void CurvableLookAndFeel::setStyle (juce::Button& b, const char* style)
{
    b.getProperties().set ("style", style);
}

void CurvableLookAndFeel::setPalette (const Palette& p)
{
    palette = p;

    setColour (juce::ResizableWindow::backgroundColourId, p.bg);
    setColour (juce::PopupMenu::backgroundColourId, p.bg);
    setColour (juce::PopupMenu::textColourId, p.text);
    setColour (juce::PopupMenu::headerTextColourId, p.muted);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, p.accent.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, p.text);
    setColour (juce::TextEditor::backgroundColourId, p.keyWhite);
    setColour (juce::TextEditor::textColourId, p.text);
    setColour (juce::TextEditor::highlightColourId, p.accent.withAlpha (0.3f));
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, p.accent.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, p.accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, p.muted.withAlpha (0.5f));
    setColour (juce::TooltipWindow::backgroundColourId, p.bg);
    setColour (juce::TooltipWindow::textColourId, p.text);
    setColour (juce::TooltipWindow::outlineColourId, p.lineStrong);
}

void CurvableLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                bool isHighlighted, bool isDown)
{
    const auto style = styleOf (button);
    auto r = button.getLocalBounds().toFloat().reduced (1.0f);

    if (style == "segment")
    {
        // 未选中：透明（露出外面的凹槽）；选中：凸起
        if (button.getToggleState())
            neu::raised (g, r.reduced (2.0f), juce::jmin (10.0f, r.getHeight() / 2.0f), palette, 3);
        else if (isHighlighted)
        {
            g.setColour (palette.text.withAlpha (0.05f));
            g.fillRoundedRectangle (r.reduced (2.0f), 10.0f);
        }
        return;
    }

    const auto radius = style == "round" || style == "pill" || style == "chip" ? r.getHeight() / 2.0f : 12.0f;
    r = r.reduced (4.0f);

    if (isDown || (style == "chip" && button.getToggleState()))
        neu::inset (g, r, radius, palette, 3);
    else
        neu::raised (g, r, radius, palette, 3);

    if (isHighlighted && ! isDown)
    {
        g.setColour (palette.text.withAlpha (0.04f));
        g.fillRoundedRectangle (r, radius);
    }
}

void CurvableLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    const auto style = styleOf (button);
    const bool on = button.getToggleState();

    auto colour = palette.text;

    if (style == "segment")
        colour = on ? palette.accent : palette.muted;
    else if (style == "chip" && on)
        colour = palette.warm;
    else if (button.getProperties()["colour"].toString() == "accent")
        colour = palette.accent;
    else if (button.getProperties()["colour"].toString() == "error")
        colour = palette.error;

    if (! button.isEnabled())
        colour = colour.withMultipliedAlpha (0.45f);

    auto textArea = button.getLocalBounds().reduced (style == "pill" ? 18 : 10, 2);

    // 状态圆点（"dot" = "accent" / "error"）：在文字左侧
    if (const auto dot = button.getProperties()["dot"].toString(); dot.isNotEmpty())
    {
        const auto dotArea = textArea.removeFromLeft (14).toFloat();
        g.setColour (dot == "error" ? palette.error : palette.accent);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ dotArea.getX() + 3.5f, dotArea.getCentreY() }));
    }

    // 下拉箭头（"chevron"）：在文字右侧
    if (button.getProperties()["chevron"])
    {
        const auto c = textArea.removeFromRight (16).toFloat().getCentre();
        juce::Path chevron;
        chevron.startNewSubPath (c.x - 4.0f, c.y - 2.0f);
        chevron.lineTo (c.x, c.y + 2.5f);
        chevron.lineTo (c.x + 4.0f, c.y - 2.0f);
        g.setColour (palette.muted);
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (colour);
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawFittedText (button.getButtonText(), textArea, juce::Justification::centred, 1, 0.85f);
}

juce::Font CurvableLookAndFeel::getTextButtonFont (juce::TextButton& button, int)
{
    const bool bold = button.getToggleState() && styleOf (button) == "segment";
    return juce::FontOptions (14.0f, bold ? juce::Font::bold : juce::Font::plain);
}

void CurvableLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (palette.bg);
    g.setColour (palette.lineStrong);
    g.drawRect (0, 0, width, height);
}

void CurvableLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                         bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                         bool isMouseOver, bool isMouseDown)
{
    // 细长的凹槽 + 胶囊形滑块（滚动时会频繁重画，所以不用模糊阴影）
    const auto track = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto radius = (isScrollbarVertical ? track.getWidth() : track.getHeight()) / 2.0f;

    g.setColour (palette.shadowDark.withAlpha (palette.isDark ? 0.35f : 0.22f));
    g.fillRoundedRectangle (track, radius);

    if (thumbSize <= 0)
        return;

    auto thumb = isScrollbarVertical ? track.withY ((float) thumbStartPosition).withHeight ((float) thumbSize)
                                     : track.withX ((float) thumbStartPosition).withWidth ((float) thumbSize);
    thumb = thumb.reduced (1.0f);

    g.setColour (isMouseDown ? palette.accent : palette.muted.withAlpha (isMouseOver ? 0.75f : 0.5f));
    g.fillRoundedRectangle (thumb, radius - 1.0f);
}

} // namespace curvable
