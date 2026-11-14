#include "model/CurvePresetLibrary.h"
#include "model/ProjectSerializer.h"

#include <algorithm>
#include <cmath>

namespace curvable
{

namespace
{
    const juce::Identifier libraryType { "CurvePresets" };
    const juce::Identifier presetType  { "Preset" };
    const juce::Identifier nameId      { "name" };
    const juce::Identifier lengthId    { "length" };
    const juce::Identifier versionId   { "version" };
    constexpr int currentVersion = 1;
}

CurvePresetLibrary::CurvePresetLibrary (juce::File f) : file (std::move (f)) {}

std::vector<CurvePreset> CurvePresetLibrary::load() const
{
    std::vector<CurvePreset> presets;

    if (! file.existsAsFile())
        return presets;

    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return presets;

    const auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.hasType (libraryType) || (int) tree.getProperty (versionId, 1) > currentVersion)
        return presets;

    for (const auto& child : tree)
    {
        if (! child.hasType (presetType))
            continue;

        CurvePreset preset;
        preset.name = child.getProperty (nameId).toString().trim();
        preset.length = child.getProperty (lengthId, 1.0);
        preset.curve = ProjectSerializer::readCurve (child);

        if (preset.name.isEmpty() || ! std::isfinite (preset.length) || preset.length <= 0.0)
            continue;

        // 同名的只保留后一个
        std::erase_if (presets, [&] (const CurvePreset& p) { return p.name == preset.name; });
        presets.push_back (std::move (preset));
    }

    std::sort (presets.begin(), presets.end(), [] (const CurvePreset& a, const CurvePreset& b)
    {
        return a.name.compareNatural (b.name) < 0;
    });

    return presets;
}

bool CurvePresetLibrary::save (const CurvePreset& preset) const
{
    auto copy = preset;
    copy.name = copy.name.trim();

    if (copy.name.isEmpty() || ! std::isfinite (copy.length) || copy.length <= 0.0)
        return false;

    copy.curve.normalise();

    auto presets = load();
    std::erase_if (presets, [&] (const CurvePreset& p) { return p.name == copy.name; });
    presets.push_back (std::move (copy));
    return write (presets);
}

bool CurvePresetLibrary::remove (const juce::String& name) const
{
    auto presets = load();
    const auto before = presets.size();
    std::erase_if (presets, [&] (const CurvePreset& p) { return p.name == name; });
    return presets.size() != before && write (presets);
}

bool CurvePresetLibrary::contains (const juce::String& name) const
{
    const auto presets = load();
    return std::any_of (presets.begin(), presets.end(), [&] (const CurvePreset& p) { return p.name == name.trim(); });
}

bool CurvePresetLibrary::write (const std::vector<CurvePreset>& presets) const
{
    juce::ValueTree tree (libraryType);
    tree.setProperty (versionId, currentVersion, nullptr);

    for (const auto& preset : presets)
    {
        juce::ValueTree child (presetType);
        child.setProperty (nameId, preset.name, nullptr);
        child.setProperty (lengthId, preset.length, nullptr);
        ProjectSerializer::writeCurve (preset.curve, child);
        tree.appendChild (child, nullptr);
    }

    const auto xml = tree.createXml();

    if (xml == nullptr || ! file.getParentDirectory().createDirectory())
        return false;

    // 先写临时文件再替换，避免写到一半时被别的实例读到
    juce::TemporaryFile temp (file);

    if (! xml->writeTo (temp.getFile()))
        return false;

    return temp.overwriteTargetFileWithTemporary();
}

} // namespace curvable
