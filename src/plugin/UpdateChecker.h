#pragma once

#include "hosting/HostingContext.h"

namespace curvable
{

/** 检查 GitHub 上有没有新版本。整个进程共用一个（juce::SharedResourcePointer<UpdateChecker>），在消息线程上使用。

    只读取公开的 Release 信息（api.github.com），不发送任何数据。自动检查最多一天一次，可以在设置里关掉；
    检查到的结果记在设置文件里，下次打开时不用联网也能提示。 */
class UpdateChecker final : public juce::ChangeBroadcaster,
                            private juce::Thread
{
public:
    UpdateChecker();
    ~UpdateChecker() override;

    struct Release
    {
        juce::String version;     ///< 例如 "0.3.0"
        juce::String downloadUrl; ///< Windows：安装程序；其他系统：Release 页面
    };

    enum class State { idle, checking, failed, done };

    bool isAutoCheckEnabled() const;
    void setAutoCheckEnabled (bool);

    /** 自动检查：开着并且离上次检查超过一天时，在后台检查。 */
    void checkIfDue();

    /** 立即检查（设置菜单）。 */
    void checkNow();

    State getState() const noexcept { return state; }

    /** 最近一次检查是用户手动发起的（编辑器据此提示"已是最新"或"检查失败"）。 */
    bool wasManual() const noexcept { return manual; }

    /** 比正在运行的版本新的版本；没有时为空。 */
    std::optional<Release> getNewerRelease() const;

    /** "以后再说"：这个版本不再弹出提示（设置菜单里仍然可以下载）。 */
    bool isDismissed (const Release&) const;
    void dismiss (const Release&);

    /** 在浏览器里打开下载地址（只打开 GitHub 上的地址）。 */
    static void openDownload (const Release&);

    static juce::String currentVersion();

private:
    void run() override;
    void start (bool isManual);
    void finished (std::optional<Release>);

    juce::PropertiesFile& settings() const { return context->getSettings(); }

    juce::SharedResourcePointer<HostingContext> context;
    State state = State::idle;
    bool manual = false;
    juce::WeakReference<UpdateChecker> self;

    JUCE_DECLARE_WEAK_REFERENCEABLE (UpdateChecker)
    JUCE_DECLARE_NON_COPYABLE (UpdateChecker)
};

} // namespace curvable
