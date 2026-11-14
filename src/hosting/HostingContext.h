#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace curvable
{

/** 音源浏览器里的一项。 */
struct SynthEntry
{
    juce::String name;
    juce::String vendor;
    juce::File file;                                     ///< 插件文件（包）；AU 等非文件插件为空
    std::optional<juce::PluginDescription> description;  ///< 已读取过该插件时才有，此时可直接加载
    bool fromFLStudio = false;                           ///< 来自 FL Studio 的插件数据库
    bool recent = false;

    /** 去重和记录"最近使用"用的键。 */
    juce::String getKey() const;
};

/** 同一进程内所有 Curvable 实例共享的托管环境：插件格式、已扫描的插件列表、设置文件。

    通过 juce::SharedResourcePointer<HostingContext> 获取，需在消息线程上创建。
    插件列表保存在用户设置目录中（Windows：%APPDATA%\asahanada\Curvable）。
*/
class HostingContext final : private juce::ChangeListener
{
public:
    HostingContext();
    ~HostingContext() override;

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    juce::PropertiesFile& getSettings() noexcept { return *settings; }

    /** 扫描时记录"正在扫描哪个插件"的文件；扫描导致崩溃后，下次会自动跳过该插件。 */
    juce::File getDeadMansPedalFile() const;

    /** 已扫描插件中所有的乐器（音源）。 */
    juce::Array<juce::PluginDescription> getInstruments() const;

    /** 在已扫描列表中查找与 desc 对应的插件（插件被移动到其他路径后仍能找到）。 */
    std::optional<juce::PluginDescription> findKnownMatch (const juce::PluginDescription& desc) const;

    /** 这个文件或文件夹看起来像插件吗？（只看扩展名，不加载，可用于拖拽时的快速判断） */
    bool isPotentialPluginFile (const juce::File&) const;

    /** 读取单个插件文件，把其中的插件加入已扫描列表，并返回其中的乐器。
        用于拖拽或手动选择文件，无需完整扫描。会加载该插件，可能耗时一两秒。
        若拖入的是 VST3 包内部的文件，会自动定位到包本身。 */
    juce::Array<juce::PluginDescription> addPluginFile (const juce::File&);

    /** 该插件文件是否因之前扫描失败而被屏蔽。 */
    bool isBlocked (const juce::File&) const;

    /** 把拖入的文件换算成插件文件：FL Studio 浏览器里的 .fst 会通过插件数据库找到对应的插件；
        其他文件原样返回。无法换算时返回空 File。 */
    juce::File resolveDroppedFile (const juce::File&) const;

    /** 音源浏览器的完整列表，无需扫描、不加载任何插件：
        最近使用的排在最前，其余按名称排序。
        来源：Curvable 已读取过的乐器 + FL Studio 插件数据库中的 Generators。 */
    juce::Array<SynthEntry> getSynthCatalog() const;

    void markRecentlyUsed (const juce::PluginDescription&);

    /** FL Studio 插件数据库的默认位置（Documents/Image-Line/FL Studio/Presets/Plugin database）。 */
    static juce::File getFLPluginDatabaseFolder();

    /** 是否是 Curvable 自己（不允许托管自己）。 */
    static bool isCurvable (const juce::String& name, const juce::String& vendor);

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    std::unique_ptr<juce::PropertiesFile> settings;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostingContext)
};

} // namespace curvable
