#pragma once

#include "plugin/PluginProcessor.h"
#include "ui/CurvableLookAndFeel.h"
#include "ui/PianoRoll.h"
#include "ui/SlotsPanel.h"
#include "ui/SynthBrowser.h"

namespace curvable
{

class CurvableEditor final : public juce::AudioProcessorEditor,
                             public juce::FileDragAndDropTarget,
                             private juce::ChangeListener,
                             private juce::ComponentListener,
                             private juce::Timer
{
public:
    explicit CurvableEditor (CurvableProcessor&);
    ~CurvableEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // 从资源管理器 / 访达 / FL 浏览器拖入插件文件即可加载，无需扫描
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    enum class Page { pianoRoll, synth };

    /** 浮在最上层的提示（拖拽提示），不拦截鼠标。 */
    struct Overlay final : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    /** 浮在卷帘上方的一次性提示卡片（带"知道了"按钮）。 */
    struct NoticeCard final : public juce::Component
    {
        explicit NoticeCard (const Palette& p) : palette (p)
        {
            CurvableLookAndFeel::setStyle (dismissButton, "pill");
            dismissButton.setWantsKeyboardFocus (false);
            addAndMakeVisible (dismissButton);
        }

        void paint (juce::Graphics&) override;
        void resized() override { dismissButton.setBounds (getLocalBounds().removeFromRight (104).reduced (6, 4)); }

        const Palette& palette;
        juce::String text;
        juce::TextButton dismissButton { "Got it" };
    };

    /** Synth 页右边缘的竖向把手：单击展开参数槽面板。 */
    struct SlotsHandle final : public juce::Button
    {
        SlotsHandle (const Palette& p, AutomationSlots& s) : juce::Button ("Automation slots"), palette (p), slots (s) {}
        void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
        const Palette& palette;
        AutomationSlots& slots;
    };

    /** 圆形图标按钮（设置）。 */
    struct GearButton final : public juce::Button
    {
        explicit GearButton (const Palette& p) : juce::Button ("Settings"), palette (p) {}
        void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
        const Palette& palette;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void componentMovedOrResized (juce::Component&, bool wasMoved, bool wasResized) override;
    void componentBeingDeleted (juce::Component&) override;

    void refresh();
    void timerCallback() override;
    void setPage (Page);
    void refreshModeButtons();
    void applyTheme (bool dark);
    void showSettingsMenu();
    void updateNotice();
    void rebuildChrome();

    /** 在 Synth 页嵌入音源界面（或隐藏它），并让窗口贴合它。 */
    void updateEmbeddedEditor();
    void detachEmbeddedEditor();
    void fitWindowToSynth (juce::AudioProcessorEditor&);
    void leaveSynthLayout();

    juce::Rectangle<int> getBodyArea() const;
    bool isCompact() const noexcept;

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

    Palette palette;
    CurvableLookAndFeel lookAndFeel;

    juce::TextButton pianoRollTab { "Piano roll" };
    juce::TextButton synthTab { "Synth" };
    juce::TextButton autoModeButton { "AUTO" };
    juce::TextButton triggerModeButton { "TRIGGER" };
    juce::TextButton offModeButton { "OFF" };
    juce::TextButton synthButton;
    GearButton settingsButton { palette };

    PianoRoll pianoRoll { processor, palette };
    SynthBrowser browser;
    Overlay overlay;
    SlotsPanel slotsPanel { processor.getAutomationSlots(), hosted, palette };
    SlotsHandle slotsHandle { palette, processor.getAutomationSlots() };
    bool slotsOpen = false;

    static constexpr int slotsHandleWidth = 30;

    /** Synth 页右侧参数槽面板（展开时）或把手（收起时）占用的宽度；卷帘页为 0。 */
    int getSlotsWidth() const noexcept
    {
        return page == Page::synth ? (slotsOpen ? SlotsPanel::preferredWidth : slotsHandleWidth) : 0;
    }
    void setSlotsOpen (bool);
    NoticeCard copiesNotice { palette };

    juce::Rectangle<int> logoBounds, tabsBounds, modesBounds;
    juce::Image chrome;

    juce::String statusText;
    bool statusIsError = false;
    bool loading = false;
    bool dragHighlight = false;
    int shownAwake = -1; ///< 音源按钮上显示的运行中实例数

    Page page = Page::pianoRoll;
    juce::Component::SafePointer<juce::AudioProcessorEditor> embeddedEditor;

    bool synthLayoutActive = false;               ///< 窗口当前按音源界面定尺寸
    juce::Point<int> pianoRollSize { 1000, 620 }; ///< 回到卷帘页时恢复的窗口大小
    bool layingOut = false;                       ///< 防止"我们调整音源大小 → 音源通知我们 → 再调整"的循环

    /** 编辑器自己的提示（例如拖入的文件无法识别），优先于音源状态显示。 */
    juce::String message;
    bool messageIsError = false;

    std::unique_ptr<juce::DocumentWindow> pluginManagerWindow;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvableEditor)
};

} // namespace curvable
