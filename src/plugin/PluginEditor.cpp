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
    synthButton.onClick  = [this] { browser.isVisible() ? hideSynthBrowser() : showSynthBrowser(); };
    editorButton.onClick = [this] { hosted.isEditorShowing() ? hosted.closeEditor() : hosted.showEditor(); };

    addAndMakeVisible (synthButton);
    addAndMakeVisible (editorButton);

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
    setResizeLimits (640, 400, 4096, 2160);
    setSize (1000, 620);
}

CurvableEditor::~CurvableEditor()
{
    hosted.removeChangeListener (this);
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

    // 主体区域：M1 阶段只显示状态，后续替换为卷帘编辑器
    auto body = bounds.reduced (24);

    g.setColour (statusIsError ? errorColour : juce::Colours::white.withAlpha (0.75f));
    g.setFont (juce::FontOptions (16.0f));
    g.drawFittedText (statusText, body.removeFromTop (body.getHeight() / 2).withTrimmedTop (body.getHeight() / 3),
                      juce::Justification::centred, 3);

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

    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("v" CURVABLE_VERSION_STRING " - M1 pass-through mode", getLocalBounds().removeFromBottom (28).withTrimmedRight (12),
                juce::Justification::centredRight);
}

void CurvableEditor::resized()
{
    auto toolbar = getLocalBounds().removeFromTop (toolbarHeight).reduced (8, 7);
    toolbar.removeFromLeft (130);

    synthButton.setBounds (toolbar.removeFromLeft (260));
    toolbar.removeFromLeft (8);
    editorButton.setBounds (toolbar.removeFromLeft (110));

    browser.setBounds (getLocalBounds().withTrimmedTop (toolbarHeight));
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

    editorButton.setEnabled (hosted.isLoaded());
    editorButton.setButtonText (hosted.isEditorShowing() ? "Hide synth" : "Show synth");

    statusIsError = message.isNotEmpty() ? messageIsError : hosted.getLastError().isNotEmpty();

    if (message.isNotEmpty())
        statusText = message;
    else if (hosted.getLastError().isNotEmpty())
        statusText = hosted.getLastError();
    else if (loading)
        statusText = "Loading synth...";
    else if (hosted.isLoaded())
        statusText = "Play notes from the host's piano roll - they are passed straight to " + name + ".";
    else
        statusText = "Click \"Load synth\" to pick one of your synths, or drag a plugin here.";

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
}

void CurvableEditor::hideSynthBrowser()
{
    browser.setVisible (false);
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
    repaint();
}

void CurvableEditor::fileDragExit (const juce::StringArray&)
{
    dragHighlight = false;
    repaint();
}

void CurvableEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragHighlight = false;
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
