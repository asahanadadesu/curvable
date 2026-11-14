#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/SynthBrowser.h"

namespace curvable
{

class CurvableEditor final : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::ChangeListener,
                             private juce::ComponentListener
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
    enum class Page { pianoRoll, synth };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void componentMovedOrResized (juce::Component&, bool wasMoved, bool wasResized) override;
    void componentBeingDeleted (juce::Component&) override;

    void refresh();
    void setPage (Page);
    void refreshModeButtons();

    /** 在 Synth 页嵌入音源界面（或隐藏它），并让窗口足够大。 */
    void updateEmbeddedEditor();
    void detachEmbeddedEditor();

    /** Synth 页：让窗口贴合音源界面的大小。 */
    void fitWindowToSynth (juce::AudioProcessorEditor&);
    void leaveSynthLayout();
    juce::Rectangle<int> getBodyArea() const;
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

    juce::TextButton pianoRollTab { "Piano roll" };
    juce::TextButton synthTab { "Synth" };
    juce::TextButton synthButton;

    // 播放模式
    juce::TextButton autoModeButton { "AUTO" };
    juce::TextButton triggerModeButton { "TRIGGER" };
    juce::TextButton offModeButton { "OFF" };

    // 卷帘页（M2 临时）：测试乐句
    juce::TextButton testPhraseButton { "Insert test phrase" };
    juce::TextButton clearNotesButton { "Clear notes" };
    SynthBrowser browser;

    juce::String statusText;
    bool statusIsError = false;
    bool loading = false;
    bool dragHighlight = false;

    Page page = Page::pianoRoll;
    juce::Component::SafePointer<juce::AudioProcessorEditor> embeddedEditor;

    bool synthLayoutActive = false;            ///< 窗口当前按音源界面定尺寸
    juce::Point<int> pianoRollSize { 1000, 620 }; ///< 回到卷帘页时恢复的窗口大小
    bool layingOut = false;                    ///< 防止"我们调整音源大小 → 音源通知我们 → 再调整"的循环

    /** 编辑器自己的提示（例如拖入的文件无法识别），优先于音源状态显示。 */
    juce::String message;
    bool messageIsError = false;

    std::unique_ptr<juce::DocumentWindow> pluginManagerWindow;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableEditor)
};

} // namespace curvable
