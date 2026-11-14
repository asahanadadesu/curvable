#include "hosting/FLDatabaseParser.h"

#include <catch2/catch_test_macros.hpp>

using curvable::parseFLNfo;

TEST_CASE ("parses a typical single-plugin nfo", "[fldb]")
{
    const std::string text =
        "fileversion=2\r\n"
        "ps_name=Serum 2\r\n"
        "ps_presetfilename=%FLPluginDBPath%\\Installed\\Generators\\VST3\\Serum 2.fst\r\n"
        "ps_files=1\r\n"
        "ps_file_name_0=Serum 2\r\n"
        "ps_file_filename_0=C:\\Program Files\\Common Files\\VST3\\Serum2.vst3\\Contents\\x86_64-win\\Serum2.vst3\r\n"
        "ps_file_plugclass_0=8\r\n"
        "ps_file_vendorname_0=Xfer Records\r\n";

    const auto records = parseFLNfo (text);

    REQUIRE (records.size() == 1);
    CHECK (records[0].name == "Serum 2");
    CHECK (records[0].vendor == "Xfer Records");
    CHECK (records[0].filename == "C:\\Program Files\\Common Files\\VST3\\Serum2.vst3\\Contents\\x86_64-win\\Serum2.vst3");
}

TEST_CASE ("parses multiple indexed entries in order", "[fldb]")
{
    const std::string text =
        "ps_file_name_1=Second\n"
        "ps_file_filename_1=B.vst3\n"
        "ps_file_name_0=First\n"
        "ps_file_filename_0=A.vst3\n"
        "ps_file_vendorname_0=Vendor A\n";

    const auto records = parseFLNfo (text);

    REQUIRE (records.size() == 2);
    CHECK (records[0].name == "First");
    CHECK (records[0].vendor == "Vendor A");
    CHECK (records[1].name == "Second");
    CHECK (records[1].vendor.empty());
}

TEST_CASE ("ignores entries without a file and malformed lines", "[fldb]")
{
    const std::string text =
        "\xEF\xBB\xBF"
        "garbage line without equals\n"
        "ps_file_name_0=No file\n"
        "ps_file_name_x=Bad index\n"
        "ps_file_filename_=Empty index\n"
        "  ps_file_filename_3 =  D.vst3  \n";

    const auto records = parseFLNfo (text);

    REQUIRE (records.size() == 1);
    CHECK (records[0].filename == "D.vst3");
}

TEST_CASE ("empty input gives no records", "[fldb]")
{
    CHECK (parseFLNfo ("").empty());
}

TEST_CASE ("reads a single value by key", "[fldb]")
{
    const std::string text =
        "Bitmap=Serum 2.png\r\n"
        "ps_presetfilename=%FLPluginDBPath%\\Installed\\Generators\\VST3\\Serum 2.fst\r\n";

    CHECK (curvable::getFLNfoValue (text, "ps_presetfilename") == "%FLPluginDBPath%\\Installed\\Generators\\VST3\\Serum 2.fst");
    CHECK (curvable::getFLNfoValue (text, "missing").empty());
}
