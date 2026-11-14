#include "plugin/PluginEditor.h"

namespace curvable
{

CurvableEditor::CurvableEditor (CurvableProcessor& p)
    : AudioProcessorEditor (p)
{
    setResizable (true, true);
    setResizeLimits (640, 400, 4096, 2160);
    setSize (1000, 620);
}

void CurvableEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff12141a));

    auto area = getLocalBounds().toFloat();

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (32.0f, juce::Font::bold));
    g.drawText ("Curvable", area.removeFromTop (area.getHeight() * 0.55f), juce::Justification::centredBottom);

    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("v" CURVABLE_VERSION_STRING, area.removeFromTop (30.0f), juce::Justification::centred);
}

} // namespace curvable
