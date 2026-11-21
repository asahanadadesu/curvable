#pragma once

#include <array>
#include <optional>
#include <string_view>

namespace curvable::version
{

using Number = std::array<int, 3>;

/** "0.2.0"、"v0.2.0"、"0.2.0-beta" → {0, 2, 0}；不是这种格式时为空。 */
inline std::optional<Number> parse (std::string_view text)
{
    if (! text.empty() && (text.front() == 'v' || text.front() == 'V'))
        text.remove_prefix (1);

    Number result {};

    for (std::size_t i = 0; i < result.size(); ++i)
    {
        if (text.empty() || text.front() < '0' || text.front() > '9')
            return std::nullopt;

        int value = 0;

        while (! text.empty() && text.front() >= '0' && text.front() <= '9')
        {
            value = value * 10 + (text.front() - '0');
            text.remove_prefix (1);

            if (value > 99999)
                return std::nullopt;
        }

        result[i] = value;

        if (i + 1 < result.size())
        {
            if (text.empty() || text.front() != '.')
                return std::nullopt;

            text.remove_prefix (1);
        }
    }

    if (! text.empty() && text.front() != '-' && text.front() != '+')
        return std::nullopt;

    return result;
}

/** candidate 比 current 新吗？任一个格式不对时为 false。 */
inline bool isNewer (std::string_view candidate, std::string_view current)
{
    const auto a = parse (candidate), b = parse (current);
    return a.has_value() && b.has_value() && *a > *b;
}

} // namespace curvable::version
