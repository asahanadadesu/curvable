#include "plugin/PluginEditor.h"

namespace curvable
{

namespace
{
    const auto backgroundColour = juce::Colour (0xff12141a);
    const auto toolbarColour    = juce::Colour (0xff1b1e26);
    const auto accentColour     = juce::Colour (0xff5ad1a0);
    const auto errorColour      = juce::Colour (0xffe0796b);

    constexpr int toolbarHeight = 44;
    constexpr int tabRadioGroup = 1001;

    constexpr int defaultMinWidth  = 640;
    constexpr int defaultMinHeight = 400;

    // Synth 页的最小尺寸：工具栏要放得下；音源界面比这小时两侧留黑边
    constexpr int synthPageMinWidth  = 560;
    constexpr int synthPageMinHeight = 300;

    /** 插件扫描与管理窗口（JUCE 自带的 PluginListComponent）。 */
    class PluginManagerWindow final : public juce::DocumentWindow
    {
    public:
        PluginManagerWindow (HostingContext& context, std::function<void()> onCloseRequested)
            : DocumentWindow ("Curvable - Plugin manager", backgroundColour, DocumentWindow::closeButton),
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
}

//==============================================================================
CurvableEditor::CurvableEditor (CurvableProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      hosted (p.getHostedInstrument()),
      context (p.getHostingContext())
{
    synthButton.onClick = [this] { browser.isVisible() ? hideSynthBrowser() : showSynthBrowser(); };
    addAndMakeVisible (synthButton);

    for (auto* tab : { &pianoRollTab, &synthTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (tabRadioGroup);
        tab->setColour (juce::TextButton::buttonOnColourId, accentColour.withAlpha (0.25f));
        addAndMakeVisible (tab);
    }

    pianoRollTab.setToggleState (true, juce::dontSendNotification);
    pianoRollTab.onClick = [this] { if (pianoRollTab.getToggleState()) setPage (Page::pianoRoll); };
    synthTab.onClick     = [this] { if (synthTab.getToggleState())     setPage (Page::synth); };

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
    browser.onUnload        = [this] { hideSynthBrowser(); message = {}; hosted.unload(); };
    addChildComponent (browser);

    hosted.addChangeListener (this);
    refresh();

    setResizable (true, true);
    setResizeLimits (defaultMinWidth, defaultMinHeight, 4096, 2160);
    setSize (1000, 620);

    // 已经加载了音源时，打开窗口直接显示音源页
    if (hosted.isLoaded())
        setPage (Page::synth);
}

CurvableEditor::~CurvableEditor()
{
    hosted.removeChangeListener (this);
    detachEmbeddedEditor();

    // 窗口关闭时销毁音源界面，下次打开再重建（插件的原生子窗口不能比我们的窗口活得更久）
    hosted.releaseEditor();
}

//==============================================================================
void CurvableEditor::paint (juce::Graphics& g)
{
    g.fillAll (backgroundColour);

    auto bounds = getLocalBounds();
    auto toolbar = bounds.removeFromTop (toolbarHeight);

    g.setColour (toolbarColour);
    g.fillRect (toolbar);

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("Curvable", toolbar.removeFromLeft (130).withTrimmedLeft (16), juce::Justification::centredLeft);

    // 主体区域：Synth 页显示音源界面；卷帘页目前只显示状态，M3 替换为卷帘编辑器
    const bool showingSynth = embeddedEditor != nullptr && embeddedEditor->isVisible();

    if (showingSynth)
    {
        g.setColour (juce::Colours::black);
        g.fillRect (getBodyArea());
    }
    else
    {
        auto body = bounds.reduced (24);

        g.setColour (statusIsError ? errorColour : juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (16.0f));
        g.drawFittedText (statusText, body.removeFromTop (body.getHeight() / 2).withTrimmedTop (body.getHeight() / 3),
                          juce::Justification::centred, 3);
    }

    if (dragHighlight)
    {
        auto dropArea = getLocalBounds().withTrimmedTop (toolbarHeight).reduced (12).toFloat();
        g.setColour (accentColour.withAlpha (0.08f));
        g.fillRoundedRectangle (dropArea, 10.0f);
        g.setColour (accentColour);
        g.drawRoundedRectangle (dropArea, 10.0f, 2.0f);
        g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
        g.drawText ("Drop to load this synth", dropArea, juce::Justification::centred);
    }

    if (! showingSynth)
    {
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("v" CURVABLE_VERSION_STRING " - M1 pass-through mode", getLocalBounds().removeFromBottom (28).withTrimmedRight (12),
                    juce::Justification::centredRight);
    }
}

juce::Rectangle<int> CurvableEditor::getBodyArea() const
{
    return getLocalBounds().withTrimmedTop (toolbarHeight);
}

void CurvableEditor::resized()
{
    auto toolbar = getLocalBounds().removeFromTop (toolbarHeight).reduced (8, 7);
    toolbar.removeFromLeft (130);

    pianoRollTab.setBounds (toolbar.removeFromLeft (100));
    toolbar.removeFromLeft (4);
    synthTab.setBounds (toolbar.removeFromLeft (80));

    synthButton.setBounds (toolbar.removeFromRight (juce::jmin (280, toolbar.getWidth() / 2)));

    browser.setBounds (getBodyArea());

    if (auto* ed = embeddedEditor.getComponent(); ed != nullptr && ed->isVisible())
    {
        const juce::ScopedValueSetter<bool> svs (layingOut, true);

        // 可调整大小的音源界面填满主体区域；其余的保持自身大小并居中
        if (ed->isResizable())
            ed->setBounds (getBodyArea());

        ed->setTopLeftPosition (getBodyArea().getCentre() - ed->getLocalBounds().getCentre());
    }
}

//==============================================================================
void CurvableEditor::setPage (Page newPage)
{
    page = newPage;
    pianoRollTab.setToggleState (page == Page::pianoRoll, juce::dontSendNotification);
    synthTab.setToggleState (page == Page::synth, juce::dontSendNotification);
    updateEmbeddedEditor();
    repaint();
}

void CurvableEditor::updateEmbeddedEditor()
{
    // 嵌入的音源界面是原生子窗口，会盖住 JUCE 绘制的浮层，所以浏览面板或拖拽提示出现时要隐藏它
    const bool wantVisible = page == Page::synth && hosted.isLoaded() && ! loading
                             && ! browser.isVisible() && ! dragHighlight;

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

    const auto width  = juce::jmax (synthPageMinWidth, ed.getWidth());
    const auto height = juce::jmax (synthPageMinHeight, ed.getHeight() + toolbarHeight);

    if (ed.isResizable())
    {
        // 音源界面可调整大小：窗口也可以拖动，音源界面跟随填满
        auto minW = synthPageMinWidth, minH = synthPageMinHeight;

        if (auto* synthConstrainer = ed.getConstrainer())
        {
            minW = juce::jmax (minW, synthConstrainer->getMinimumWidth());
            minH = juce::jmax (minH, synthConstrainer->getMinimumHeight() + toolbarHeight);
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
        setSize (pianoRollSize.x, pianoRollSize.y);
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
void CurvableEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void CurvableEditor::refresh()
{
    const auto name = hosted.getPluginName();

    if (loading)
        synthButton.setButtonText ("Loading...");
    else if (hosted.isLoaded())
        synthButton.setButtonText ("Synth: " + name + "  v");
    else if (hosted.isMissing())
        synthButton.setButtonText ("Missing: " + name + "  v");
    else
        synthButton.setButtonText ("Load synth...  v");

    synthButton.setColour (juce::TextButton::textColourOffId,
                           hosted.isMissing() ? errorColour : (hosted.isLoaded() ? accentColour : juce::Colours::white));

    synthTab.setButtonText (hosted.isMissing() ? "Synth (!)" : "Synth");

    statusIsError = message.isNotEmpty() ? messageIsError : hosted.getLastError().isNotEmpty();

    if (message.isNotEmpty())
        statusText = message;
    else if (hosted.getLastError().isNotEmpty())
        statusText = hosted.getLastError();
    else if (loading)
        statusText = "Loading synth...";
    else if (hosted.isLoaded())
        statusText = page == Page::pianoRoll
                         ? "Play notes from the host's piano roll - they are passed straight to " + name + ".\n"
                           "Curvable's own piano roll will live here."
                         : name + " is loaded.";
    else
        statusText = "Click \"Load synth\" to pick one of your synths, or drag a plugin here.";

    updateEmbeddedEditor();
    repaint();
}

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
}

void CurvableEditor::hideSynthBrowser()
{
    if (! browser.isVisible())
        return;

    browser.setVisible (false);
    updateEmbeddedEditor();
}

void CurvableEditor::loadDescription (const juce::PluginDescription& desc)
{
    message = {};
    loading = true;
    refresh();

    juce::Component::SafePointer<CurvableEditor> safeThis (this);

    hosted.load (desc, [safeThis] (const juce::String& error)
    {
        if (auto* editor = safeThis.getComponent())
        {
            if (error.isEmpty())
                if (const auto loaded = editor->hosted.getLoadedDescription())
                    editor->context.markRecentlyUsed (*loaded);

            editor->loading = false;

            // 加载成功后直接显示音源界面，方便马上调音色
            if (error.isEmpty())
                editor->page = Page::synth;

            editor->setPage (editor->page);
            editor->refresh();
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
    updateEmbeddedEditor();
    repaint();
}

void CurvableEditor::fileDragExit (const juce::StringArray&)
{
    dragHighlight = false;
    updateEmbeddedEditor();
    repaint();
}

void CurvableEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHighlight = false;
    updateEmbeddedEditor();
    repaint();

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
    pluginManagerWindow = std::make_unique<PluginManagerWindow> (context, [safeThis]
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
