#include "i18n/Translations.h"

#include <catch2/catch_test_macros.hpp>

#include <regex>
#include <set>
#include <string>

using namespace curvable;

namespace
{
    /** C++ 字符串字面量的内容（处理 \" \\ \n \xHH；相邻的字面量连在一起）。 */
    std::string unescape (const std::string& raw)
    {
        std::string out;

        for (std::size_t i = 0; i < raw.size(); ++i)
        {
            if (raw[i] != '\\' || i + 1 >= raw.size())
            {
                out += raw[i];
                continue;
            }

            const auto c = raw[++i];

            if (c == 'n')       out += '\n';
            else if (c == 't')  out += '\t';
            else if (c == 'x')
            {
                std::string hex;
                while (i + 1 < raw.size() && std::isxdigit ((unsigned char) raw[i + 1]) && hex.size() < 2)
                    hex += raw[++i];
                out += (char) std::stoi (hex, nullptr, 16);
            }
            else out += c; // \" \\ 等
        }

        return out;
    }

    /** 源代码里所有 tr("...")、trf("...")、TRK("...") 的原文。 */
    std::set<std::string> findTranslatableStrings()
    {
        const juce::File root (CURVABLE_SOURCE_DIR);
        std::set<std::string> found;

        // tr ( "a" "b" ... ) 或 trf ( "a", ...)
        const std::regex call (R"re(\b(?:tr|trf|TRK)\s*\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+))re");
        const std::regex literal (R"re("((?:[^"\\]|\\.)*)")re");

        for (const auto& folder : { "src/ui", "src/plugin" })
        {
            for (const auto& file : root.getChildFile (folder).findChildFiles (juce::File::findFiles, true, "*.cpp;*.h"))
            {
                juce::MemoryBlock bytes;
                REQUIRE (file.loadFileAsData (bytes));
                const std::string text ((const char*) bytes.getData(), bytes.getSize());

                for (auto it = std::sregex_iterator (text.begin(), text.end(), call); it != std::sregex_iterator(); ++it)
                {
                    const auto group = (*it)[1].str();
                    std::string joined;

                    for (auto lit = std::sregex_iterator (group.begin(), group.end(), literal); lit != std::sregex_iterator(); ++lit)
                        joined += unescape ((*lit)[1].str());

                    found.insert (joined);
                }
            }
        }

        return found;
    }

    std::set<std::string> placeholders (const std::string& text)
    {
        std::set<std::string> result;
        const std::regex brace (R"re(\{\d\})re");

        for (auto it = std::sregex_iterator (text.begin(), text.end(), brace); it != std::sregex_iterator(); ++it)
            result.insert (it->str());

        return result;
    }
}

TEST_CASE ("every interface string has a translation in every language", "[i18n]")
{
    const auto strings = findTranslatableStrings();
    REQUIRE (strings.size() > 150); // 扫描确实找到了界面文字

    std::set<std::string> keys;
    for (const auto& e : i18n::getEntries())
        keys.insert (e.text[0]);

    for (const auto& s : strings)
    {
        INFO ("Missing translation: " << s);
        CHECK (keys.count (s) == 1);
    }

    // 表里没有用不到的条目（改了原文却忘了改表）
    for (const auto& k : keys)
    {
        INFO ("Unused translation: " << k);
        CHECK (strings.count (k) == 1);
    }
}

TEST_CASE ("translations are complete and keep their placeholders", "[i18n]")
{
    std::set<std::string> seen;

    for (const auto& e : i18n::getEntries())
    {
        const std::string english = e.text[0];
        INFO (english);
        CHECK (seen.insert (english).second); // 没有重复

        for (int lang = 1; lang < i18n::numLanguages; ++lang)
        {
            const auto* text = e.text[(std::size_t) lang];
            REQUIRE (text != nullptr);
            CHECK (std::string (text).size() > 0);
            CHECK (placeholders (text) == placeholders (english));

            // 本项目的界面文字不用间隔符
            CHECK (std::string (text).find ("\xc2\xb7") == std::string::npos);
            CHECK (juce::CharPointer_UTF8::isValidString (text, (int) std::string (text).size()));
        }
    }
}

TEST_CASE ("tr looks up the current language and falls back to English", "[i18n]")
{
    const auto before = i18n::getLanguage();

    i18n::setLanguage (i18n::Language::chinese);
    CHECK (i18n::tr ("Draw") == juce::String::fromUTF8 ("绘制"));
    CHECK (i18n::tr ("Not in the table") == "Not in the table");
    CHECK (i18n::trf ("Snap  {0}", "1/16") == juce::String::fromUTF8 ("吸附  1/16"));
    CHECK (i18n::trf ("Added {0} notes from {1}", 3, "a.mid") == juce::String::fromUTF8 ("已从 a.mid 添加 3 个音符"));

    i18n::setLanguage (i18n::Language::english);
    CHECK (i18n::trf ("Added {0} notes from {1}", 3, "a.mid") == "Added 3 notes from a.mid");

    CHECK (i18n::languageFromCode ("ja") == i18n::Language::japanese);
    CHECK (i18n::languageFromCode ("xx") == i18n::Language::english);
    CHECK (juce::String (i18n::codeOf (i18n::Language::spanish)) == "es");

    i18n::setLanguage (before);
}
