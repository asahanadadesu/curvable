#include "plugin/PluginEditor.h"

namespace curvable
{

namespace
{
    // 布局（基准尺寸 1000 × 620）
    constexpr int margin            = 16;
    constexpr int toolbarTop        = 12;
    constexpr int toolbarHeight     = 52;
    constexpr int toolbarAreaHeight = toolbarTop + toolbarHeight + 8; ///< 工具栏占用的高度（Synth 页的音源界面紧接其下）
    constexpr int bodyGap           = 6;

    constexpr int tabRadioGroup  = 1001;
    constexpr int modeRadioGroup = 1002;

    constexpr int defaultMinWidth  = 720;
    constexpr int defaultMinHeight = 460;

    // Synth 页：窗口贴合音源界面；工具栏决定最小宽度
    constexpr int synthPageMinWidth  = 480;
    constexpr int synthPageMinHeight = 300;

    // 窗口较窄时省略 Logo，按钮变紧凑，以便贴合较窄的音源界面
    constexpr int compactBelowWidth = 800;

    constexpr auto themeKey = "theme";
    constexpr auto copiesNoticeKey = "copiesNoticeDismissed";

    enum MenuIds { dayThemeId = 1, nightThemeId, testPhraseId, clearNotesId, pluginManagerId, shortcutsId, syncCopiesId };

    /** 插件扫描与管理窗口（JUCE 自带的 PluginListComponent）。 */
    class PluginManagerWindow final : public juce::DocumentWindow
    {
    public:
        PluginManagerWindow (HostingContext& context, juce::Colour background, std::function<void()> onCloseRequested)
            : DocumentWindow ("Curvable - Plugin manager", background, DocumentWindow::closeButton),
              onClose (std::move (onCloseRequested))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new juce::PluginListComponent (context.formatManager,
                                                            context.knownPlugins,
                                                            context.getDeadMansPedalFile(),
                                                            &context.getSettings(),
                                                            true),
                             false);
            setResizable (true, false);
            setResizeLimits (500, 300, 4000, 3000);
            setAlwaysOnTop (true);
            centreWithSize (760, 520);
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            if (onClose)
                onClose();
        }

    private:
        std::function<void()> onClose;
    };

    Palette paletteFromSettings (HostingContext& context)
    {
        return context.getSettings().getValue (themeKey) == "night" ? Palette::night() : Palette::day();
    }
}

//==============================================================================
void CurvableEditor::NoticeCard::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (4.0f);
    neu::raised (g, r, 14.0f, palette, 4);

    auto textArea = getLocalBounds().reduced (18, 4).withTrimmedRight (100);
    g.setColour (palette.warm);
    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) textArea.getX() + 4.0f, (float) textArea.getCentreY() }));
    textArea.removeFromLeft (16);

    g.setColour (palette.text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 2, 0.9f);
}

//==============================================================================
void CurvableEditor::SlotsHandle::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    g.fillAll (palette.bg);
    g.setColour (palette.lineStrong);
    g.fillRect (0, 0, 1, getHeight());

    const auto r = getLocalBounds().toFloat().reduced (4.0f, 8.0f);

    if (isDown)
        neu::inset (g, r, 10.0f, palette, 2);
    else
        neu::raised (g, r, 10.0f, palette, 2);

    // 有绑定时点亮一个圆点
    bool anyBound = false;
    for (int i = 0; i < AutomationSlots::numSlots; ++i)
        anyBound = anyBound || slots.getBinding (i) >= 0;

    const auto centre = r.getCentre();

    // 竖排文字，旁边一个向左的箭头（表示可以展开）
    g.saveState();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, centre.x, centre.y));
    g.setColour (isHighlighted ? palette.text : palette.muted);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText ("AUTOMATION", juce::Rectangle<float> (juce::jmax (40.0f, r.getHeight() - 60.0f), 18.0f).withCentre (centre).toNearestInt(),
                      juce::Justification::centred, 1, 0.7f);
    g.restoreState();

    juce::Path chevron;
    const auto top = juce::Point<float> (centre.x, r.getY() + 16.0f);
    chevron.startNewSubPath (top.x + 2.5f, top.y - 4.0f);
    chevron.lineTo (top.x - 2.0f, top.y);
    chevron.lineTo (top.x + 2.5f, top.y + 4.0f);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (anyBound)
    {
        g.setColour (palette.accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ centre.x, r.getBottom() - 16.0f }));
    }
}

//==============================================================================
void CurvableEditor::GearButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    const auto r = getLocalBounds().toFloat().reduced (4.0f);

    if (isDown)
        neu::inset (g, r, r.getHeight() / 2.0f, palette, 3);
    else
        neu::raised (g, r, r.getHeight() / 2.0f, palette, 3);

    // 齿轮图标
    const auto c = r.getCentre();
    juce::Path gear;
    for (int i = 0; i < 8; ++i)
    {
        const auto angle = juce::MathConstants<float>::twoPi * (float) i / 8.0f;
        gear.addLineSegment (juce::Line<float> (c.getPointOnCircumference (5.0f, angle), c.getPointOnCircumference (8.0f, angle)), 2.2f);
    }
    gear.addEllipse (c.x - 5.5f, c.y - 5.5f, 11.0f, 11.0f);

    g.setColour (isHighlighted ? palette.text : palette.muted);
    g.strokePath (gear, juce::PathStrokeType (1.8f));
}

//==============================================================================
CurvableEditor::CurvableEditor (CurvableProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      hosted (p.getHostedInstrument()),
      context (p.getHostingContext()),
      palette (paletteFromSettings (p.getHostingContext())),
      lookAndFeel (palette)
{
    setLookAndFeel (&lookAndFeel);

    // 工具栏
    for (auto* tab : { &pianoRollTab, &synthTab })
    {
        CurvableLookAndFeel::setStyle (*tab, "segment");
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (tabRadioGroup);
        tab->setWantsKeyboardFocus (false);
        addAndMakeVisible (tab);
    }

    pianoRollTab.setToggleState (true, juce::dontSendNotification);
    pianoRollTab.onClick = [this] { if (pianoRollTab.getToggleState()) setPage (Page::pianoRoll); };
    synthTab.onClick     = [this] { if (synthTab.getToggleState())     setPage (Page::synth); };

    struct ModeButton { juce::TextButton* button; PlaybackMode mode; };

    for (auto [button, mode] : { ModeButton { &autoModeButton, PlaybackMode::automatic },
                                 ModeButton { &triggerModeButton, PlaybackMode::trigger },
                                 ModeButton { &offModeButton, PlaybackMode::off } })
    {
        CurvableLookAndFeel::setStyle (*button, "segment");
        button->setClickingTogglesState (true);
        button->setRadioGroupId (modeRadioGroup);
        button->setWantsKeyboardFocus (false);
        button->onClick = [this, button = button, mode = mode]
        {
            if (button->getToggleState())
                processor.setPlaybackMode (mode);
        };
        addAndMakeVisible (button);
    }

    autoModeButton.setTooltip ("Play Curvable's notes along the host's timeline");
    triggerModeButton.setTooltip ("A note in the host's piano roll starts playback; it loops while the note is held. Host notes make no sound.");
    offModeButton.setTooltip ("Don't play Curvable's notes");

    CurvableLookAndFeel::setStyle (synthButton, "pill");
    synthButton.getProperties().set ("chevron", true);
    synthButton.setWantsKeyboardFocus (false);
    synthButton.onClick = [this] { browser.isVisible() ? hideSynthBrowser() : showSynthBrowser(); };
    addAndMakeVisible (synthButton);

    settingsButton.setTooltip ("Settings");
    settingsButton.setWantsKeyboardFocus (false);
    settingsButton.onClick = [this] { showSettingsMenu(); };
    addAndMakeVisible (settingsButton);

    // 主体
    addAndMakeVisible (pianoRoll);

    browser.setPalette (palette);
    browser.onChoose = [this] (const SynthEntry& entry)
    {
        hideSynthBrowser();

        if (entry.description.has_value())
            loadDescription (*entry.description);
        else
            loadPluginFile (entry.file); // 来自 FL 数据库、尚未读取过的插件
    };
    browser.onDismiss       = [this] { hideSynthBrowser(); };
    browser.onBrowseFile    = [this] { hideSynthBrowser(); browseForPluginFile(); };
    browser.onPluginManager = [this] { showPluginManager(); };
    browser.onUnload        = [this] { hideSynthBrowser(); message = {}; hosted.unload(); processor.markStateChanged(); };
    addChildComponent (browser);

    overlay.setInterceptsMouseClicks (false, false);
    overlay.painter = [this] (juce::Graphics& g)
    {
        if (! dragHighlight)
            return;

        auto area = overlay.getLocalBounds().toFloat().reduced (8.0f);
        g.setColour (palette.bg.withAlpha (0.85f));
        g.fillRoundedRectangle (area, 18.0f);
        g.setColour (palette.accent);
        g.drawRoundedRectangle (area, 18.0f, 2.0f);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText ("Drop to load this synth", area, juce::Justification::centred);
    };
    addChildComponent (overlay);

    // 第一次用到多个音源副本时提醒一次：音源自带的混响、延迟会被每个副本各加一遍
    copiesNotice.text = "Curvable now runs several copies of your synth so notes can bend on their own. "
                        "Turn off the synth's own reverb and delay and use FL's effects instead, or they will stack up.";
    copiesNotice.dismissButton.onClick = [this]
    {
        context.getSettings().setValue (copiesNoticeKey, true);
        context.getSettings().saveIfNeeded();
        updateNotice();
    };
    addChildComponent (copiesNotice);

    slotsPanel.onClose = [this] { setSlotsOpen (false); };
    addChildComponent (slotsPanel);

    slotsHandle.setTooltip ("Automation slots: let FL automate knobs of the synth");
    slotsHandle.setWantsKeyboardFocus (false);
    slotsHandle.onClick = [this] { setSlotsOpen (true); };
    addChildComponent (slotsHandle);
    processor.getAutomationSlots().changed.addChangeListener (this);

    processor.projectChanged.addChangeListener (this);
    hosted.addChangeListener (this);
    startTimerHz (4);

    setResizable (true, true);
    setResizeLimits (defaultMinWidth, defaultMinHeight, 4096, 2160);
    setSize (1000, 620);
    refresh();

    // 已经加载了音源时，打开窗口直接显示音源页
    if (hosted.isLoaded())
        setPage (Page::synth);
}

CurvableEditor::~CurvableEditor()
{
    stopTimer();
    processor.projectChanged.removeChangeListener (this);
    hosted.removeChangeListener (this);
    processor.getAutomationSlots().changed.removeChangeListener (this);
    detachEmbeddedEditor();

    // 窗口关闭时销毁音源界面，下次打开再重建（插件的原生子窗口不能比我们的窗口活得更久）
    hosted.releaseEditor();
    setLookAndFeel (nullptr);
}

//==============================================================================
bool CurvableEditor::isCompact() const noexcept
{
    return getWidth() < compactBelowWidth;
}

juce::Rectangle<int> CurvableEditor::getBodyArea() const
{
    auto area = getLocalBounds().withTrimmedTop (toolbarAreaHeight);

    // Synth 页：音源界面紧贴工具栏下方、左右不留边；卷帘页：四周留出边距
    if (page == Page::synth)
        return area.withTrimmedRight (getSlotsWidth());

    return area.withTrimmedTop (bodyGap).reduced (margin, 0).withTrimmedBottom (margin);
}

void CurvableEditor::resized()
{
    const bool compact = isCompact();
    const int side = compact ? 10 : margin;
    auto toolbar = juce::Rectangle<int> (side, toolbarTop, getWidth() - side * 2, toolbarHeight);

    // Logo（窄窗口时省略）
    logoBounds = {};
    if (! compact)
    {
        logoBounds = toolbar.removeFromLeft (150);
        toolbar.removeFromLeft (8);
    }

    // 分段按钮组：凹槽 + 按钮
    auto layoutGroup = [&] (juce::Rectangle<int>& groupBounds, std::initializer_list<std::pair<juce::Button*, int>> buttons)
    {
        int total = 8;
        for (auto& [b, w] : buttons)
            total += w;

        groupBounds = toolbar.removeFromLeft (total).withSizeKeepingCentre (total, 44);
        auto inner = groupBounds.reduced (4);

        for (auto& [b, w] : buttons)
            b->setBounds (inner.removeFromLeft (w));

        toolbar.removeFromLeft (compact ? 8 : 14);
    };

    layoutGroup (tabsBounds, { { &pianoRollTab, compact ? 86 : 104 }, { &synthTab, compact ? 62 : 76 } });
    layoutGroup (modesBounds, { { &autoModeButton, compact ? 52 : 64 }, { &triggerModeButton, compact ? 76 : 86 },
                                { &offModeButton, compact ? 44 : 54 } });

    settingsButton.setBounds (toolbar.removeFromRight (48).withSizeKeepingCentre (48, 48));
    toolbar.removeFromRight (6);
    synthButton.setBounds (toolbar.removeFromRight (juce::jmin (220, toolbar.getWidth())).withSizeKeepingCentre (juce::jmin (220, toolbar.getWidth()), 48));

    // 主体
    const auto body = getBodyArea();
    pianoRoll.setBounds (body);
    browser.setBounds (getLocalBounds().withTrimmedTop (toolbarAreaHeight).reduced (page == Page::synth ? 0 : margin, 0)
                           .withTrimmedBottom (page == Page::synth ? 0 : margin));
    overlay.setBounds (getLocalBounds().withTrimmedTop (toolbarAreaHeight));
    slotsPanel.setBounds (getLocalBounds().withTrimmedTop (toolbarAreaHeight).removeFromRight (getSlotsWidth()));
    slotsHandle.setBounds (slotsPanel.getBounds());
    updateNotice();

    if (auto* ed = embeddedEditor.getComponent(); ed != nullptr && ed->isVisible())
    {
        const juce::ScopedValueSetter<bool> svs (layingOut, true);

        // 可调整大小的音源界面填满主体区域；其余的保持自身大小并居中
        if (ed->isResizable())
            ed->setBounds (body);

        ed->setTopLeftPosition (body.getCentre() - ed->getLocalBounds().getCentre());
    }

    rebuildChrome();
}

void CurvableEditor::rebuildChrome()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    const auto scale = juce::Component::getApproximateScaleFactorForComponent (this);
    chrome = juce::Image (juce::Image::ARGB, juce::roundToInt (getWidth() * scale), juce::roundToInt (toolbarAreaHeight * scale), true);

    juce::Graphics g (chrome);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (palette.bg);

    if (! logoBounds.isEmpty())
    {
        const auto badge = logoBounds.withWidth (logoBounds.getHeight()).reduced (7).toFloat();
        neu::raised (g, badge, 12.0f, palette, 3);

        juce::Path curve;
        const auto b = badge.reduced (9.0f);
        curve.startNewSubPath (b.getX(), b.getBottom() - 3.0f);
        curve.cubicTo (b.getX() + b.getWidth() * 0.45f, b.getBottom() - 3.0f,
                       b.getX() + b.getWidth() * 0.3f, b.getY() + 3.0f,
                       b.getCentreX() + 2.0f, b.getY() + 3.0f);
        curve.quadraticTo (b.getRight() - 3.0f, b.getY() + 3.0f, b.getRight(), b.getCentreY());
        g.setColour (palette.accent);
        g.strokePath (curve, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (palette.text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText ("Curvable", logoBounds.withTrimmedLeft (logoBounds.getHeight() + 2), juce::Justification::centredLeft);
    }

    neu::inset (g, tabsBounds.toFloat(), 14.0f, palette, 3);
    neu::inset (g, modesBounds.toFloat(), 14.0f, palette, 3);
}

void CurvableEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette.bg);
    g.drawImage (chrome, juce::Rectangle<float> (0.0f, 0.0f, (float) getWidth(), (float) toolbarAreaHeight));

    // Synth 页：没有音源界面时显示状态
    const bool showingSynth = embeddedEditor != nullptr && embeddedEditor->isVisible();

    if (page == Page::synth && ! showingSynth && ! browser.isVisible())
    {
        g.setColour (statusIsError ? palette.error : palette.muted);
        g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText (statusText, getBodyArea().reduced (40), juce::Justification::centred, 4);
    }
}

//==============================================================================
void CurvableEditor::setPage (Page newPage)
{
    page = newPage;
    pianoRollTab.setToggleState (page == Page::pianoRoll, juce::dontSendNotification);
    synthTab.setToggleState (page == Page::synth, juce::dontSendNotification);
    refresh();
    resized();
}

void CurvableEditor::updateEmbeddedEditor()
{
    // 嵌入的音源界面是原生子窗口，会盖住 JUCE 绘制的浮层，所以浏览面板或拖拽提示出现时要隐藏它
    const bool wantVisible = page == Page::synth && hosted.isLoaded() && ! loading
                             && ! browser.isVisible() && ! dragHighlight;

    pianoRoll.setVisible (page == Page::pianoRoll);
    slotsPanel.setVisible (page == Page::synth && slotsOpen && ! browser.isVisible());
    slotsHandle.setVisible (page == Page::synth && ! slotsOpen && ! browser.isVisible());

    if (! wantVisible)
    {
        if (embeddedEditor != nullptr)
            embeddedEditor->setVisible (false);

        // 仍在 Synth 页（例如打开了浏览面板）时保持窗口大小，避免跳动
        if (page != Page::synth || ! hosted.isLoaded())
            leaveSynthLayout();

        return;
    }

    auto* ed = hosted.getOrCreateEditor();

    if (ed != embeddedEditor.getComponent())
    {
        detachEmbeddedEditor();
        embeddedEditor = ed;

        if (ed != nullptr)
        {
            addChildComponent (ed);
            ed->addComponentListener (this);
        }
    }

    if (ed == nullptr)
        return;

    ed->setVisible (true);
    fitWindowToSynth (*ed);
    resized();
}

void CurvableEditor::fitWindowToSynth (juce::AudioProcessorEditor& ed)
{
    if (! synthLayoutActive)
    {
        pianoRollSize = { getWidth(), getHeight() };
        synthLayoutActive = true;
    }

    const auto width  = juce::jmax (synthPageMinWidth, ed.getWidth() + getSlotsWidth());
    const auto height = juce::jmax (synthPageMinHeight, ed.getHeight() + toolbarAreaHeight);

    if (ed.isResizable())
    {
        // 音源界面可调整大小：窗口也可以拖动，音源界面跟随填满
        auto minW = synthPageMinWidth, minH = synthPageMinHeight;

        if (auto* synthConstrainer = ed.getConstrainer())
        {
            minW = juce::jmax (minW, synthConstrainer->getMinimumWidth() + getSlotsWidth());
            minH = juce::jmax (minH, synthConstrainer->getMinimumHeight() + toolbarAreaHeight);
        }

        setResizeLimits (minW, minH, 8192, 8192);
    }
    else
    {
        // 固定大小的音源界面：窗口锁定为刚好装下它
        setResizeLimits (width, height, width, height);
    }

    if (getWidth() != width || getHeight() != height)
        setSize (width, height);
}

void CurvableEditor::leaveSynthLayout()
{
    if (! synthLayoutActive)
        return;

    synthLayoutActive = false;
    setResizeLimits (defaultMinWidth, defaultMinHeight, 4096, 2160);

    if (page == Page::pianoRoll)
        setSize (juce::jmax (defaultMinWidth, pianoRollSize.x), juce::jmax (defaultMinHeight, pianoRollSize.y));
}

void CurvableEditor::detachEmbeddedEditor()
{
    if (auto* ed = embeddedEditor.getComponent())
    {
        ed->removeComponentListener (this);
        removeChildComponent (ed);
    }

    embeddedEditor = nullptr;
}

void CurvableEditor::componentMovedOrResized (juce::Component& comp, bool, bool wasResized)
{
    // 音源自己改变了界面大小（例如切换界面缩放），让窗口重新贴合；由我们引起的变化忽略
    if (wasResized && ! layingOut && &comp == embeddedEditor.getComponent())
        updateEmbeddedEditor();
}

void CurvableEditor::componentBeingDeleted (juce::Component& comp)
{
    // HostedInstrument 切换或卸载音源时会删除旧界面
    if (&comp == embeddedEditor.getComponent())
    {
        comp.removeComponentListener (this);
        embeddedEditor = nullptr;
    }
}

//==============================================================================
void CurvableEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.getAutomationSlots().changed)
    {
        slotsHandle.repaint(); // 绑定的圆点
        return;
    }

    refresh();
}

void CurvableEditor::refresh()
{
    const auto name = hosted.getPluginName();

    if (loading)
        synthButton.setButtonText ("Loading...");
    else if (hosted.isLoaded())
    {
        // 多个实例时显示数量；还在创建时显示进度
        const auto have = hosted.getNumInstances(), want = hosted.getWantedInstances();

        if (have < want)
            synthButton.setButtonText (name + "  (" + juce::String (have) + "/" + juce::String (want) + ")");
        else if (have > 1)
        {
            // 副本数，括号里是正在运行的（其余因空闲而暂停）
            const auto awake = hosted.getNumAwakeInstances();
            shownAwake = awake;
            synthButton.setButtonText (name + juce::String::fromUTF8 ("  ×") + juce::String (have)
                                       + (awake < have ? " (" + juce::String (awake) + " on)" : juce::String()));
        }
        else
            synthButton.setButtonText (name);

        synthButton.setTooltip (have > 1 || want > 1
                                    ? juce::String (have) + " copies of the synth let notes bend independently. "
                                      "Idle copies pause to save CPU (\"on\" = currently running)."
                                    : juce::String());
    }
    else if (hosted.isMissing())
        synthButton.setButtonText ("Missing: " + name);
    else
        synthButton.setButtonText ("Load synth");

    synthButton.getProperties().set ("dot", hosted.isMissing() ? "error" : (hosted.isLoaded() ? "accent" : ""));
    synthButton.repaint();
    synthTab.setButtonText (hosted.isMissing() ? "Synth (!)" : "Synth");

    statusIsError = message.isNotEmpty() ? messageIsError
                                         : (hosted.getLastError().isNotEmpty() || hosted.getCloneError().isNotEmpty());

    if (message.isNotEmpty())
        statusText = message;
    else if (hosted.getLastError().isNotEmpty())
        statusText = hosted.getLastError();
    else if (hosted.getCloneError().isNotEmpty())
        statusText = "Could not create another copy of the synth (" + hosted.getCloneError()
                     + "). Some bent notes play unbent for now; retrying shortly.";
    else if (loading)
        statusText = "Loading synth...";
    else if (hosted.isLoaded())
        statusText = {};
    else
        statusText = "No synth loaded. Click \"Load synth\" or drag a plugin here.";

    // 卷帘页底部只显示快捷键提示；音源状态由右上角的音源按钮表示，只有出错（例如加载失败）时才占用这里
    pianoRoll.setStatus (statusIsError ? statusText : juce::String(), statusIsError);

    refreshModeButtons();
    updateEmbeddedEditor();
    updateNotice();
    repaint();
}

void CurvableEditor::setSlotsOpen (bool shouldBeOpen)
{
    slotsOpen = shouldBeOpen;

    if (slotsOpen && page != Page::synth)
    {
        setPage (Page::synth); // 会重新布局
        return;
    }

    if (! slotsOpen)
        processor.getAutomationSlots().cancelLearn();

    // 窗口宽度随面板变化；没有嵌入音源界面时只需重新布局
    updateEmbeddedEditor();
    resized();
    repaint();
}

void CurvableEditor::updateNotice()
{
    const bool show = page == Page::pianoRoll && hosted.isLoaded() && hosted.getNumInstances() > 1
                      && ! browser.isVisible() && ! context.getSettings().getBoolValue (copiesNoticeKey, false);

    if (show)
    {
        const auto body = getBodyArea();
        const auto width = juce::jmin (680, body.getWidth() - 40);
        copiesNotice.setBounds (juce::Rectangle<int> (width, 58).withCentre ({ body.getCentreX(), body.getY() + 80 }));
        copiesNotice.toFront (false);
    }

    copiesNotice.setVisible (show);
}

void CurvableEditor::timerCallback()
{
    // 运行中的实例数随播放变化，没有 change 消息，定时检查
    if (hosted.getNumInstances() > 1 && hosted.getNumAwakeInstances() != shownAwake && ! loading)
        refresh();
}

void CurvableEditor::refreshModeButtons()
{
    const auto mode = processor.getProject().mode;
    autoModeButton.setToggleState (mode == PlaybackMode::automatic, juce::dontSendNotification);
    triggerModeButton.setToggleState (mode == PlaybackMode::trigger, juce::dontSendNotification);
    offModeButton.setToggleState (mode == PlaybackMode::off, juce::dontSendNotification);
}

//==============================================================================
void CurvableEditor::applyTheme (bool dark)
{
    palette = dark ? Palette::night() : Palette::day();
    lookAndFeel.setPalette (palette);
    browser.setPalette (palette);

    context.getSettings().setValue (themeKey, dark ? "night" : "day");
    context.getSettings().saveIfNeeded();

    rebuildChrome();
    pianoRoll.paletteChanged();
    slotsPanel.paletteChanged();
    sendLookAndFeelChange();
    repaint();
}

void CurvableEditor::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Theme");
    menu.addItem (dayThemeId, "Day", true, ! palette.isDark);
    menu.addItem (nightThemeId, "Night", true, palette.isDark);
    menu.addSeparator();
    menu.addItem (testPhraseId, "Insert test phrase");
    menu.addItem (clearNotesId, "Clear all notes", ! processor.getProject().notes.empty());
    menu.addSeparator();
    menu.addItem (syncCopiesId, "Sync synth copies now", hosted.getNumInstances() > 1);
    menu.addItem (pluginManagerId, "Plugin manager...");
    menu.addItem (shortcutsId, "Mouse & keyboard shortcuts (F1)");

    juce::Component::SafePointer<CurvableEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&settingsButton), [safeThis] (int result)
    {
        auto* self = safeThis.getComponent();
        if (self == nullptr)
            return;

        auto replaceNotes = [self] (std::vector<Note> notes, NoteId nextId, const juce::String& name)
        {
            const auto before = self->processor.getProject();
            auto project = before;
            project.notes = std::move (notes);
            project.nextId = std::max (project.nextId, nextId);
            self->processor.setProject (std::move (project), false);
            self->processor.commitEdit (before, name);
        };

        switch (result)
        {
            case dayThemeId:   self->applyTheme (false); break;
            case nightThemeId: self->applyTheme (true); break;

            case testPhraseId:
            {
                // 测试乐句的音符 id 接在现有 id 之后，撤销 / 重做不会冲突
                auto phrase = Project::createTestPhrase();
                const auto offset = self->processor.getProject().nextId;
                for (auto& n : phrase.notes)
                    n.id += offset;
                replaceNotes (phrase.notes, phrase.nextId + offset, "Insert test phrase");
                break;
            }

            case clearNotesId:    replaceNotes ({}, 0, "Clear all notes"); break;
            case pluginManagerId: self->showPluginManager(); break;
            case syncCopiesId:    self->hosted.syncClones(); break;

            case shortcutsId:
                self->setPage (Page::pianoRoll);
                self->pianoRoll.showShortcuts (true);
                break;
            default: break;
        }
    });
}

//==============================================================================
void CurvableEditor::showSynthBrowser()
{
    const auto current = hosted.getLoadedDescription();
    SynthEntry currentEntry;

    if (current.has_value())
        currentEntry.description = current;

    browser.setEntries (context.getSynthCatalog(),
                        current.has_value() ? currentEntry.getKey() : juce::String(),
                        hosted.isLoaded() || hosted.isMissing());
    browser.setVisible (true);
    browser.toFront (false);
    browser.focusSearch();
    updateEmbeddedEditor();
    repaint();
}

void CurvableEditor::hideSynthBrowser()
{
    if (! browser.isVisible())
        return;

    browser.setVisible (false);
    updateEmbeddedEditor();
    repaint();
}

void CurvableEditor::loadDescription (const juce::PluginDescription& desc)
{
    message = {};
    loading = true;
    refresh();

    juce::Component::SafePointer<CurvableEditor> safeThis (this);
    const auto previousPlugin = hosted.getPluginName() + "|" + hosted.getLoadedIdentifier();

    hosted.load (desc, [safeThis, previousPlugin] (const juce::String& error)
    {
        if (auto* editor = safeThis.getComponent())
        {
            if (error.isEmpty())
            {
                // 换成了别的音源：旧的参数绑定没有意义了
                if (previousPlugin != editor->hosted.getPluginName() + "|" + editor->hosted.getLoadedIdentifier())
                    editor->processor.getAutomationSlots().clearAll();

                if (const auto loaded = editor->hosted.getLoadedDescription())
                    editor->context.markRecentlyUsed (*loaded);

                editor->processor.markStateChanged();
            }

            editor->loading = false;

            // 加载成功后直接显示音源界面，方便马上调音色
            editor->setPage (error.isEmpty() ? Page::synth : editor->page);
        }
    });
}

void CurvableEditor::showMessage (const juce::String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    refresh();
}

//==============================================================================
bool CurvableEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (context.isPotentialPluginFile (context.resolveDroppedFile (juce::File (f))))
            return true;

    return false;
}

void CurvableEditor::fileDragEnter (const juce::StringArray&, int, int)
{
    hideSynthBrowser();
    dragHighlight = true;
    overlay.setVisible (true);
    overlay.toFront (false);
    overlay.repaint();
    updateEmbeddedEditor();
}

void CurvableEditor::fileDragExit (const juce::StringArray&)
{
    dragHighlight = false;
    overlay.setVisible (false);
    updateEmbeddedEditor();
}

void CurvableEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHighlight = false;
    overlay.setVisible (false);
    updateEmbeddedEditor();
    hideSynthBrowser();

    for (const auto& f : files)
    {
        // FL 浏览器里拖来的 .fst 会被换算成对应的插件文件
        if (const auto file = context.resolveDroppedFile (juce::File (f)); context.isPotentialPluginFile (file))
        {
            loadPluginFile (file);
            return;
        }
    }
}

void CurvableEditor::browseForPluginFile()
{
    auto startDir = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);

   #if JUCE_WINDOWS
    startDir = juce::File (juce::SystemStats::getEnvironmentVariable ("CommonProgramFiles", startDir.getFullPathName()))
                   .getChildFile ("VST3");
   #elif JUCE_MAC
    startDir = juce::File ("/Library/Audio/Plug-Ins");
   #endif

    fileChooser = std::make_unique<juce::FileChooser> ("Choose a synth plugin", startDir, "*.vst3;*.component");

    juce::Component::SafePointer<CurvableEditor> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (auto* self = safeThis.getComponent())
                                      if (const auto file = chooser.getResult(); file != juce::File())
                                          self->loadPluginFile (file);
                              });
}

void CurvableEditor::loadPluginFile (const juce::File& file)
{
    showMessage ("Reading " + file.getFileName() + "...", false);

    // 读取插件需要一两秒；稍后执行，让"正在读取"的提示先显示出来
    juce::Component::SafePointer<CurvableEditor> safeThis (this);

    juce::Timer::callAfterDelay (30, [safeThis, file]
    {
        auto* self = safeThis.getComponent();

        if (self == nullptr)
            return;

        const auto instruments = self->context.addPluginFile (file);

        if (instruments.isEmpty())
        {
            self->showMessage ("Could not load a synth from " + file.getFileName()
                                   + ". It may not be an instrument, or it failed to start.", true);
            return;
        }

        if (instruments.size() == 1)
        {
            self->loadDescription (instruments.getReference (0));
            return;
        }

        // 一个文件里有多个乐器（例如插件合集），让用户选
        self->message = {};
        self->refresh();

        juce::PopupMenu menu;
        juce::KnownPluginList::addToMenu (menu, instruments, juce::KnownPluginList::sortAlphabetically);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&self->synthButton),
                            [safeThis, instruments] (int result)
                            {
                                const auto index = juce::KnownPluginList::getIndexChosenByMenu (instruments, result);

                                if (auto* editor = safeThis.getComponent(); editor != nullptr && index >= 0)
                                    editor->loadDescription (instruments.getReference (index));
                            });
    });
}

void CurvableEditor::showPluginManager()
{
    if (pluginManagerWindow != nullptr)
    {
        pluginManagerWindow->toFront (true);
        return;
    }

    juce::Component::SafePointer<CurvableEditor> safeThis (this);
    pluginManagerWindow = std::make_unique<PluginManagerWindow> (context, palette.bg, [safeThis]
    {
        juce::MessageManager::callAsync ([safeThis]
        {
            if (auto* self = safeThis.getComponent())
            {
                self->pluginManagerWindow = nullptr;
                self->refresh(); // 扫描后可能有了新的音源
            }
        });
    });
}

} // namespace curvable
