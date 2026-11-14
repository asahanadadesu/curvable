#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/SynthBrowser.h"

namespace curvable
{

class CurvableEditor final : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::ChangeListener
{
public:
    explicit CurvableEditor (CurvableProcessor&);
    ~CurvableEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // 从资源管理器 / 访达拖入插件文件即可加载，无需扫描
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void refresh();
    void showSynthBrowser();
    void hideSynthBrowser();
    void showPluginManager();
    void browseForPluginFile();
    void loadPluginFile (const juce::File&);
    void loadDescription (const juce::PluginDescription&);
    void showMessage (const juce::String& text, bool isError);

    CurvableProcessor& processor;
    HostedInstrument& hosted;
    HostingContext& context;

    juce::TextButton synthButton;
    juce::TextButton editorButton { "Show synth" };
    SynthBrowser browser;

    juce::String statusText;
    bool statusIsError = false;
    bool loading = false;
    bool dragHighlight = false;

    /** 编辑器自己的提示（例如拖入的文件无法识别），优先于音源状态显示。 */
    juce::String message;
    bool messageIsError = false;

    std::unique_ptr<juce::DocumentWindow> pluginManagerWindow;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableEditor)
};

} // namespace curvable
