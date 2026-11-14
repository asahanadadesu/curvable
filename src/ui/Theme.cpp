#include "ui/Theme.h"

namespace curvable
{

namespace
{
    juce::Colour rgba (int r, int g, int b, float a) { return juce::Colour ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, a); }
}

Palette Palette::day()
{
    Palette p;
    p.isDark      = false;
    p.bg          = juce::Colour (0xffe3e7ee);
    p.shadowDark  = rgba (160, 172, 192, 0.65f);
    p.shadowLight = rgba (255, 255, 255, 0.95f);
    p.text        = juce::Colour (0xff39414f);
    p.muted       = juce::Colour (0xff7f8899);
    p.error       = juce::Colour (0xffcf4f3a);
    p.line        = rgba (120, 132, 152, 0.15f);
    p.lineStrong  = rgba (120, 132, 152, 0.34f);
    p.blackRow    = rgba (120, 132, 152, 0.09f);
    p.accent      = juce::Colour (0xff5674ec);
    p.warm        = juce::Colour (0xffe07a4f);
    p.keyWhite    = juce::Colour (0xfff3f5f9);
    p.keyBlack    = juce::Colour (0xff4b5364);
    p.keyLine     = rgba (120, 132, 152, 0.22f);
    p.noteLow     = juce::Colour (0xffc9d2f4);
    p.noteHigh    = juce::Colour (0xff4a68e0);
    return p;
}

Palette Palette::night()
{
    Palette p;
    p.isDark      = true;
    p.bg          = juce::Colour (0xff2a2e35);
    p.shadowDark  = rgba (0, 0, 0, 0.5f);
    p.shadowLight = rgba (255, 255, 255, 0.055f);
    p.text        = juce::Colour (0xffd5dae3);
    p.muted       = juce::Colour (0xff8a93a3);
    p.error       = juce::Colour (0xffff7a6b);
    p.line        = rgba (255, 255, 255, 0.045f);
    p.lineStrong  = rgba (255, 255, 255, 0.11f);
    p.blackRow    = rgba (0, 0, 0, 0.16f);
    p.accent      = juce::Colour (0xff7d9bff);
    p.warm        = juce::Colour (0xffff9a70);
    p.keyWhite    = juce::Colour (0xff3a3f48);
    p.keyBlack    = juce::Colour (0xff1b1e23);
    p.keyLine     = rgba (0, 0, 0, 0.28f);
    p.noteLow     = juce::Colour (0xff3e4661);
    p.noteHigh    = juce::Colour (0xff86a2ff);
    return p;
}

juce::Colour Palette::noteColour (float velocity) const
{
    return noteLow.interpolatedWith (noteHigh, juce::jlimit (0.0f, 1.0f, velocity));
}

juce::Colour Palette::textOn (juce::Colour background) const
{
    return background.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff2a3040) : juce::Colours::white;
}

//==============================================================================
namespace neu
{
    void raised (juce::Graphics& g, juce::Rectangle<float> r, float radius, const Palette& p, int depth)
    {
        juce::Path path;
        path.addRoundedRectangle (r, radius);

        juce::DropShadow (p.shadowDark, depth * 2, { depth, depth }).drawForPath (g, path);
        juce::DropShadow (p.shadowLight, depth * 2, { -depth, -depth }).drawForPath (g, path);

        g.setColour (p.bg);
        g.fillPath (path);
    }

    void inset (juce::Graphics& g, juce::Rectangle<float> r, float radius, const Palette& p, int depth)
    {
        juce::Path path;
        path.addRoundedRectangle (r, radius);

        g.setColour (p.bg);
        g.fillPath (path);

        const juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (path);

        // 一个"外框减去内部"的形状：它的阴影落进内部，就成了内阴影
        juce::Path frame;
        frame.addRectangle (r.expanded ((float) depth * 4.0f));
        frame.addRoundedRectangle (r, radius);
        frame.setUsingNonZeroWinding (false);

        juce::DropShadow (p.shadowDark, depth * 2, { depth, depth }).drawForPath (g, frame);
        juce::DropShadow (p.shadowLight, depth * 2, { -depth, -depth }).drawForPath (g, frame);
    }

    void raisedFast (juce::Graphics& g, const juce::Path& path, juce::Colour fill, const Palette& p, float offset)
    {
        g.setColour (p.shadowDark);
        g.fillPath (path, juce::AffineTransform::translation (offset, offset));
        g.setColour (p.shadowLight);
        g.fillPath (path, juce::AffineTransform::translation (-offset, -offset));
        g.setColour (fill);
        g.fillPath (path);
    }
}

} // namespace curvable
