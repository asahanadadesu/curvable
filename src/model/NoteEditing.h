#pragma once

#include "model/Project.h"

#include <set>
#include <vector>

/** 卷帘编辑操作。纯函数，不依赖界面，便于单元测试。

    拖动类操作（移动、伸缩、改力度）总是基于拖动开始时的原始工程计算，
    这样拖动过程中反复调用也不会累积误差，越界时也能按整组夹住。
*/
namespace curvable::editing
{

using IdSet = std::set<NoteId>;

constexpr double minNoteLength = 1.0 / 64.0;

/** 吸附到最近的网格线。grid <= 0 表示不吸附。 */
double snapNearest (double beat, double grid) noexcept;

/** 吸附到不晚于 beat 的网格线（带容差：正好落在网格线上的不会退到前一格）。 */
double snapFloor (double beat, double grid) noexcept;

/** 把 ids 中的音符整体平移；平移量会被夹住，使所有音符都保持在 0 拍之后、音高 0..127 之内。 */
void moveNotes (Project& target, const Project& original, const IdSet& ids, double deltaBeats, int deltaPitch);

/** 改变 ids 中音符的时长（每个音符加上 deltaLength，最短为 minLength）。 */
void resizeNotes (Project& target, const Project& original, const IdSet& ids, double deltaLength, double minLength);

/** 改变 ids 中音符的力度（加上 delta，夹在 0.01..1）。 */
void changeVelocities (Project& target, const Project& original, const IdSet& ids, float delta);

void setVelocity (Project&, const IdSet& ids, float velocity);

/** 移调（整组夹住，不会有音符越界）。 */
void transposeNotes (Project&, const IdSet& ids, int semitones);

/** 按拍平移（整组夹住）。 */
void shiftNotes (Project&, const IdSet& ids, double deltaBeats);

void deleteNotes (Project&, const IdSet& ids);

/** 复制：返回音符副本，起点相对于其中最早的音符（最早的音符起点为 0）。 */
std::vector<Note> copyNotes (const Project&, const IdSet& ids);

/** 在 atBeat 处粘贴，返回新音符的 id。 */
IdSet pasteNotes (Project&, const std::vector<Note>& clip, double atBeat);

/** 把选中的音符复制一份放在它们之后（偏移量为选区长度，按网格向上取整），返回新音符的 id。 */
IdSet duplicateNotes (Project&, const IdSet& ids, double grid);

//==============================================================================
// 连奏（把一串音符合并成一个音符，用滑音连起来）

struct GlideResult
{
    enum class Error { none, tooFewNotes, sameStart };

    NoteId id = 0;           ///< 合并后的音符（沿用第一个音符的 id）
    Error error = Error::none;
};

/** 默认的滑音时长（拍）。 */
constexpr double defaultGlideBeats = 0.25;

/** 把 ids 中的音符按起点顺序合并成一个音符：
    - 起点 = 第一个音符的起点，终点 = 最晚的终点；音高、力度沿用第一个音符；
    - 后面每个音符变成曲线上的偏移（它的音高 − 第一个音符的音高）；
      两个音符之间有空隙时，斜坡从前一个音符的终点开始、在后一个音符的起点到达；
      首尾相接或重叠时，斜坡在后一个音符的起点前 min (glideBeats, 两个起点间距的一半) 开始；
    - 每个音符原有的曲线（节点、弯曲程度、颤音段）平移到它在合并后音符里的位置；
      整个音符的颤音变成它那一段的颤音段；颤音参数取第一个有颤音的音符的。
    至少两个音符；有两个音符起点相同（和弦）时不合并。 */
GlideResult glideNotes (Project&, const IdSet& ids, double glideBeats = defaultGlideBeats);

/** 和弦之间的滑音（也包括单条旋律）。选中的音符按起点分成若干组（和弦），每一组的每个声部
    滑到下一组里移动距离最小的音（总移动量最小；相同时更均匀的优先），各自合并成一个连奏音符（glideNotes）。
    - 下一组的音更少：多出来的声部滑到最近的音上（同音重叠）；
    - 下一组的音更多：多出来的音单独开始一个新声部。
    至少两组；每组最多 12 个音。 */
struct ChordGlideResult
{
    enum class Error { none, tooFewNotes, oneChordOnly, chordTooLarge };

    IdSet ids;                 ///< 合并后的音符（以及没有被合并的音符）
    Error error = Error::none;
};

ChordGlideResult glideChords (Project&, const IdSet& ids, double glideBeats = defaultGlideBeats);

/** 声部配对（纯函数，便于测试）：from 中每个音高配一个 to 的下标。
    from 不多于 to 时一对一；多于 to 时 to 的每个音至少配到一次。 */
std::vector<int> matchVoices (const std::vector<double>& from, const std::vector<double>& to);

//==============================================================================
// 切开

/** 在 cut（相对音符起点，拍）处把曲线分成两段，切开前后在每个时刻的值都不变（包括弯曲程度）。
    第二段的时间从 0 开始。 */
std::pair<PitchCurve, PitchCurve> splitCurve (const PitchCurve&, double cut);

/** 在 atBeat 处切开 ids 中跨过这个位置的音符（离两端都至少 minNoteLength）。
    后半段的音高改为切点处最接近的半音，曲线相应平移，所以切开后音高完全不变。返回后半段的 id。 */
IdSet sliceNotes (Project&, const IdSet& ids, double atBeat);

/** 与 [beatStart, beatEnd) × [pitchLow, pitchHigh] 有重叠的音符。 */
IdSet notesInRange (const Project&, double beatStart, double beatEnd, int pitchLow, int pitchHigh);

IdSet allNotes (const Project&);

/** 去掉 ids 中已不存在的音符（例如撤销之后）。 */
IdSet existingOnly (const Project&, const IdSet& ids);

const Note* findNote (const Project&, NoteId);
Note* findNote (Project&, NoteId);

//==============================================================================
// 音高曲线

/** 在 time 处插入节点，返回它的下标。曲线原本为空时，先在音符起点补一个 0 偏移的节点
    （这样第一次点击就是"从原音高滑到这里"）；time 在起点上时则不补。 */
int insertCurvePoint (PitchCurve&, double time, double semitones);

/** 移动节点：time 夹在相邻节点之间以及 [0, length] 之内（节点顺序不会改变）。 */
void moveCurvePoint (PitchCurve&, int index, double time, double semitones, double length);

/** 删除节点。其余节点保留（即使都是 0 偏移）。 */
void removeCurvePoint (PitchCurve&, int index);

/** 改变 index 到下一个节点之间这一段的弯曲程度（夹在 -1..1）。 */
void setCurveTension (PitchCurve&, int index, float tension);

/** 常用的音高手法。音头类（scoop、overshoot）只替换音头一段，音尾类（fall、doit）只替换音尾一段，
    所以可以组合（例如先 scoop 再 fall）；bendRelease 替换整条主曲线；clear 去掉主曲线和颤音。
    幅度不超过 bendRange。颤音不受影响（clear 除外）。 */
enum class Shape { scoop, overshoot, fall, doit, bendRelease, clear };

void applyShape (PitchCurve&, Shape, double noteLength, double bendRange);

//==============================================================================
// 颤音

/** 给 segment（起点节点的下标）这一段加上或去掉颤音。整个音符的颤音会先换成按段的。 */
void setSegmentVibrato (PitchCurve&, int segment, bool on);

/** 整个音符加上或去掉颤音。 */
void setWholeNoteVibrato (PitchCurve&, bool on);

/** 去掉所有颤音（参数保留，再次加上时沿用）。 */
void removeVibrato (PitchCurve&);

/** 切换颤音速度是否跟随曲速，尽量保持听起来的速度不变。 */
void setVibratoSync (Vibrato&, bool sync, double secondsPerBeat);

/** 跟随曲速时可选的速度（每拍周期数）和名字。 */
inline constexpr double vibratoSyncRates[] = { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0 };
inline constexpr const char* vibratoSyncNames[] = { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

/** 最接近 cyclesPerBeat 的可选速度的下标。 */
int nearestVibratoSyncIndex (double cyclesPerBeat) noexcept;

} // namespace curvable::editing
