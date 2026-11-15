#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <vector>

/** 标记一个需要翻译、但要稍后才翻译的字符串（例如放在表里，显示时再 tr）。
    单元测试会扫描源代码里的 tr("...")、trf("...") 和 TRK("...")，检查每一句都有全部语言的翻译。 */
#define TRK(text) text

namespace curvable::i18n
{

enum class Language { english, chinese, japanese, korean, spanish };
inline constexpr int numLanguages = 5;

struct LanguageInfo
{
    Language language;
    const char* code;       ///< 保存在设置里的代号
    const char* nativeName; ///< 菜单里显示的名字（用这种语言自己写）
};

const std::array<LanguageInfo, numLanguages>& getLanguages();
Language languageFromCode (const juce::String&); ///< 不认识的代号当作英语
const char* codeOf (Language);

/** 当前界面语言（全局：同一个进程里的所有 Curvable 窗口共用）。默认英语。 */
void setLanguage (Language);
Language getLanguage() noexcept;

/** 翻译。english 是英文原文（也是查表用的键）；没有翻译时返回原文。 */
juce::String tr (const char* english);
juce::String tr (const char* english, Language);

/** 带参数的翻译：原文里的 {0}、{1}……依次换成参数。 */
template <typename... Args>
juce::String trf (const char* english, const Args&... args)
{
    auto text = tr (english);
    int index = 0;
    ((text = text.replace ("{" + juce::String (index++) + "}", juce::String (args))), ...);
    return text;
}

/** 翻译表的一行：英文原文和各语言的译文（顺序与 Language 相同）。 */
struct Entry
{
    std::array<const char*, numLanguages> text;
};

const std::vector<Entry>& getEntries();
bool hasEntry (const juce::String& english);

/** 语言改变时通知（消息线程）。 */
struct Listener
{
    virtual ~Listener() = default;
    virtual void languageChanged() = 0;
};

void addListener (Listener*);
void removeListener (Listener*);

} // namespace curvable::i18n
