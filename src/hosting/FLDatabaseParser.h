#pragma once

#include <string>
#include <vector>

namespace curvable
{

/** FL Studio 插件数据库（Plugin database）里 .nfo 文件记录的一个插件。 */
struct FLPluginRecord
{
    std::string name;     ///< ps_file_name_N
    std::string vendor;   ///< ps_file_vendorname_N
    std::string filename; ///< ps_file_filename_N：插件文件路径（VST3 可能指向包内部的文件）
};

/** 解析 .nfo 文件内容（"key=value" 文本，每行一项）。

    一个 .nfo 可能记录多个插件文件（下标 _0、_1、...），全部返回，缺少路径的条目会被忽略。
    纯函数，不访问文件系统，便于单元测试。
*/
std::vector<FLPluginRecord> parseFLNfo (const std::string& text);

/** 读取 .nfo 中某个键的值（例如 "ps_presetfilename"），不存在时返回空字符串。 */
std::string getFLNfoValue (const std::string& text, const std::string& key);

} // namespace curvable
