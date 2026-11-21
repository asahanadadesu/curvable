#include "model/Version.h"

#include <catch2/catch_test_macros.hpp>

using namespace curvable;

TEST_CASE ("release versions are parsed from tags", "[version]")
{
    CHECK (version::parse ("0.2.0") == version::Number { 0, 2, 0 });
    CHECK (version::parse ("v1.10.3") == version::Number { 1, 10, 3 });
    CHECK (version::parse ("v0.3.0-beta") == version::Number { 0, 3, 0 });

    CHECK_FALSE (version::parse ("").has_value());
    CHECK_FALSE (version::parse ("v").has_value());
    CHECK_FALSE (version::parse ("0.2").has_value());
    CHECK_FALSE (version::parse ("0.2.x").has_value());
    CHECK_FALSE (version::parse ("latest").has_value());
}

TEST_CASE ("only a strictly newer release counts as an update", "[version]")
{
    CHECK (version::isNewer ("v0.3.0", "0.2.0"));
    CHECK (version::isNewer ("0.2.1", "0.2.0"));
    CHECK (version::isNewer ("0.10.0", "0.9.9")); // 按数字比较，不按文字
    CHECK_FALSE (version::isNewer ("v0.2.0", "0.2.0"));
    CHECK_FALSE (version::isNewer ("0.1.9", "0.2.0"));
    CHECK_FALSE (version::isNewer ("nonsense", "0.2.0"));
}
