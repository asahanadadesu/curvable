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

/** 删除节点；剩下的节点全为 0 偏移时清空曲线（等同于不弯音）。 */
void removeCurvePoint (PitchCurve&, int index);

/** 改变 index 到下一个节点之间这一段的弯曲程度（夹在 -1..1）。 */
void setCurveTension (PitchCurve&, int index, float tension);

} // namespace curvable::editing
