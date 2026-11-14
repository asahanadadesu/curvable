#pragma once

#include "plugin/PluginProcessor.h"

namespace curvable
{

class CurvableEditor final : public juce::AudioProcessorEditor
{
public:
    explicit CurvableEditor (CurvableProcessor&);
    ~CurvableEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableEditor)
};

} // namespace curvable
