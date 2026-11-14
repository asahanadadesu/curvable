#pragma once

#include "model/Project.h"

#include <juce_data_structures/juce_data_structures.h>

#include <optional>

namespace curvable
{

/** Project 与 juce::ValueTree 互相转换（保存在宿主工程里）。

    格式带版本号；读取时忽略未知的属性和子节点，缺失的值取默认值，便于以后扩展。
*/
struct ProjectSerializer
{
    static constexpr auto type = "Project";
    static constexpr int currentVersion = 1;

    static juce::ValueTree toValueTree (const Project&);

    /** 类型不对或版本比当前更新（无法安全读取）时返回 std::nullopt。 */
    static std::optional<Project> fromValueTree (const juce::ValueTree&);

    /** 一条曲线的节点与颤音写入 parent 的子节点 / 从 parent 的子节点读出（音符和曲线预设共用）。 */
    static void writeCurve (const PitchCurve&, juce::ValueTree& parent);
    static PitchCurve readCurve (const juce::ValueTree& parent);
};

} // namespace curvable
