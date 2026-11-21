#pragma once

#include "engine/Schedule.h"

namespace curvable
{

/** 把音符分配到声部（每个声部对应一个音源实例，各有自己的弯音轮）。

    规则：
    - 同一声部里，时间上重叠的音符必须"相容"：都不弯、音量和声像也都不变；或者起点相同、弯音和包络完全一样
      （平行移动的和弦音）。音量、声像包络作用于整个实例的输出，所以和弯音一样不能共用。
      音源本身是复音的，所以不弯的音符可以全部挤在同一个声部里。
    - 有弯音的音符结束后，再保护 releaseGuard 拍，免得下一个音符改变弯音时影响到它的释放尾音
      （反过来也一样：不弯的音符的尾音也不该被后来的弯音带歪）。
    - 尽量用编号小的声部（贪心，按起点顺序），所以通常只需要很少的实例。
    - 超过 maxVoices 时，放进冲突最少的声部，并计入 overflowNotes（界面上提示）。

    纯 C++，在消息线程上编译 Schedule 时调用。
*/
struct VoiceAllocation
{
    int voiceCount = 1;    ///< 需要的声部数（至少 1）
    int overflowNotes = 0; ///< 因为超出上限而被迫共用声部的音符数
};

/** @param notes  按起点排序；结果写入每个音符的 voice */
VoiceAllocation allocateVoices (std::vector<ScheduledNote>& notes, int maxVoices, double releaseGuard);

/** 两个音符能否共用一个声部（不考虑时间是否重叠）。 */
bool areBendCompatible (const ScheduledNote& a, const ScheduledNote& b) noexcept;

} // namespace curvable
