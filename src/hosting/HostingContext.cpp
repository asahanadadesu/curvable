#include "hosting/HostingContext.h"
#include "hosting/FLDatabaseParser.h"

#include <map>

namespace curvable
{

namespace
{
    constexpr auto knownPluginsKey = "knownPlugins";
    constexpr auto recentSynthsKey = "recentSynths";
    constexpr int maxRecentSynths  = 8;

    juce::PropertiesFile::Options makeSettingsOptions()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = "Curvable";
        options.filenameSuffix      = ".settings";
        options.folderName          = "asahanada/Curvable";
        options.osxLibrarySubFolder = "Application Support";
        options.storageFormat       = juce::PropertiesFile::storeAsXML;
        // 多个宿主进程可能同时运行 Curvable
        options.processLock         = nullptr;
        return options;
    }
}

HostingContext::HostingContext()
    : settings (std::make_unique<juce::PropertiesFile> (makeSettingsOptions()))
{
    juce::addDefaultFormatsToManager (formatManager);

    if (auto xml = settings->getXmlValue (knownPluginsKey))
        knownPlugins.recreateFromXml (*xml);

    knownPlugins.addChangeListener (this);
}

HostingContext::~HostingContext()
{
    knownPlugins.removeChangeListener (this);
    settings->saveIfNeeded();
}

juce::File HostingContext::getDeadMansPedalFile() const
{
    return settings->getFile().getSiblingFile ("scan-in-progress.txt");
}

juce::Array<juce::PluginDescription> HostingContext::getInstruments() const
{
    juce::Array<juce::PluginDescription> result;

    for (const auto& type : knownPlugins.getTypes())
        if (type.isInstrument && ! isCurvable (type.name, type.manufacturerName))
            result.add (type);

    return result;
}

bool HostingContext::isCurvable (const juce::String& name, const juce::String& vendor)
{
    return name.equalsIgnoreCase ("Curvable") && vendor.equalsIgnoreCase ("asahanada");
}

std::optional<juce::PluginDescription> HostingContext::findKnownMatch (const juce::PluginDescription& desc) const
{
    const auto types = knownPlugins.getTypes();

    // 优先：完全相同的标识（同一文件、同一 ID）
    for (const auto& type : types)
        if (type.createIdentifierString() == desc.createIdentifierString())
            return type;

    // 其次：同一格式、同一插件 ID（插件文件被移动或重装到其他路径）
    for (const auto& type : types)
        if (type.pluginFormatName == desc.pluginFormatName
            && type.uniqueId == desc.uniqueId
            && type.deprecatedUid == desc.deprecatedUid)
            return type;

    return std::nullopt;
}

namespace
{
    /** VST3 / AU 是"包"（文件夹），用户可能拖入包内部的文件。向上找到最外层的包。 */
    juce::File findOutermostBundle (const juce::File& file)
    {
        auto result = file;

        for (auto parent = file.getParentDirectory(); parent != parent.getParentDirectory();
             parent = parent.getParentDirectory())
        {
            if (parent.hasFileExtension (".vst3;.component"))
                result = parent;
        }

        return result;
    }
}

bool HostingContext::isPotentialPluginFile (const juce::File& file) const
{
    return findOutermostBundle (file).hasFileExtension (".vst3;.component");
}

juce::Array<juce::PluginDescription> HostingContext::addPluginFile (const juce::File& file)
{
    const auto path = findOutermostBundle (file).getFullPathName();
    juce::Array<juce::PluginDescription> instruments;

    for (auto* format : formatManager.getFormats())
    {
        if (! format->fileMightContainThisPluginType (path))
            continue;

        juce::OwnedArray<juce::PluginDescription> found;
        knownPlugins.scanAndAddFile (path, true, found, *format);

        for (auto* desc : found)
            if (desc->isInstrument && ! isCurvable (desc->name, desc->manufacturerName))
                instruments.add (*desc);
    }

    return instruments;
}

bool HostingContext::isBlocked (const juce::File& file) const
{
    return knownPlugins.getBlacklistedFiles().contains (findOutermostBundle (file).getFullPathName());
}

//==============================================================================
namespace
{
    juce::String keyForFile (const juce::File& file)
    {
       #if JUCE_WINDOWS
        return file.getFullPathName().toLowerCase(); // Windows 路径不区分大小写
       #else
        return file.getFullPathName();
       #endif
    }

    juce::String keyForDescription (const juce::PluginDescription& desc)
    {
        const juce::File file (juce::File::isAbsolutePath (desc.fileOrIdentifier) ? desc.fileOrIdentifier : juce::String());

        if (file != juce::File())
            return keyForFile (findOutermostBundle (file));

        return desc.createIdentifierString(); // AU 等用标识符而不是文件路径
    }

    /** 读取 FL Studio 插件数据库中的所有 Generator，只保留 Curvable 能托管的格式。 */
    juce::Array<SynthEntry> readFLGenerators (const juce::File& databaseFolder)
    {
        juce::Array<SynthEntry> result;
        const auto generators = databaseFolder.getChildFile ("Installed").getChildFile ("Generators");

        if (! generators.isDirectory())
            return result;

        for (const auto& nfo : generators.findChildFiles (juce::File::findFiles, true, "*.nfo"))
        {
            for (const auto& record : parseFLNfo (nfo.loadFileAsString().toStdString()))
            {
                const auto path = juce::String::fromUTF8 (record.filename.c_str());

                if (! juce::File::isAbsolutePath (path))
                    continue;

                const auto bundle = findOutermostBundle (juce::File (path));

                // 只支持 VST3（以及 macOS 上的 AU）；VST2 无法托管
                if (! bundle.hasFileExtension (".vst3;.component") || ! bundle.exists())
                    continue;

                SynthEntry entry;
                entry.name = juce::String::fromUTF8 (record.name.c_str());
                entry.vendor = juce::String::fromUTF8 (record.vendor.c_str());
                entry.file = bundle;
                entry.fromFLStudio = true;

                if (entry.name.isEmpty())
                    entry.name = bundle.getFileNameWithoutExtension();

                if (! HostingContext::isCurvable (entry.name, entry.vendor))
                    result.add (entry);
            }
        }

        return result;
    }
}

juce::String SynthEntry::getKey() const
{
    if (description.has_value())
        return keyForDescription (*description);

    return keyForFile (file);
}

juce::File HostingContext::getFLPluginDatabaseFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("Image-Line").getChildFile ("FL Studio").getChildFile ("Presets").getChildFile ("Plugin database");
}

juce::File HostingContext::resolveDroppedFile (const juce::File& file) const
{
    if (! file.hasFileExtension (".fst"))
        return file;

    // FL 浏览器里的 .fst 旁边有同名 .nfo。"已安装"目录里的 .nfo 直接记录了插件路径；
    // 收藏目录里的 .nfo 则通过 ps_presetfilename 指向"已安装"目录里的条目，最多跟随一次。
    auto current = file;

    for (int hop = 0; hop < 2; ++hop)
    {
        const auto text = current.withFileExtension (".nfo").loadFileAsString().toStdString();

        for (const auto& record : parseFLNfo (text))
        {
            const auto path = juce::String::fromUTF8 (record.filename.c_str());

            if (juce::File::isAbsolutePath (path))
                if (const auto bundle = findOutermostBundle (juce::File (path)); bundle.exists())
                    return bundle;
        }

        const auto preset = juce::String::fromUTF8 (getFLNfoValue (text, "ps_presetfilename").c_str())
                                .replace ("%FLPluginDBPath%", getFLPluginDatabaseFolder().getFullPathName(), true)
                                .replaceCharacter ('\\', juce::File::getSeparatorChar());

        if (! juce::File::isAbsolutePath (preset) || juce::File (preset) == current)
            break;

        current = juce::File (preset);
    }

    return {};
}

juce::Array<SynthEntry> HostingContext::getSynthCatalog() const
{
    std::map<juce::String, SynthEntry> byKey;

    // FL 的记录先放进去，Curvable 已读取过的描述覆盖它（可直接加载，信息也更准确）
    for (auto& entry : readFLGenerators (getFLPluginDatabaseFolder()))
        byKey[entry.getKey()] = entry;

    for (const auto& desc : getInstruments())
    {
        SynthEntry entry;
        entry.name = desc.name;
        entry.vendor = desc.manufacturerName;
        entry.description = desc;

        if (juce::File::isAbsolutePath (desc.fileOrIdentifier))
            entry.file = findOutermostBundle (juce::File (desc.fileOrIdentifier));

        const auto key = entry.getKey();
        entry.fromFLStudio = byKey.count (key) > 0 && byKey[key].fromFLStudio;
        byKey[key] = entry;
    }

    juce::Array<SynthEntry> recent, others;
    const auto recentKeys = juce::StringArray::fromLines (settings->getValue (recentSynthsKey));

    for (auto& [key, entry] : byKey)
    {
        if (recentKeys.contains (key))
        {
            entry.recent = true;
            recent.add (entry);
        }
        else
        {
            others.add (entry);
        }
    }

    std::sort (recent.begin(), recent.end(), [&] (const SynthEntry& a, const SynthEntry& b)
    {
        return recentKeys.indexOf (a.getKey()) < recentKeys.indexOf (b.getKey());
    });

    std::sort (others.begin(), others.end(), [] (const SynthEntry& a, const SynthEntry& b)
    {
        return a.name.compareNatural (b.name) < 0;
    });

    recent.addArray (others);
    return recent;
}

void HostingContext::markRecentlyUsed (const juce::PluginDescription& desc)
{
    auto keys = juce::StringArray::fromLines (settings->getValue (recentSynthsKey));
    const auto key = keyForDescription (desc);

    keys.removeString (key);
    keys.removeEmptyStrings();
    keys.insert (0, key);

    while (keys.size() > maxRecentSynths)
        keys.remove (keys.size() - 1);

    settings->setValue (recentSynthsKey, keys.joinIntoString ("\n"));
    settings->saveIfNeeded();
}

void HostingContext::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (auto xml = knownPlugins.createXml())
    {
        settings->setValue (knownPluginsKey, xml.get());
        settings->saveIfNeeded();
    }
}

} // namespace curvable
