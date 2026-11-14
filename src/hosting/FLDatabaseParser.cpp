#include "hosting/FLDatabaseParser.h"

#include <map>
#include <string_view>

namespace curvable
{

namespace
{
    std::string_view trim (std::string_view s)
    {
        while (! s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n'))
            s.remove_prefix (1);
        while (! s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
            s.remove_suffix (1);
        return s;
    }

    /** "ps_file_filename_12" → ("ps_file_filename", 12)；没有数字后缀时返回 -1。 */
    std::pair<std::string_view, int> splitIndex (std::string_view key)
    {
        const auto underscore = key.rfind ('_');

        if (underscore == std::string_view::npos || underscore + 1 >= key.size())
            return { key, -1 };

        int index = 0;

        for (auto c : key.substr (underscore + 1))
        {
            if (c < '0' || c > '9')
                return { key, -1 };

            index = index * 10 + (c - '0');

            if (index > 100000)
                return { key, -1 };
        }

        return { key.substr (0, underscore), index };
    }

    /** 逐行回调 (key, value)，key 与 value 已去除首尾空白。 */
    template <typename Callback>
    void forEachEntry (const std::string& text, Callback&& callback)
    {
        std::string_view rest (text);

        // 去掉 UTF-8 BOM
        if (rest.size() >= 3 && rest.substr (0, 3) == "\xEF\xBB\xBF")
            rest.remove_prefix (3);

        while (! rest.empty())
        {
            const auto newline = rest.find ('\n');
            const auto line = trim (rest.substr (0, newline));
            rest = newline == std::string_view::npos ? std::string_view() : rest.substr (newline + 1);

            if (const auto equals = line.find ('='); equals != std::string_view::npos)
                callback (trim (line.substr (0, equals)), trim (line.substr (equals + 1)));
        }
    }
}

std::string getFLNfoValue (const std::string& text, const std::string& key)
{
    std::string result;

    forEachEntry (text, [&] (std::string_view k, std::string_view v)
    {
        if (result.empty() && k == key)
            result = std::string (v);
    });

    return result;
}

std::vector<FLPluginRecord> parseFLNfo (const std::string& text)
{
    std::map<int, FLPluginRecord> byIndex;

    forEachEntry (text, [&] (std::string_view key, std::string_view v)
    {
        const auto [field, index] = splitIndex (key);

        if (index < 0)
            return;

        const auto value = std::string (v);

        if (field == "ps_file_name")            byIndex[index].name = value;
        else if (field == "ps_file_vendorname") byIndex[index].vendor = value;
        else if (field == "ps_file_filename")   byIndex[index].filename = value;
    });

    std::vector<FLPluginRecord> result;

    for (auto& [index, record] : byIndex)
        if (! record.filename.empty())
            result.push_back (std::move (record));

    return result;
}

} // namespace curvable
