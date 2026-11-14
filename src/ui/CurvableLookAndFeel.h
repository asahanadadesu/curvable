#pragma once

#include "ui/Theme.h"

namespace curvable
{

/** 按钮外观由 "style" 属性决定（Button::getProperties()["style"]）：
    - "segment"：分段按钮，放在凹槽里；选中时凸起、文字为强调色
    - "pill" / "round" / "chip"：凸起的按钮；按下时凹陷
*/
class CurvableLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    explicit CurvableLookAndFeel (const Palette&);

    void setPalette (const Palette&);
    const Palette& getPalette() const noexcept { return palette; }

    static void setStyle (juce::Button&, const char* style);

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

private:
    Palette palette;
};

} // namespace curvable
