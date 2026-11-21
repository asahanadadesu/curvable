#pragma once

#include "model/NoteEditing.h"
#include "ui/RollView.h"
#include "model/CurvePresetLibrary.h"
#include "io/MidiExchange.h"
#include "i18n/Translations.h"
#include "ui/Theme.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace curvable
{

class CurvableProcessor;

/** Curvable 的卷帘编辑器：琴键、小节标尺、音符编辑区、力度区和底部工具栏。

    没有工具切换，所有操作都在同一个编辑模式里（与 FL Studio 习惯一致的地方保持一致）：
    - 空白处单击添加音符，按住拖动设置时长；Ctrl 拖动框选
    - 拖动音符移动（时间 + 音高），拖动右端改变时长，拖动左端改变起点；Alt 拖动不吸附
    - Shift 单击加选 / 取消选中；Shift 拖动复制
    - 悬停或选中的音符显示节点，直接拖动（吸附半音和网格，Alt 不吸附）；拖段中间的小圆圈改变弯曲形状；右键节点删除
    - 双击音符加一个节点（按住接着拖）
    - 悬停某一段时，上方出现琥珀线，往上拖加颤音；之后拖菱形手柄、竖线调整参数
    - 右键单击音符：菜单（颤音、Trill、Bend 手法、预设、Glide、切开、删除）；右键拖过：删除
    - 按住 S：单击音符切开，上下拖动切开竖线经过的音符
    - 滚轮：上下滚动；Shift + 滚轮：左右滚动；Ctrl + 滚轮：水平缩放；Ctrl + Shift + 滚轮：垂直缩放
    - Alt + 滚轮（在音符上）：调整力度

    键盘：Delete 删除；Ctrl+A 全选；Ctrl+C / X / V 复制 / 剪切 / 粘贴；Ctrl+B 或 Ctrl+D 复制一份到后面；
    Ctrl+Z / Ctrl+Y 撤销 / 重做；↑↓ 移调（Shift：八度）；←→ 按网格移动（Shift：一小节）；G 滑音；Esc 取消选择。
*/
class PianoRoll final : public juce::Component,
                        public juce::FileDragAndDropTarget,
                        private i18n::Listener,
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
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void paintOverChildren (juce::Graphics&) override;

    // 拖入 MIDI 文件导入音符
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int x, int y) override;
    void fileDragMove (const juce::StringArray&, int x, int y) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

    /** 配色改变后调用。 */
    void paletteChanged();

    /** 底部工具栏右侧的提示文字（例如"还没有加载音源"）。 */
    void setStatus (const juce::String& text, bool isError);

    void stopPreview();

    /** 插件内播放：▶ 或 Space。从起点（单击标尺设置）开始；循环打开时在循环区里循环。 */
    void togglePlayback();

    /** Space 交给 FL（设置菜单），不在插件里播放。全局设置。 */
    static constexpr auto spaceToHostKey = "spaceToHost";

    /** 显示 / 隐藏全部操作说明（也可以按 F1 或 ?）。 */
    void showShortcuts (bool shouldShow);

    /** 在底部提示栏显示几秒钟的提醒（例如操作无法完成的原因；isError = false 时不用红色）。 */
    void flashMessage (const juce::String&, bool isError = true);

private:
    struct Keyboard;
    struct Ruler;
    struct NoteArea;
    struct VelocityLane;
    struct EnvelopeLane;
    struct LaneTabs;
    struct ShortcutsPanel;
    struct NamePrompt;
    struct MidiButton;
    struct TransportButton;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    /** 界面语言改变：重设按钮文字和提示。 */
    void languageChanged() override;
    void updateTexts();
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

    /** 需要用户注意的问题（声部不够、弯音超出范围）；没有时为空。 */
    juce::String getWarning() const;

    // 插件内播放
    void startPlayback();
    void stopPlayback();
    void setStartBeat (double beat);
    /** 设置循环区（end <= start 表示去掉）；正在插件内播放时从当前位置接着播。 */
    void setLoop (double start, double end, bool on);
    void toggleLoop();
    /** 跟随播放头：播放头走出视野时翻页。 */
    void followPlayhead (double beat);
    void refreshTransportButtons();

    // 切开（按住 S）与吸附
    void setSlicing (bool);
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

    /** 在 atBeat 处切开 ids 中的音符，选中切开后的两半。 */
    void sliceAndSelect (const editing::IdSet& ids, double atBeat);

    /** 把选中的音符合并成一个音符，用滑音连起来（G）。 */
    void glideSelection();

    /** 鼠标下的东西怎么用（显示在底部提示栏）；离开时传空字符串。 */
    void setHoverHint (const juce::String&);

    // MIDI 导入导出
    /** 把 MIDI 文件里的音符加到 atBeat（文件开头对齐到这里）。 */
    void importMidiFile (const juce::File&, double atBeat);
    void browseForMidiImport();
    void showMidiMenu();
    void saveMidiAs (midi::Layout);
    /** 从 MIDI 按钮拖出：写一个临时文件交给系统拖放（拖到 FL 的 Playlist 或其他程序）。 */
    void dragMidiOut();
    midi::Layout getDragLayout() const;
    /** 导出结果里需要提醒的问题；没有时为空。 */
    static juce::String describeExport (const midi::ExportResult&, midi::Layout);
    /** 拖入时对齐到的位置：鼠标所在小节的开头。 */
    double dropBeat (int x) const;

    /** 问一个名字，把这条曲线存成预设（noteLength：音符长度，套用时按比例伸缩）。 */
    void savePreset (const PitchCurve&, double noteLength);

    void preview (int pitch, float velocity);

    juce::Rectangle<int> getGridBounds() const;

    CurvableProcessor& processor;
    const Palette& palette;

    std::unique_ptr<Keyboard> keyboard;
    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<NoteArea> noteArea;
    std::unique_ptr<VelocityLane> velocityLane;
    std::unique_ptr<EnvelopeLane> envelopeLane;
    std::unique_ptr<LaneTabs> laneTabs;
    std::unique_ptr<ShortcutsPanel> shortcutsPanel;
    std::unique_ptr<NamePrompt> namePrompt;
    CurvePresetLibrary presetLibrary;

    juce::ScrollBar hScroll { false }, vScroll { true };

    juce::TextButton snapButton, bendButton, helpButton { "?" };
    std::unique_ptr<MidiButton> midiButton;
    std::unique_ptr<TransportButton> playButton, loopButton, followButton;
    juce::Rectangle<int> transportBounds;
    std::unique_ptr<juce::FileChooser> fileChooser;
    double midiDropBeat = -1.0; ///< 正在拖入 MIDI 文件时对齐到的位置；没有拖入时为 -1

    RollView view;
    bool viewInitialised = false;

    Project project;          ///< 当前工程的本地副本（显示用）
    editing::IdSet selection;
    editing::IdSet overflowNotes; ///< 因为声部不够而与别的音符共用音源实例的音符
    /** 音符下方的栏：力度，或者每个音符的音量、声像包络。 */
    enum class LaneKind { velocity, volume, pan };
    LaneKind lane = LaneKind::velocity;
    void setLane (LaneKind);

    bool slicing = false;     ///< 按住 S：单击或拖动切开音符（只有一个编辑模式，没有工具切换）
    double grid = 0.25;       ///< 吸附网格（拍）；0 表示不吸附
    double defaultLength = 1.0;
    double lastMouseBeat = -1.0;
    double playheadBeat = -1.0;
    double startBeat = 0.0;   ///< 插件内播放的起点（单击标尺设置；停止后播放头回到这里）
    bool follow = true;       ///< 跟随播放头（全局设置）

    bool dragging = false;
    Project dragOrigin;

    int previewPitch = -1;

    juce::String flashText;
    juce::uint32 flashUntil = 0;
    bool flashIsError = true;

    juce::String hoverHint;

    juce::String statusText;
    bool statusIsError = false;

    juce::Rectangle<int> panelBounds, keysBounds;
    juce::Image chrome; ///< 缓存的阴影背景

    static inline std::vector<Note> clipboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRoll)
};

} // namespace curvable
