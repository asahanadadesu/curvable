#pragma once

#include "model/NoteEditing.h"
#include "ui/RollView.h"
#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace curvable
{

class CurvableProcessor;

/** Curvable 的卷帘编辑器：琴键、小节标尺、音符编辑区、力度区和底部工具栏。

    Curve 工具：单击音符添加弯音节点并拖动；拖动节点移动（吸附半音和网格，Alt 不吸附）；
    拖动段中间的小圆圈改变弯曲形状；右键节点删除；双击音符去掉整条曲线。

    鼠标（与 FL Studio 习惯一致）：
    - Draw 工具：在空白处单击添加音符，按住拖动设置时长；Select 工具：拖出框选
    - 拖动音符移动（时间 + 音高），拖动音符右端改变时长；Alt 拖动不吸附
    - 右键（或右键拖过）删除音符；Shift 单击加选 / 取消选中；Ctrl 在空白处拖动总是框选
    - 滚轮：上下滚动；Shift + 滚轮：左右滚动；Ctrl + 滚轮：水平缩放；Ctrl + Shift + 滚轮：垂直缩放
    - Alt + 滚轮（在音符上）：调整力度

    键盘：Delete 删除；Ctrl+A 全选；Ctrl+C / X / V 复制 / 剪切 / 粘贴；Ctrl+B 或 Ctrl+D 复制一份到后面；
    Ctrl+Z / Ctrl+Y 撤销 / 重做；↑↓ 移调（Shift：八度）；←→ 按网格移动（Shift：一小节）；
    P / E 切换 Draw / Select 工具；Esc 取消选择。
*/
class PianoRoll final : public juce::Component,
                        private juce::ChangeListener,
                        private juce::ScrollBar::Listener,
                        private juce::Timer
{
public:
    PianoRoll (CurvableProcessor&, const Palette&);
    ~PianoRoll() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** 配色改变后调用。 */
    void paletteChanged();

    /** 底部工具栏右侧的提示文字（例如"还没有加载音源"）。 */
    void setStatus (const juce::String& text, bool isError);

    void stopPreview();

    /** 显示 / 隐藏全部操作说明（也可以按 F1 或 ?）。 */
    void showShortcuts (bool shouldShow);

    enum class Tool { draw, select, curve };

private:
    struct Keyboard;
    struct Ruler;
    struct NoteArea;
    struct VelocityLane;
    struct ShortcutsPanel;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void scrollBarMoved (juce::ScrollBar*, double newRangeStart) override;

    // 工程与编辑
    void pullProject();
    void edit (const juce::String& name, const std::function<void (Project&)>& change);
    void beginDrag();
    void updateDrag (Project working);
    void endDrag (const juce::String& name);
    void setSelection (editing::IdSet);

    // 视图
    void clampView();
    void scrollBy (double beats, double rows);
    void centreOnContent();
    void updateScrollBars();

    /** 音符或选择变化后重画（音符区、力度区、底部提示）。 */
    void repaintContent();
    juce::String getHint() const;

    // 工具与吸附
    void setTool (Tool);
    void showSnapMenu();
    void showBendRangeMenu();
    void refreshBendButton();
    double getGrid() const noexcept { return grid; }
    juce::String getGridName() const;

    // 快捷操作
    void deleteSelection();
    void copySelection (bool cut);
    void paste();
    void duplicateSelection();

    void preview (int pitch, float velocity);

    juce::Rectangle<int> getGridBounds() const;

    CurvableProcessor& processor;
    const Palette& palette;

    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<NoteArea> noteArea;
    std::unique_ptr<VelocityLane> velocityLane;
    std::unique_ptr<ShortcutsPanel> shortcutsPanel;

    juce::ScrollBar hScroll { false }, vScroll { true };

    juce::TextButton drawButton { "Draw" }, selectButton { "Select" }, curveButton { "Curve" };
    juce::TextButton snapButton, bendButton, helpButton { "?" };

    RollView view;
    bool viewInitialised = false;

    Project project;          ///< 当前工程的本地副本（显示用）
    editing::IdSet selection;
    Tool tool = Tool::draw;
    double grid = 0.25;       ///< 吸附网格（拍）；0 表示不吸附
    double defaultLength = 1.0;
    double lastMouseBeat = -1.0;
    double playheadBeat = -1.0;

    bool dragging = false;
    Project dragOrigin;

    int previewPitch = -1;

    juce::String statusText;
    bool statusIsError = false;

    juce::Rectangle<int> panelBounds, keysBounds, toolsBounds;
    juce::Image chrome; ///< 缓存的阴影背景

    static inline std::vector<Note> clipboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};

} // namespace curvable
