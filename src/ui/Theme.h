#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace curvable
{

/** 界面配色（新拟物风格，日间 / 夜间）。 */
struct Palette
{
    bool isDark = false;

    juce::Colour bg, shadowDark, shadowLight;
    juce::Colour text, muted, error;
    juce::Colour line, lineStrong, blackRow;
    juce::Colour accent, warm;
    juce::Colour keyWhite, keyBlack, keyLine;
    juce::Colour noteLow, noteHigh; ///< 力度最小 / 最大时的音符颜色

    static Palette day();
    static Palette night();

    /** 力度 0..1 → 音符颜色。 */
    juce::Colour noteColour (float velocity) const;

    /** 画在该颜色上的文字应使用的颜色。 */
    juce::Colour textOn (juce::Colour background) const;
};

/** 新拟物风格的基本形状。阴影较耗时，大面积的形状应缓存到图像里再绘制。 */
namespace neu
{
    /** 凸起：右下暗影 + 左上亮影。 */
    void raised (juce::Graphics&, juce::Rectangle<float>, float cornerRadius, const Palette&, int depth = 6);

    /** 凹陷：内阴影。 */
    void inset (juce::Graphics&, juce::Rectangle<float>, float cornerRadius, const Palette&, int depth = 4);

    /** 轻量凸起（不做模糊，适合每帧大量绘制的小元素，例如音符）。 */
    void raisedFast (juce::Graphics&, const juce::Path&, juce::Colour fill, const Palette&, float offset = 1.5f);
}

} // namespace curvable
