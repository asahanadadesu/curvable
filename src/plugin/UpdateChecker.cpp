#include "plugin/UpdateChecker.h"

#include "model/Version.h"

namespace curvable
{

namespace
{
    constexpr auto releaseApi = "https://api.github.com/repos/asahanadadesu/curvable/releases/latest";
    constexpr auto downloadPrefix = "https://github.com/asahanadadesu/curvable/";

    constexpr auto autoCheckKey   = "checkForUpdates";
    constexpr auto lastCheckKey   = "updateLastCheck";
    constexpr auto latestKey      = "updateLatestVersion";
    constexpr auto latestUrlKey   = "updateLatestUrl";
    constexpr auto dismissedKey   = "updateDismissedVersion";

    constexpr juce::int64 checkInterval = 24 * 60 * 60 * 1000; ///< 自动检查的间隔（毫秒）
}

UpdateChecker::UpdateChecker() : juce::Thread ("Curvable update check")
{
    self = this; // 后台线程只复制它，不在那边创建
}

UpdateChecker::~UpdateChecker()
{
    stopThread (10000);
}

juce::String UpdateChecker::currentVersion()
{
    return CURVABLE_VERSION_STRING;
}

bool UpdateChecker::isAutoCheckEnabled() const
{
    return settings().getBoolValue (autoCheckKey, true);
}

void UpdateChecker::setAutoCheckEnabled (bool enabled)
{
    settings().setValue (autoCheckKey, enabled);
    settings().saveIfNeeded();
}

void UpdateChecker::checkIfDue()
{
    const auto last = settings().getValue (lastCheckKey).getLargeIntValue();
    const auto now = juce::Time::currentTimeMillis();

    if (isAutoCheckEnabled() && (now - last >= checkInterval || now < last))
        start (false);
}

void UpdateChecker::checkNow()
{
    start (true);
}

void UpdateChecker::start (bool isManual)
{
    manual = manual || isManual;

    if (state == State::checking)
        return;

    manual = isManual;
    state = State::checking;
    sendChangeMessage();
    startThread (juce::Thread::Priority::low);
}

void UpdateChecker::run()
{
    std::optional<Release> result;
    int status = 0;

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: Curvable/" + currentVersion())
                             .withConnectionTimeoutMs (8000)
                             .withStatusCode (&status);

    if (auto stream = juce::URL (releaseApi).createInputStream (options); stream != nullptr && ! threadShouldExit())
    {
        const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
        const auto tag = json["tag_name"].toString();

        if (status == 200 && version::parse (tag.toStdString()).has_value())
        {
            Release release;
            release.version = tag.trimCharactersAtStart ("vV");
            release.downloadUrl = json["html_url"].toString();

           #if JUCE_WINDOWS
            if (const auto* assets = json["assets"].getArray())
                for (const auto& asset : *assets)
                    if (asset["name"].toString().endsWithIgnoreCase ("-Windows-Setup.exe"))
                        release.downloadUrl = asset["browser_download_url"].toString();
           #endif

            result = release;
        }
    }

    if (threadShouldExit())
        return;

    juce::MessageManager::callAsync ([ref = self, result]
    {
        if (auto* checker = ref.get())
            checker->finished (result);
    });
}

void UpdateChecker::finished (std::optional<Release> result)
{
    state = result.has_value() ? State::done : State::failed;

    if (result.has_value())
    {
        settings().setValue (lastCheckKey, juce::String (juce::Time::currentTimeMillis()));
        settings().setValue (latestKey, result->version);
        settings().setValue (latestUrlKey, result->downloadUrl);
        settings().saveIfNeeded();
    }

    sendChangeMessage();
}

std::optional<UpdateChecker::Release> UpdateChecker::getNewerRelease() const
{
    Release release { settings().getValue (latestKey), settings().getValue (latestUrlKey) };

    if (! version::isNewer (release.version.toStdString(), currentVersion().toStdString()))
        return std::nullopt;

    return release;
}

bool UpdateChecker::isDismissed (const Release& release) const
{
    return settings().getValue (dismissedKey) == release.version;
}

void UpdateChecker::dismiss (const Release& release)
{
    settings().setValue (dismissedKey, release.version);
    settings().saveIfNeeded();
    sendChangeMessage();
}

void UpdateChecker::openDownload (const Release& release)
{
    // 地址来自网络：只打开本项目在 GitHub 上的地址
    const auto url = release.downloadUrl.startsWith (downloadPrefix) ? release.downloadUrl
                                                                     : juce::String (downloadPrefix) + "releases/latest";
    juce::URL (url).launchInDefaultBrowser();
}

} // namespace curvable
