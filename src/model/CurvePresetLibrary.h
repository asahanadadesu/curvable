#pragma once

#include "model/PitchCurve.h"

#include <juce_core/juce_core.h>

#include <vector>

namespace curvable
{

/** 一条保存下来的曲线：节点、弯曲程度、颤音 / trill 设置，以及保存时音符的长度（套用时按比例伸缩）。 */
struct CurvePreset
{
    juce::String name;
    PitchCurve curve;
    double length = 1.0; ///< 保存时音符的长度（拍）
};

/** 曲线预设库：一个 XML 文件，所有工程、所有 Curvable 实例共用。
    不缓存：每次读写都直接操作文件，这样别的实例刚保存的预设也能马上看到。 */
class CurvePresetLibrary
{
public:
    explicit CurvePresetLibrary (juce::File file);

    /** 按名称排序（不区分大小写）。文件不存在或损坏时返回空列表。 */
    std::vector<CurvePreset> load() const;

    /** 保存一个预设；同名的会被替换。名称去掉首尾空白后不能为空。 */
    bool save (const CurvePreset&) const;

    bool remove (const juce::String& name) const;

    bool contains (const juce::String& name) const;

    const juce::File& getFile() const noexcept { return file; }

    static constexpr auto fileName = "CurvePresets.xml";

private:
    bool write (const std::vector<CurvePreset>&) const;

    juce::File file;
};

} // namespace curvable
