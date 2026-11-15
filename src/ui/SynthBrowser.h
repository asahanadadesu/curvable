#pragma once

#include "hosting/HostingContext.h"
#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace curvable
{

/** 音源浏览面板：列出所有可用音源，输入即可搜索，单击或回车加载。

    键盘：直接输入搜索；上下键选择；回车加载；Esc 关闭。
*/
class SynthBrowser final : public juce::Component,
                           private juce::ListBoxModel,
                           private juce::KeyListener
{
public:
    SynthBrowser();
    ~SynthBrowser() override;

    std::function<void (const SynthEntry&)> onChoose;
    std::function<void()> onDismiss;
    std::function<void()> onBrowseFile;
    std::function<void()> onPluginManager;
    std::function<void()> onUnload;

    /** @param currentKey  当前已加载音源的 SynthEntry::getKey()，用于标记 */
    void setEntries (juce::Array<SynthEntry> entries, const juce::String& currentKey, bool canUnload);

    void setPalette (const Palette&);

    /** 界面语言改变后重设文字。 */
    void updateTexts();

    /** 打开面板时调用：清空搜索并让搜索框获得焦点。 */
    void focusSearch();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;

    bool keyPressed (const juce::KeyPress&, juce::Component*) override;

    void applyFilter();
    void choose (int row);

    juce::TextEditor search;
    juce::ListBox list { "Synths", this };
    juce::TextButton browseButton { "Browse for file..." };
    juce::TextButton managerButton { "Plugin manager..." };
    juce::TextButton unloadButton { "Unload synth" };
    juce::TextButton closeButton { "Close" };

    juce::Array<SynthEntry> allEntries;
    juce::Array<SynthEntry> filtered;
    juce::String currentKey;
    Palette palette = Palette::day();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SynthBrowser)
};

} // namespace curvable
