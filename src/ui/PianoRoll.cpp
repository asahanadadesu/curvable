#include "ui/PianoRoll.h"
#include "plugin/PluginProcessor.h"
#include "ui/CurvableLookAndFeel.h"

namespace curvable
{

using i18n::tr;
using i18n::trf;

namespace
{
    constexpr int bottomBarHeight = 48;
    constexpr int bottomBarGap    = 14;
    constexpr int panelPadding    = 10;
    constexpr int keysWidth       = 60;
    constexpr int keysGap         = 10;
    constexpr int rulerHeight     = 28;
    constexpr int velocityHeight  = 72;
    constexpr int velocityGap     = 8;
    constexpr int scrollBarSize   = 10;
    constexpr int scrollBarGap    = 6;

    constexpr float defaultVelocity = 0.8f;

    bool isBlackKey (int pitch) noexcept
    {
        const auto pc = pitch % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    /** FL Studio 的记法：60 = C5。 */
    juce::String noteName (int pitch)
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String (names[pitch % 12]) + juce::String (pitch / 12);
    }

    struct GridOption { const char* name; double beats; };

    const GridOption gridOptions[] = {
        { TRK ("Bar"), 4.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
        { TRK ("1/8 triplet"), 1.0 / 3.0 }, { TRK ("1/16 triplet"), 1.0 / 6.0 }, { TRK ("None"), 0.0 },
    };
}

//==============================================================================
// 琴键：凸起的独立面板，模仿真实钢琴；按下试听
struct PianoRoll::Keyboard final : public juce::Component
{
    explicit Keyboard (PianoRoll& o) : owner (o) {}

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        const auto& v = owner.view;
        const auto bounds = getLocalBounds().toFloat();

        juce::Path clip;
        clip.addRoundedRectangle (bounds, 12.0f);
        g.reduceClipRegion (clip);
        g.fillAll (p.keyWhite);

        const auto rh = (float) v.rowHeight;
        const auto w = bounds.getWidth();

        for (int pitch = juce::jmin (127, v.yToPitch (0)); pitch >= juce::jmax (0, v.yToPitch (getHeight())); --pitch)
        {
            const auto y = (float) v.pitchToY (pitch);

            if (pitch == pressedPitch)
            {
                g.setColour (p.accent.withAlpha (0.28f));
                g.fillRect (0.0f, y, w, rh);
            }

            if (isBlackKey (pitch))
            {
                g.setColour (pitch == pressedPitch ? p.accent : p.keyBlack);
                g.fillRoundedRectangle (-4.0f, y + 1.0f, w * 0.62f + 4.0f, rh - 2.0f, 4.0f);
            }

            // 白键之间只有 B/C、E/F 是直接相邻的，其余被黑键隔开
            if (pitch % 12 == 0 || pitch % 12 == 5)
            {
                g.setColour (p.keyLine);
                g.fillRect (0.0f, y + rh - 1.0f, w, 1.0f);
            }

            if (pitch % 12 == 0 && rh >= 9.0f)
            {
                g.setColour (p.muted);
                g.setFont (juce::FontOptions (juce::jmin (11.0f, rh - 1.0f), juce::Font::bold));
                g.drawText (noteName (pitch), juce::Rectangle<float> (0.0f, y, w - 7.0f, rh), juce::Justification::centredRight);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override  { press (owner.view.yToPitch (e.position.y)); }
    void mouseEnter (const juce::MouseEvent&) override   { owner.setHoverHint (tr ("Click a key  Hear the pitch")); }
    void mouseExit (const juce::MouseEvent&) override    { owner.setHoverHint ({}); }
    void mouseDrag (const juce::MouseEvent& e) override  { press (owner.view.yToPitch (e.position.y)); }
    void mouseUp (const juce::MouseEvent&) override
    {
        owner.stopPreview();
        pressedPitch = -1;
        repaint();
    }

    void press (int pitch)
    {
        pitch = juce::jlimit (0, 127, pitch);

        if (pitch == pressedPitch)
            return;

        pressedPitch = pitch;
        owner.preview (pitch, defaultVelocity);
        repaint();
    }

    PianoRoll& owner;
    int pressedPitch = -1;
};

//==============================================================================
// 小节标尺
struct PianoRoll::Ruler final : public juce::Component
{
    explicit Ruler (PianoRoll& o) : owner (o) {}

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        const auto& v = owner.view;
        const auto w = (float) getWidth(), h = (float) getHeight();

        // 小节号间隔：保证数字之间至少约 36 像素
        int barStep = 1;
        while (barStep * 4.0 * v.pixelsPerBeat < 36.0 && barStep < 256)
            barStep *= 2;

        const auto firstBeat = std::floor (v.startBeat);
        const auto lastBeat = v.xToBeat (w);

        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

        for (double beat = firstBeat; beat <= lastBeat; beat += 1.0)
        {
            const auto x = (float) std::round (v.beatToX (beat));
            const auto bar = (int) std::floor (beat / 4.0 + 1.0e-9);
            const bool isBar = std::abs (beat - bar * 4.0) < 1.0e-9;

            if (isBar && bar % barStep == 0)
            {
                g.setColour (p.muted);
                g.drawText (juce::String (bar + 1), juce::Rectangle<float> (x + 4.0f, 4.0f, 40.0f, 16.0f),
                            juce::Justification::centredLeft, false);
                g.setColour (p.lineStrong);
                g.fillRect (x, h - 9.0f, 1.0f, 9.0f);
            }
            else if (v.pixelsPerBeat >= 12.0)
            {
                g.setColour (p.lineStrong);
                g.fillRect (x, h - 5.0f, 1.0f, 5.0f);
            }
        }

        // 循环区：标尺上方的一条（关着时变淡）
        if (const auto loop = loopRange(); ! loop.isEmpty())
        {
            const auto on = owner.project.loopOn || dragKind == DragKind::newLoop;
            const auto x0 = (float) v.beatToX (loop.getStart()), x1 = (float) v.beatToX (loop.getEnd());
            const auto band = juce::Rectangle<float> (x0, 1.0f, juce::jmax (2.0f, x1 - x0), 6.0f);
            g.setColour (p.warm.withAlpha (on ? 0.9f : 0.3f));
            g.fillRoundedRectangle (band, 3.0f);
            g.fillRect (x0, 1.0f, 2.0f, h - 1.0f);
            g.fillRect (x1 - 2.0f, 1.0f, 2.0f, h - 1.0f);
        }

        // 起点：空心三角（不在播放时）
        if (owner.playheadBeat < 0.0)
        {
            const auto x = (float) v.beatToX (owner.startBeat);
            juce::Path marker;
            marker.addTriangle (x - 5.0f, h - 9.0f, x + 5.0f, h - 9.0f, x, h - 2.0f);
            g.setColour (p.accent.withAlpha (0.8f));
            g.strokePath (marker, juce::PathStrokeType (1.4f));
        }

        if (owner.playheadBeat >= 0.0)
        {
            const auto x = (float) v.beatToX (owner.playheadBeat);
            juce::Path marker;
            marker.addTriangle (x - 5.0f, h - 9.0f, x + 5.0f, h - 9.0f, x, h - 2.0f);
            g.setColour (p.accent);
            g.fillPath (marker);
        }
    }

    /** 显示的循环区：正在拖出新的循环区时是拖出的范围。 */
    juce::Range<double> loopRange() const
    {
        if (dragKind == DragKind::newLoop || dragKind == DragKind::loopStart || dragKind == DragKind::loopEnd)
            return dragLoop;

        return owner.project.hasLoop() ? juce::Range<double> (owner.project.loopStart, owner.project.loopEnd) : juce::Range<double>();
    }

    double snapped (float x, bool noSnap) const
    {
        const auto beat = juce::jmax (0.0, owner.view.xToBeat (x));
        return noSnap ? beat : editing::snapNearest (beat, owner.grid > 0.0 ? owner.grid : 1.0);
    }

    /** 单击：设置起点（播放中从那里接着播）；拖动：拖出循环区；拖循环区两端：改变它；双击：去掉循环区。 */
    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();
        dragKind = DragKind::none;

        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

        if (owner.project.hasLoop())
        {
            const auto& v = owner.view;

            if (std::abs (e.position.x - (float) v.beatToX (owner.project.loopStart)) <= 5.0f)
                dragKind = DragKind::loopStart;
            else if (std::abs (e.position.x - (float) v.beatToX (owner.project.loopEnd)) <= 5.0f)
                dragKind = DragKind::loopEnd;

            if (dragKind != DragKind::none)
                dragLoop = { owner.project.loopStart, owner.project.loopEnd };
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto noSnap = e.mods.isAltDown();
        const auto minimum = owner.grid > 0.0 ? owner.grid : 0.25;

        if (dragKind == DragKind::none && e.getDistanceFromDragStart() > 4 && ! e.mods.isPopupMenu())
            dragKind = DragKind::newLoop;

        const auto beat = snapped (e.position.x, noSnap);

        switch (dragKind)
        {
            case DragKind::newLoop:
            {
                const auto a = snapped ((float) e.getMouseDownX(), noSnap);
                dragLoop = { juce::jmin (a, beat), juce::jmax (a, beat) };
                break;
            }
            case DragKind::loopStart: dragLoop.setStart (juce::jmin (beat, dragLoop.getEnd() - minimum)); break;
            case DragKind::loopEnd:   dragLoop.setEnd (juce::jmax (beat, dragLoop.getStart() + minimum)); break;
            case DragKind::none:      break;
        }

        repaint();
        owner.repaintContent();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto kind = std::exchange (dragKind, DragKind::none);

        if (e.mods.isPopupMenu())
            return;

        if (kind == DragKind::none)
        {
            if (e.getNumberOfClicks() < 2)
                owner.setStartBeat (snapped (e.position.x, e.mods.isAltDown()));
        }
        else if (dragLoop.getLength() > 1.0e-6)
        {
            owner.setLoop (dragLoop.getStart(), dragLoop.getEnd(), kind == DragKind::newLoop || owner.project.loopOn);
        }

        repaint();
        owner.repaintContent();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu() && owner.project.hasLoop())
            owner.setLoop (0.0, 0.0, false);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto& v = owner.view;
        const auto nearEdge = owner.project.hasLoop()
                              && (std::abs (e.position.x - (float) v.beatToX (owner.project.loopStart)) <= 5.0f
                                  || std::abs (e.position.x - (float) v.beatToX (owner.project.loopEnd)) <= 5.0f);
        setMouseCursor (nearEdge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::PointingHandCursor);
        owner.setHoverHint (tr ("Click  Set the start    Drag  Set a loop    Double-click  Remove the loop    Right-click  Menu"));
    }

    void mouseExit (const juce::MouseEvent&) override { owner.setHoverHint ({}); }

    void showMenu()
    {
        enum { loopSelectionId = 1, toggleLoopId, clearLoopId };
        juce::PopupMenu menu;
        menu.addItem (loopSelectionId, tr ("Loop the selected notes"), ! owner.selection.empty());
        menu.addItem (toggleLoopId, tr ("Loop playback"), owner.project.hasLoop(), owner.project.loopOn);
        menu.addItem (clearLoopId, tr ("Remove the loop"), owner.project.hasLoop());

        juce::Component::SafePointer<PianoRoll> safeOwner (&owner);
        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safeOwner] (int result)
        {
            auto* o = safeOwner.getComponent();

            if (o == nullptr)
                return;

            switch (result)
            {
                case loopSelectionId: o->toggleLoop(); break; // 有选中音符时：循环选区
                case toggleLoopId:    o->setLoop (o->project.loopStart, o->project.loopEnd, ! o->project.loopOn); break;
                case clearLoopId:     o->setLoop (0.0, 0.0, false); break;
                default: break;
            }
        });
    }

    enum class DragKind { none, newLoop, loopStart, loopEnd };

    PianoRoll& owner;
    DragKind dragKind = DragKind::none;
    juce::Range<double> dragLoop;
};

//==============================================================================
// 音符编辑区
struct PianoRoll::NoteArea final : public juce::Component
{
    explicit NoteArea (PianoRoll& o) : owner (o) {}

    enum class Mode { none, move, resize, trimStart, marquee, erase, rightPending, shiftPending,
                      curvePoint, curveTension, vibratoDepth, envelopePoint, vibratoSpeed, slice };

    /** 颤音的控件。 */
    enum class VibratoControl { none, depthLine, envelopePoint, addLine, speedPost };

    struct Hit
    {
        NoteId id = 0;
        int edge = 0; ///< -1 = 左端（改起点），1 = 右端（改时长），0 = 本体
    };

    /** 音符上的节点和手柄。 */
    struct CurveHit
    {
        NoteId id = 0;
        int point = -1;   ///< 节点下标
        int segment = -1; ///< 弯曲手柄所在的段（起点节点的下标）；琥珀线：所在的段（-1 = 整个音符）
        VibratoControl control = VibratoControl::none;
        int vibratoRun = -1;    ///< 控件属于第几段颤音（getVibratoRuns 的下标）
        int envelopePoint = -1; ///< 深度包络点的下标

        bool operator== (const CurveHit&) const = default;
    };

    /** 还没有颤音的一段（没有节点时是整个音符）：悬停时在它上方画一条琥珀线，往上拖就加颤音。 */
    struct VibratoLine
    {
        NoteId id = 0;
        int segment = -1;          ///< 起点节点的下标；-1 = 整个音符
        double start = 0.0, end = 0.0; ///< 相对音符起点（拍）

        bool operator== (const VibratoLine&) const = default;
    };

    //==============================================================================
    // 音符几何：音符是一条沿自身曲线弯曲的胶囊（圆头粗线）
    float bodyThickness() const
    {
        const auto rh = (float) owner.view.rowHeight;
        const auto pad = juce::jlimit (1.0f, 2.5f, rh * 0.14f);
        return rh - pad * 2.0f;
    }

    double secondsPerBeat() const { return 60.0 / juce::jmax (1.0, owner.processor.getHostBpm()); }

    /** 放大到每个颤音周期足够宽时，颤音直接画成音符本体的弯曲；否则画成本体里的细波纹。 */
    bool literalVibrato (const Note& n) const
    {
        const auto bpc = n.curve.minBeatsPerCycle (secondsPerBeat());
        return bpc > 0.0 && bpc * owner.view.pixelsPerBeat >= 28.0;
    }

    /** 音符中线在 time（相对音符起点）处的 y。 */
    float centreY (const Note& n, double time) const
    {
        const auto& v = owner.view;
        auto offset = n.curve.evaluate (time);

        if (literalVibrato (n))
            offset += n.curve.vibratoAt (time, n.length, secondsPerBeat());

        return (float) (v.pitchToY (n.pitch) + v.rowHeight * 0.5 - offset * v.rowHeight);
    }

    float pointY (const Note& n, double semitones) const
    {
        const auto& v = owner.view;
        return (float) (v.pitchToY (n.pitch) + v.rowHeight * 0.5 - semitones * v.rowHeight);
    }

    juce::Rectangle<float> noteBounds (const Note& n) const
    {
        const auto& v = owner.view;
        const auto x0 = (float) v.beatToX (n.start), x1 = (float) v.beatToX (n.getEnd());
        const auto h = bodyThickness();
        const auto y = centreY (n, 0.0) - h / 2.0f;
        return { x0 + 1.0f, y, juce::jmax (3.0f, x1 - x0 - 2.0f), h };
    }

    /** 音符本体的轮廓（填充用）。grow > 0 时整体加粗 grow 像素（选中、声部不够的描边用）。 */
    juce::Path bodyPath (const Note& n, float grow = 0.0f) const
    {
        const auto r = noteBounds (n);
        juce::Path path;

        if (n.curve.pointsFlat() && ! literalVibrato (n))
        {
            const auto outer = r.expanded (grow);
            path.addRoundedRectangle (outer, outer.getHeight() / 2.0f);
            return path;
        }

        // 中线从左端圆头的圆心画到右端圆头的圆心，再描成粗线
        const auto& v = owner.view;
        const auto half = r.getHeight() / 2.0f;
        const auto xa = r.getX() + half;
        const auto xb = juce::jmax (xa, r.getRight() - half);

        juce::Path centre;
        auto yAt = [&] (float x) { return centreY (n, juce::jlimit (0.0, n.length, v.xToBeat (x) - n.start)); };
        centre.startNewSubPath (xa, yAt (xa));

        constexpr float step = 2.0f;
        for (auto x = xa + step; x < xb; x += step)
            centre.lineTo (x, yAt (x));

        centre.lineTo (xb + 0.01f, yAt (xb)); // 长度为 0 时也要有一点，才画得出圆点

        juce::PathStrokeType (r.getHeight() + grow * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createStrokedPath (path, centre);
        return path;
    }

    Hit hitTest (juce::Point<float> pos) const
    {
        const auto& notes = owner.project.notes;
        const auto& v = owner.view;
        const auto rh = (float) v.rowHeight;

        // 后画的在上面，所以倒序查找
        for (auto it = notes.rbegin(); it != notes.rend(); ++it)
        {
            const auto r = noteBounds (*it);

            if (pos.x < r.getX() || pos.x > r.getRight())
                continue;

            // 沿曲线判断：离中线不超过半行就算命中（不弯的音符就是整行）
            const auto t = juce::jlimit (0.0, it->length, v.xToBeat (pos.x) - it->start);
            if (std::abs (pos.y - centreY (*it, t)) <= rh * 0.5f)
            {
                const auto edgeWidth = juce::jlimit (3.0f, 8.0f, r.getWidth() * 0.3f);
                const auto edge = pos.x >= r.getRight() - edgeWidth ? 1 : (pos.x <= r.getX() + edgeWidth ? -1 : 0);
                return { it->id, edge };
            }
        }

        return {};
    }

    /** 节点的屏幕位置；超出音符长度的节点（音符被缩短后）不显示。 */
    std::optional<juce::Point<float>> pointPosition (const Note& n, const CurvePoint& p) const
    {
        if (p.time > n.length + 1.0e-9)
            return std::nullopt;

        return juce::Point<float> ((float) owner.view.beatToX (n.start + p.time), pointY (n, p.semitones));
    }

    std::optional<juce::Point<float>> handlePosition (const Note& n, int segment) const
    {
        const auto& pts = n.curve.points;

        if (segment < 0 || segment + 1 >= (int) pts.size())
            return std::nullopt;

        const auto& a = pts[(size_t) segment];
        const auto& b = pts[(size_t) segment + 1];

        // 两端一样高（没有弯曲可调）或这一段太短时不显示手柄
        if (a.semitones == b.semitones || b.time > n.length + 1.0e-9
            || (b.time - a.time) * owner.view.pixelsPerBeat < 16.0)
            return std::nullopt;

        const auto mid = (a.time + b.time) * 0.5;
        return juce::Point<float> ((float) owner.view.beatToX (n.start + mid), pointY (n, n.curve.evaluate (mid)));
    }

    /** 颤音的控件（每一段颤音各一组，参数各自独立），画在音符本体上方，只在显示手柄的音符上出现：
        - 琥珀线：离本体上沿的高度 = 那个时刻的深度（一行 = 一个半音）。上下拖改变整段的深度（trill：相邻音的音程）；
          深度为 0 的地方正好是"往上拉加颤音"的那条线；
        - 线上的包络点（只有颤音有）：深度随时间的变化。拖动改变那一时刻的深度，双击线加点，右键删点；
        - 段中间的蓝色竖杆和顶上的横条：左右拖改变速度。 */

    /** 这一段颤音的琥珀线在 time（相对音符起点）处的 y。pts 是它的包络点（trill 为空）。 */
    float depthLineY (const Note& n, const PitchCurve::Run& run, const std::vector<Vibrato::EnvelopePoint>& pts, double time) const
    {
        const auto u = (time - run.start) / std::max (1.0e-9, run.length());
        const auto depth = std::abs (run.settings.depth) * Vibrato::levelAt (pts, u);
        return vibratoLineY (n, time) - (float) (depth * owner.view.rowHeight);
    }

    juce::Point<float> envelopePointPosition (const Note& n, const PitchCurve::Run& run, const Vibrato::EnvelopePoint& pt) const
    {
        const auto t = run.start + pt.x * run.length();
        return { (float) owner.view.beatToX (n.start + t), vibratoLineY (n, t) - (float) (std::abs (run.settings.depth) * pt.level * owner.view.rowHeight) };
    }

    /** 段太窄时不显示速度手柄（放不下，也容易挡住别的控件）。 */
    bool showsSpeedHandle (const PitchCurve::Run& run) const
    {
        return run.length() * owner.view.pixelsPerBeat >= 36.0;
    }

    /** 速度手柄：浮在这一段中间、琥珀线上方的蓝色小胶囊（里面左右两个小箭头，表示左右拖）。 */
    juce::Rectangle<float> speedBar (const Note& n, const PitchCurve::Run& run, const std::vector<Vibrato::EnvelopePoint>& pts) const
    {
        const auto mid = (run.start + run.end) * 0.5;
        const auto x = (float) owner.view.beatToX (n.start + mid);
        return juce::Rectangle<float> (26.0f, 14.0f).withCentre ({ x, depthLineY (n, run, pts, mid) - 16.0f });
    }

    /** 这一段颤音的控件在 pos 处命中的东西（control 为 none 表示没有命中）。 */
    CurveHit vibratoControlHit (const Note& n, const PitchCurve::Run& run, int runIndex, juce::Point<float> pos) const
    {
        const auto pts = run.settings.getEnvelope (run.length());
        CurveHit hit { n.id };
        hit.vibratoRun = runIndex;

        for (int i = (int) pts.size(); --i >= 0;)
        {
            if (envelopePointPosition (n, run, pts[(size_t) i]).getDistanceFrom (pos) <= 6.0f)
            {
                hit.control = VibratoControl::envelopePoint;
                hit.envelopePoint = i;
                return hit;
            }
        }

        if (showsSpeedHandle (run) && speedBar (n, run, pts).expanded (3.0f).contains (pos))
        {
            hit.control = VibratoControl::speedPost;
            return hit;
        }

        const auto t = owner.view.xToBeat (pos.x) - n.start;

        if (t >= run.start && t <= run.end && std::abs (pos.y - depthLineY (n, run, pts, t)) <= 4.5f)
        {
            hit.control = VibratoControl::depthLine;
            return hit;
        }

        return {};
    }

    /** 显示弯曲手柄、颤音手柄的音符：悬停（或正在编辑）的音符；没有时是唯一选中的音符。切开时不显示。 */
    NoteId handleNote() const
    {
        if (owner.slicing)
            return 0;

        if (focusNote != 0)
            return focusNote;

        return owner.selection.size() == 1 ? *owner.selection.begin() : 0;
    }

    /** 节点画出来并且可以直接拖的音符：选中的音符和显示手柄的音符。 */
    bool showsPoints (const Note& n) const
    {
        return ! owner.slicing && (n.id == handleNote() || owner.selection.count (n.id) != 0);
    }

    /** time（相对音符起点）所在、还没有颤音的一段。太短（画不下）时没有。 */
    std::optional<VibratoLine> vibratoLineAt (const Note& n, double time) const
    {
        const auto& c = n.curve;

        if (c.vibrato.wholeNote || time < 0.0 || time > n.length)
            return std::nullopt;

        VibratoLine line { n.id, -1, 0.0, n.length };

        if (! c.points.empty())
        {
            const auto seg = c.segmentAt (time);

            if (seg < 0 || c.points[(size_t) seg].vibrato)
                return std::nullopt;

            line.segment = seg;
            line.start = c.points[(size_t) seg].time;
            line.end = seg + 1 < (int) c.points.size() ? c.points[(size_t) seg + 1].time : n.length;
            line.end = std::min (line.end, n.length);
        }

        if ((line.end - line.start) * owner.view.pixelsPerBeat < 14.0)
            return std::nullopt;

        return line;
    }

    /** 琥珀线在 time 处的 y：本体上沿再往上一点。 */
    float vibratoLineY (const Note& n, double time) const
    {
        // 按主曲线算（不含颤音）：放大后颤音画成本体的弯曲时，线也不能跟着波形起伏
        return pointY (n, n.curve.evaluate (juce::jlimit (0.0, n.length, time))) - bodyThickness() * 0.5f - 7.0f;
    }

    /** 鼠标在悬停音符上方的琥珀线、颤音控件附近（或在它们与本体之间）时，仍然算悬停在这个音符上。 */
    bool nearVibratoLine (const Note& n, juce::Point<float> pos) const
    {
        const auto t = owner.view.xToBeat (pos.x) - n.start;

        if (t < 0.0 || t > n.length || pos.y > centreY (n, t))
            return false;

        if (hoverLine && hoverLine->id == n.id && t >= hoverLine->start && t <= hoverLine->end
            && pos.y >= vibratoLineY (n, t) - 7.0f)
            return true;

        for (const auto& run : n.curve.getVibratoRuns (n.length))
        {
            if (t < run.start || t > run.end)
                continue;

            const auto pts = run.settings.getEnvelope (run.length());
            const auto top = std::min (depthLineY (n, run, pts, t), speedBar (n, run, pts).getY()) - 8.0f;

            if (pos.y >= top)
                return true;
        }

        return false;
    }

    /** 某个音符上的节点和手柄。 */
    CurveHit curveHitOn (const Note& n, juce::Point<float> pos) const
    {
        for (int i = (int) n.curve.points.size(); --i >= 0;)
            if (const auto pt = pointPosition (n, n.curve.points[(size_t) i]); pt && pt->getDistanceFrom (pos) <= 7.0f)
                return { n.id, i, -1 };

        // 弯曲手柄、颤音控件只在显示手柄的音符上
        if (n.id != handleNote())
            return {};

        // 颤音控件画在本体上方，常常压在上一行的音符上：控件很细，所以优先于别的音符
        {
            const auto runs = n.curve.getVibratoRuns (n.length);

            for (int r = 0; r < (int) runs.size(); ++r)
                if (const auto hit = vibratoControlHit (n, runs[(size_t) r], r, pos); hit.control != VibratoControl::none)
                    return hit;
        }

        for (int i = 0; i + 1 < (int) n.curve.points.size(); ++i)
            if (const auto h = handlePosition (n, i); h && h->getDistanceFrom (pos) <= 6.0f)
                return { n.id, -1, i };

        // 琥珀线：往上拖加颤音
        if (hoverLine && hoverLine->id == n.id)
        {
            const auto t = owner.view.xToBeat (pos.x) - n.start;

            if (t >= hoverLine->start && t <= hoverLine->end && std::abs (pos.y - vibratoLineY (n, t)) <= 5.0f)
                return { n.id, -1, hoverLine->segment, VibratoControl::addLine };
        }

        return {};
    }

    /** 可以直接拖的节点和手柄（不含音符本体）：先看显示手柄的音符，再看选中的音符。
        音符两端的节点只有朝里的半边算节点，朝外的半边留给拖动改变起点、时长。 */
    CurveHit editableCurveHit (juce::Point<float> pos) const
    {
        if (owner.slicing)
            return {};

        auto test = [&] (const Note& n) -> CurveHit
        {
            auto hit = curveHitOn (n, pos);

            if (hit.point >= 0)
            {
                if (const auto p = pointPosition (n, n.curve.points[(size_t) hit.point]))
                {
                    const auto noteHit = hitTest (pos);

                    if (noteHit.id == n.id && ((noteHit.edge > 0 && pos.x >= p->x) || (noteHit.edge < 0 && pos.x <= p->x)))
                        return {};
                }
            }

            return hit;
        };

        if (const auto* focus = editing::findNote (owner.project, handleNote()))
            if (const auto hit = test (*focus); hit.id != 0)
                return hit;

        for (auto it = owner.project.notes.rbegin(); it != owner.project.notes.rend(); ++it)
            if (owner.selection.count (it->id) != 0)
                if (const auto hit = test (*it); hit.id != 0)
                    return hit;

        return {};
    }

    /** 鼠标位置 → 曲线节点（相对音符的拍、半音），默认吸附到网格和半音，Alt 不吸附。 */
    std::pair<double, double> mouseToCurve (const Note& n, juce::Point<float> pos, bool noSnap) const
    {
        const auto& v = owner.view;
        auto beat = v.xToBeat (pos.x);

        if (! noSnap)
            beat = editing::snapNearest (beat, owner.grid);

        auto semitones = (v.pitchToY (n.pitch) + v.rowHeight * 0.5 - pos.y) / v.rowHeight;

        if (! noSnap)
            semitones = std::round (semitones);

        return { juce::jlimit (0.0, n.length, beat - n.start), semitones };
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        const auto& v = owner.view;
        const auto w = (float) getWidth(), h = (float) getHeight();
        const auto rh = (float) v.rowHeight;

        // 行：黑键行加深，B/C 之间的分隔线加强
        for (int pitch = juce::jmin (127, v.yToPitch (0)); pitch >= juce::jmax (0, v.yToPitch (h)); --pitch)
        {
            const auto y = (float) v.pitchToY (pitch);

            if (isBlackKey (pitch))
            {
                g.setColour (p.blackRow);
                g.fillRect (0.0f, y, w, rh);
            }

            g.setColour (pitch % 12 == 0 ? p.lineStrong : p.line);
            g.fillRect (0.0f, y + rh - 1.0f, w, 1.0f);
        }

        // 竖线：吸附网格（足够宽时）、拍、小节
        auto drawLines = [&] (double step, juce::Colour colour)
        {
            if (step <= 0.0 || step * v.pixelsPerBeat < 7.0)
                return;

            g.setColour (colour);

            for (auto beat = std::ceil (v.startBeat / step - 1.0e-9) * step; beat <= v.xToBeat (w); beat += step)
                g.fillRect ((float) std::round (v.beatToX (beat)), 0.0f, 1.0f, h);
        };

        if (owner.grid > 0.0 && owner.grid < 1.0)
            drawLines (owner.grid, p.line.withMultipliedAlpha (0.6f));

        drawLines (1.0, p.line);
        drawLines (4.0, p.lineStrong);

        // 循环区外面稍微压暗（循环打开时）
        if (const auto loop = owner.ruler->loopRange(); ! loop.isEmpty() && (owner.project.loopOn || owner.ruler->dragKind == Ruler::DragKind::newLoop))
        {
            const auto x0 = (float) v.beatToX (loop.getStart()), x1 = (float) v.beatToX (loop.getEnd());
            g.setColour (p.text.withAlpha (0.045f));
            g.fillRect (0.0f, 0.0f, juce::jmax (0.0f, x0), h);
            g.fillRect (x1, 0.0f, juce::jmax (0.0f, w - x1), h);
        }

        // 音符：凸起的胶囊，沿自身曲线弯曲；颜色表示力度；选中的加暖色描边
        const auto handleId = handleNote();
        const Note* focus = nullptr;

        for (const auto& n : owner.project.notes)
        {
            const auto r = noteBounds (n);

            if (r.getRight() < 0.0f || r.getX() > w)
                continue;

            const auto path = bodyPath (n);

            if (! path.getBounds().intersects (juce::Rectangle<float> (0.0f, 0.0f, w, h)))
                continue;

            if (n.id == handleId)
                focus = &n;

            // 悬停和选中的音符显示节点，可以直接拖动
            const auto showPoints = showsPoints (n) && ! n.curve.points.empty();

            // 显示节点时，弯过的音符在原音高处画一条细线作参照
            if (showPoints && ! n.curve.isFlat())
            {
                const auto baseY = pointY (n, 0.0);
                const float dashes[] = { 3.0f, 3.0f };
                g.setColour (p.muted.withAlpha (0.45f));
                g.drawDashedLine ({ r.getX(), baseY, r.getRight(), baseY }, dashes, 2, 1.0f);
            }

            const auto colour = p.noteColour (n.velocity);
            neu::raisedFast (g, path, colour, p, 1.2f);


            // 描边：不能直接描本体的轮廓（弯折处粗线的轮廓会折进本体里，描出来的线互相交叠）。
            // 改为在下面铺一层加粗的本体，再把本体盖回去，只露出外圈
            if (owner.selection.count (n.id) != 0)
            {
                g.setColour (p.warm);
                g.fillPath (bodyPath (n, 1.6f));
                g.setColour (colour);
                g.fillPath (path);
            }
            else if (owner.overflowNotes.count (n.id) != 0)
            {
                // 声部不够：弯音会受别的音符影响。虚线画在加粗轮廓上，折进去的部分被本体盖住
                juce::Path dashed;
                const float dashes[] = { 4.0f, 3.0f };
                juce::PathStrokeType (1.6f).createDashedStroke (dashed, bodyPath (n, 0.8f), dashes, 2);
                g.setColour (p.error);
                g.fillPath (dashed);
                g.setColour (colour);
                g.fillPath (path);
            }

            if (r.getWidth() > 34.0f && r.getHeight() >= 10.0f)
            {
                g.setColour (p.textOn (colour).withAlpha (0.8f));
                g.setFont (juce::FontOptions (juce::jmin (11.0f, r.getHeight() - 1.0f)));
                g.drawText (noteName (n.pitch), r.withTrimmedLeft (r.getHeight() * 0.4f), juce::Justification::centredLeft, false);
            }

            // 颤音波纹叠在本体上面（放大到足够宽时本体自己弯曲，不再画波纹）
            if (n.curve.hasVibrato() && ! literalVibrato (n))
                paintVibratoWave (g, n, colour);

            if (showPoints)
                paintCurvePoints (g, n, false);
        }

        // 正在编辑的音符：节点画在最上面，并显示弯曲手柄、颤音手柄和加颤音的琥珀线
        if (focus != nullptr)
        {
            paintVibratoLine (g, *focus);
            paintCurvePoints (g, *focus, true);
        }

        paintSliceLine (g);

        if (mode == Mode::marquee && ! marquee.isEmpty())
        {
            g.setColour (p.accent.withAlpha (0.12f));
            g.fillRect (marquee);
            g.setColour (p.accent.withAlpha (0.6f));
            g.drawRect (marquee, 1.0f);
        }

        if (owner.playheadBeat >= 0.0)
        {
            g.setColour (p.accent.withAlpha (0.75f));
            g.fillRect ((float) v.beatToX (owner.playheadBeat) - 1.0f, 0.0f, 2.0f, h);
        }
    }

    /** 颤音画成叠在本体上的细波纹（只画颤音段），振幅等于真实深度，超出本体也照样画出来。
        周期太密时画成随包络变宽的半透明带。 */
    void paintVibratoWave (juce::Graphics& g, const Note& n, juce::Colour fill) const
    {
        const auto& p = owner.palette;
        const auto& v = owner.view;
        const auto spb = secondsPerBeat();
        const auto rh = (float) v.rowHeight;

        // 力度高的音符颜色接近强调色，这时改用浅色，保证看得清
        const auto ink = std::abs (fill.getPerceivedBrightness() - p.accent.getPerceivedBrightness()) < 0.18f ? p.textOn (fill) : p.accent;

        for (const auto& run : n.curve.getVibratoRuns (n.length))
        {
            const auto& vib = run.settings;

            if (vib.depth == 0.0)
                continue;

            const auto cyclePx = vib.beatsPerCycle (spb) * v.pixelsPerBeat;
            const auto x0 = juce::jmax (-2.0f, (float) v.beatToX (n.start + run.start));
            const auto x1 = juce::jmin ((float) getWidth() + 2.0f, (float) v.beatToX (n.start + run.end));

            if (x1 <= x0)
                continue;

            auto timeAt = [&] (float x) { return juce::jlimit (run.start, run.end, v.xToBeat (x) - n.start); };
            auto amplitude = [&] (double t) { return (float) (vib.depth * vib.envelope (t - run.start, run.length())) * rh; };
            const auto trill = vib.shape == Vibrato::Shape::trill;

            if (cyclePx < 6.0)
            {
                // 颤音：上下对称；trill：本音到相邻音之间
                juce::Path band;
                band.startNewSubPath (x0, centreY (n, timeAt (x0)));

                for (auto x = x0; x <= x1; x += 2.0f)
                    band.lineTo (x, centreY (n, timeAt (x)) - amplitude (timeAt (x)));

                for (auto x = x1; x >= x0; x -= 2.0f)
                    band.lineTo (x, centreY (n, timeAt (x)) + (trill ? 0.0f : amplitude (timeAt (x))));

                band.closeSubPath();
                g.setColour (ink.withAlpha (0.35f));
                g.fillPath (band);
                continue;
            }

            juce::Path wave;
            const auto step = (float) juce::jlimit (0.5, 2.0, cyclePx / (trill ? 40.0 : 10.0)); // trill 的切换很陡，采样密一些
            wave.startNewSubPath (x0, centreY (n, timeAt (x0)));

            for (auto x = x0 + step; x <= x1; x += step)
            {
                const auto t = timeAt (x);
                wave.lineTo (x, centreY (n, t) - (float) vib.evaluate (t - run.start, run.length(), spb) * rh);
            }

            wave.lineTo (x1, centreY (n, timeAt (x1)));

            // 先描一圈背景色的细边，在任何底色上都看得清
            g.setColour (p.bg.withAlpha (0.55f));
            g.strokePath (wave, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (ink);
            g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    static juce::String formatRate (const Vibrato& vib)
    {
        if (vib.sync)
            return editing::syncRateName (vib);

        return juce::String (vib.rate, 1) + " Hz";
    }

    /** +2 st、-0.25 st 这样的偏移。 */
    static juce::String formatSemitones (double st)
    {
        const auto sign = juce::String (st > 0 ? "+" : "");

        if (std::abs (st - std::round (st)) < 1.0e-6)
            return sign + juce::String ((int) std::round (st)) + " st";

        return sign + juce::String (st, 2) + " st";
    }

    void paintValueLabel (juce::Graphics& g, const juce::String& text, juce::Point<float> centre, juce::Colour colour) const
    {
        const auto& p = owner.palette;
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        const auto width = (float) juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), text) + 14.0f;
        const auto box = juce::Rectangle<float> (width, 18.0f).withCentre (centre);
        g.setColour (p.bg.withAlpha (0.9f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (colour);
        g.drawText (text, box, juce::Justification::centred, false);
    }

    /** 沿 [t0, t1]（相对音符起点，拍）每 2 像素取一个点连成的线，只取看得见的部分。 */
    juce::Path sampledLine (const Note& n, double t0, double t1, const std::function<float (double)>& yAt) const
    {
        const auto& v = owner.view;
        const auto x0 = juce::jmax ((float) v.beatToX (n.start + t0), -4.0f);
        const auto x1 = juce::jmin ((float) v.beatToX (n.start + t1), (float) getWidth() + 4.0f);
        juce::Path line;

        if (x1 <= x0)
            return line;

        auto timeAt = [&] (float x) { return juce::jlimit (t0, t1, v.xToBeat (x) - n.start); };
        line.startNewSubPath (x0, yAt (timeAt (x0)));

        for (auto x = x0 + 2.0f; x < x1; x += 2.0f)
            line.lineTo (x, yAt (timeAt (x)));

        line.lineTo (x1, yAt (timeAt (x1)));
        return line;
    }

    /** 先描一圈背景色的边，在任何底色上都看得清。 */
    void strokeWithHalo (juce::Graphics& g, const juce::Path& path, juce::Colour colour, float thickness) const
    {
        const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;
        g.setColour (owner.palette.bg.withAlpha (0.8f));
        g.strokePath (path, juce::PathStrokeType (thickness + 2.0f, joint, juce::PathStrokeType::rounded));
        g.setColour (colour);
        g.strokePath (path, juce::PathStrokeType (thickness, joint, juce::PathStrokeType::rounded));
    }

    /** 悬停的那一段上方的琥珀线（还没有颤音的段才有）。拖动时不画（波纹已经出来了）。 */
    void paintVibratoLine (juce::Graphics& g, const Note& n) const
    {
        if (! hoverLine || hoverLine->id != n.id || mode != Mode::none)
            return;

        const auto hovered = hoverControl.control == VibratoControl::addLine;
        const auto line = sampledLine (n, hoverLine->start, hoverLine->end, [&] (double t) { return vibratoLineY (n, t); });
        strokeWithHalo (g, line, owner.palette.warm.withAlpha (hovered ? 1.0f : 0.75f), hovered ? 4.0f : 2.5f);
    }

    void paintVibratoHandles (juce::Graphics& g, const Note& n) const
    {
        const auto runs = n.curve.getVibratoRuns (n.length);

        for (int r = 0; r < (int) runs.size(); ++r)
            paintVibratoControls (g, n, runs[(size_t) r], r);
    }

    void paintVibratoControls (juce::Graphics& g, const Note& n, const PitchCurve::Run& run, int runIndex) const
    {
        const auto& p = owner.palette;
        const auto& v = owner.view;
        const auto& vib = run.settings;
        const auto trill = vib.shape == Vibrato::Shape::trill;
        const auto pts = vib.getEnvelope (run.length());

        const auto x0 = (float) v.beatToX (n.start + run.start);
        const auto x1 = (float) v.beatToX (n.start + run.end);

        if (x1 - x0 < 4.0f)
            return;

        // 正在拖、悬停的控件
        const auto dragged = owner.dragging && n.id == focusNote && runIndex == dragRunIndex;
        const auto draggedControl = ! dragged ? VibratoControl::none
                                  : mode == Mode::vibratoDepth ? VibratoControl::depthLine
                                  : mode == Mode::envelopePoint ? VibratoControl::envelopePoint
                                  : mode == Mode::vibratoSpeed ? VibratoControl::speedPost : VibratoControl::none;
        auto isActive = [&] (VibratoControl control, int point = -1)
        {
            if (draggedControl == control && (point < 0 || point == dragEnvelopePoint))
                return true;

            return ! owner.dragging && hoverControl.id == n.id && hoverControl.vibratoRun == runIndex
                   && hoverControl.control == control && (point < 0 || hoverControl.envelopePoint == point);
        };

        // 琥珀线
        const auto line = sampledLine (n, run.start, run.end, [&] (double t) { return depthLineY (n, run, pts, t); });
        strokeWithHalo (g, line, p.warm, isActive (VibratoControl::depthLine) ? 3.5f : 2.5f);

        // 包络点
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const auto pos = envelopePointPosition (n, run, pts[(size_t) i]);
            const auto size = isActive (VibratoControl::envelopePoint, i) ? 10.0f : 7.0f;
            const auto dot = juce::Rectangle<float> (size, size).withCentre (pos);
            g.setColour (p.bg);
            g.fillEllipse (dot.expanded (1.5f));
            g.setColour (p.warm);
            g.fillEllipse (dot);
        }

        // 速度：浮在琥珀线上方的蓝色小胶囊，里面左右两个小箭头
        if (showsSpeedHandle (run))
        {
            const auto speedActive = isActive (VibratoControl::speedPost);
            const auto pill = speedActive ? speedBar (n, run, pts).expanded (1.5f) : speedBar (n, run, pts);
            const auto radius = pill.getHeight() * 0.5f;

            g.setColour (p.bg.withAlpha (0.85f));
            g.fillRoundedRectangle (pill.expanded (1.5f), radius + 1.5f);
            g.setColour (p.accent.withAlpha (speedActive ? 1.0f : 0.9f));
            g.fillRoundedRectangle (pill, radius);

            const auto c = pill.getCentre();
            const auto a = 3.2f;         // 箭头半高
            const auto gap = 2.2f;       // 两个箭头离中心的距离
            juce::Path arrows;
            arrows.addTriangle (c.x - gap, c.y - a, c.x - gap, c.y + a, c.x - gap - a * 1.25f, c.y);
            arrows.addTriangle (c.x + gap, c.y - a, c.x + gap, c.y + a, c.x + gap + a * 1.25f, c.y);
            g.setColour (p.textOn (p.accent));
            g.fillPath (arrows);
        }

        // 数值
        if (draggedControl == VibratoControl::depthLine || draggedControl == VibratoControl::envelopePoint)
        {
            const auto* point = draggedControl == VibratoControl::envelopePoint && dragEnvelopePoint >= 0 && dragEnvelopePoint < (int) pts.size()
                                    ? &pts[(size_t) dragEnvelopePoint] : nullptr;
            const auto text = trill ? trf ("Trill {0}", formatSemitones (vib.depth))
                                    : juce::String (std::abs (vib.depth) * (point != nullptr ? point->level : 1.0), 2) + " st";
            const auto labelAt = point != nullptr ? envelopePointPosition (n, run, *point)
                                                  : juce::Point<float> (x1, depthLineY (n, run, pts, run.end));
            paintValueLabel (g, text, labelAt.translated (0.0f, -16.0f), p.text);
        }

        if (draggedControl == VibratoControl::speedPost)
            paintValueLabel (g, formatRate (vib), speedBar (n, run, pts).getCentre().translated (0.0f, -18.0f), p.text);
    }

    void paintCurvePoints (juce::Graphics& g, const Note& n, bool focused) const
    {
        const auto& p = owner.palette;
        const auto range = owner.project.bendRange;

        if (focused)
        {
            for (int i = 0; i + 1 < (int) n.curve.points.size(); ++i)
            {
                if (const auto pos = handlePosition (n, i))
                {
                    const auto active = mode == Mode::curveTension && curveIndex == i;
                    g.setColour (p.bg);
                    g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (*pos));
                    g.setColour (active ? p.warm : p.text.withAlpha (0.7f));
                    g.drawEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (*pos), 1.5f);
                }
            }
        }

        if (focused)
            paintVibratoHandles (g, n);

        for (int i = 0; i < (int) n.curve.points.size(); ++i)
        {
            const auto& point = n.curve.points[(size_t) i];
            const auto pos = pointPosition (n, point);

            if (! pos)
                continue;

            // 超出弯音范围的节点标成错误色（发声时会被截断）
            const auto outOfRange = std::abs (point.semitones) > range + 1.0e-9;
            const auto active = focused && mode == Mode::curvePoint && curveIndex == i;
            const auto size = active ? 11.0f : (focused ? 9.0f : 7.0f);
            const auto dot = juce::Rectangle<float> (size, size).withCentre (*pos);

            g.setColour (p.bg);
            g.fillEllipse (dot.expanded (1.5f));
            g.setColour (outOfRange ? p.error : (active ? p.warm : p.accent));
            g.fillEllipse (dot);

            // 拖动时显示数值
            if (active)
            {
                const auto text = formatSemitones (point.semitones);

                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                const auto box = juce::Rectangle<float> (64.0f, 18.0f).withCentre (pos->translated (0.0f, -16.0f));
                g.setColour (p.bg.withAlpha (0.9f));
                g.fillRoundedRectangle (box, 6.0f);
                g.setColour (outOfRange ? p.error : p.text);
                g.drawText (text, box, juce::Justification::centred, false);
            }
        }
    }

    //==============================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();

        const auto& v = owner.view;
        downBeat = v.xToBeat (e.position.x);
        downPitch = v.yToPitch (e.position.y);
        mode = Mode::none;
        curveIndex = -1;
        pressedNote = 0;
        duplicating = false;

        // 按住 S：在这个位置画一条竖线，松开鼠标时切开它经过的音符（单击 = 只切鼠标下的音符）
        if (owner.slicing && ! e.mods.isPopupMenu())
        {
            mode = Mode::slice;
            sliceBeat = sliceBeatAt (e.position.x, e.mods.isAltDown());
            sliceY0 = sliceY1 = e.position.y;
            repaint();
            return;
        }

        // 悬停或选中的音符上的节点、手柄：直接拖；右键节点删除它（右键其他手柄 = 右键音符）
        if (! e.mods.isShiftDown())
        {
            const auto curveHit = editableCurveHit (e.position);
            const auto deletable = curveHit.point >= 0 || curveHit.control == VibratoControl::envelopePoint;

            if (curveHit.id != 0 && (deletable || ! e.mods.isPopupMenu()))
            {
                setFocusNote (curveHit.id);

                if (e.mods.isPopupMenu())
                {
                    if (curveHit.point >= 0)
                        deleteCurvePoint (curveHit.id, curveHit.point);
                    else
                        deleteEnvelopePoint (curveHit);
                    return;
                }

                // 编辑颤音时选中这个音符，鼠标移开后控件仍然显示
                if (curveHit.control != VibratoControl::none && curveHit.control != VibratoControl::addLine && owner.selection.count (curveHit.id) == 0)
                    owner.setSelection ({ curveHit.id });

                owner.beginDrag();
                base = owner.dragOrigin;

                if (auto* n = editing::findNote (base, curveHit.id))
                {
                    // 双击琥珀线：在这里加一个包络点，接着可以拖
                    if (curveHit.control == VibratoControl::depthLine && e.getNumberOfClicks() >= 2)
                    {
                        if (const auto added = addEnvelopePoint (*n, curveHit, e.position.x); added.control == VibratoControl::envelopePoint)
                        {
                            owner.updateDrag (base);
                            beginHandleDrag (*editing::findNote (base, curveHit.id), added);
                            return;
                        }
                    }

                    beginHandleDrag (*n, curveHit);
                }
                return;
            }
        }

        const auto hit = hitTest (e.position);

        // 右键：单击弹出菜单，拖动删除经过的音符（在 mouseDrag / mouseUp 里决定）
        if (e.mods.isPopupMenu())
        {
            mode = Mode::rightPending;
            pressedNote = hit.id;
            return;
        }

        if (hit.id != 0)
        {
            if (e.getNumberOfClicks() < 2)
                lastAddedNote = 0;

            // 双击音符本体：加一个节点，按住可以接着拖（刚画出来的音符除外：那是在空白处双击）
            if (e.getNumberOfClicks() >= 2 && hit.edge == 0 && ! e.mods.isShiftDown() && hit.id != lastAddedNote)
            {
                owner.setSelection ({ hit.id });
                setFocusNote (hit.id);
                owner.beginDrag();
                base = owner.dragOrigin;

                if (auto* n = editing::findNote (base, hit.id))
                {
                    const auto [t, st] = mouseToCurve (*n, e.position, e.mods.isAltDown());
                    curveIndex = editing::insertCurvePoint (n->curve, t, st);
                    mode = Mode::curvePoint;
                    curveEditName = "Add bend point";
                    owner.updateDrag (base);
                }
                return;
            }

            // Shift：单击加选 / 取消选中，拖动复制
            if (e.mods.isShiftDown())
            {
                mode = Mode::shiftPending;
                pressedNote = hit.id;
                return;
            }

            if (owner.selection.count (hit.id) == 0)
                owner.setSelection ({ hit.id });

            owner.beginDrag();
            base = owner.dragOrigin;
            anchor = hit.id;
            mode = hit.edge > 0 ? Mode::resize : (hit.edge < 0 ? Mode::trimStart : Mode::move);

            if (const auto* n = editing::findNote (base, anchor))
            {
                anchorPitch = n->pitch;
                owner.preview (n->pitch, n->velocity);
            }
            return;
        }

        lastAddedNote = 0;

        // 空白处：按住 Ctrl → 框选；否则添加音符
        if (e.mods.isCommandDown())
        {
            mode = Mode::marquee;
            marqueeBase = e.mods.isShiftDown() ? owner.selection : editing::IdSet {};
            marquee = {};

            if (! e.mods.isShiftDown())
                owner.setSelection ({});
            return;
        }

        owner.beginDrag();
        base = owner.dragOrigin;

        const auto start = juce::jmax (0.0, e.mods.isAltDown() ? downBeat : editing::snapFloor (downBeat, owner.grid));
        const auto pitch = juce::jlimit (0, 127, downPitch);
        anchor = base.addNote (start, owner.defaultLength, pitch, defaultVelocity);
        anchorPitch = pitch;
        lastAddedNote = anchor;

        owner.updateDrag (base);
        owner.setSelection ({ anchor });
        owner.preview (pitch, defaultVelocity);
        mode = Mode::resize;
    }

    /** Shift + 拖动音符：在原处留下一份副本，拖走的是原来的音符（选中的音符被按住时复制整个选区）。 */
    void beginDuplicateDrag()
    {
        auto ids = owner.selection;

        if (ids.count (pressedNote) == 0)
            ids = { pressedNote };

        owner.setSelection (ids);
        owner.beginDrag();
        base = owner.dragOrigin;

        editing::duplicateInPlace (base, ids);

        anchor = pressedNote;
        duplicating = true;
        mode = Mode::move;

        if (const auto* n = editing::findNote (base, anchor))
        {
            anchorPitch = n->pitch;
            owner.preview (n->pitch, n->velocity);
        }

        owner.updateDrag (base);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto& v = owner.view;
        const auto beat = v.xToBeat (e.position.x);
        const auto pitch = v.yToPitch (e.position.y);
        const bool noSnap = e.mods.isAltDown();
        owner.lastMouseBeat = beat;

        // 右键、Shift 按下后移动了一段距离才算拖动：右键拖 = 删除，Shift 拖 = 复制
        if ((mode == Mode::rightPending || mode == Mode::shiftPending) && e.getDistanceFromDragStart() > 4)
        {
            if (mode == Mode::rightPending)
            {
                mode = Mode::erase;
                owner.beginDrag();
                eraseAt (e.mouseDownPosition);
            }
            else if (pressedNote != 0)
            {
                beginDuplicateDrag();
            }
        }

        switch (mode)
        {
            case Mode::rightPending:
            case Mode::shiftPending:
                break;

            case Mode::trimStart:
            {
                const auto* a = editing::findNote (base, anchor);
                if (a == nullptr)
                    break;

                const auto target = a->start + (beat - downBeat);
                const auto snapped = noSnap ? target : editing::snapNearest (target, owner.grid);
                const auto minLength = (noSnap || owner.grid <= 0.0) ? editing::minNoteLength : owner.grid;

                auto working = base;
                editing::trimNoteStarts (working, base, owner.selection, snapped - a->start, minLength);
                owner.updateDrag (std::move (working));
                break;
            }

            case Mode::move:
            {
                const auto* a = editing::findNote (base, anchor);
                if (a == nullptr)
                    break;

                const auto target = a->start + (beat - downBeat);
                const auto snapped = noSnap ? target : editing::snapNearest (target, owner.grid);

                auto working = base;
                editing::moveNotes (working, base, owner.selection, snapped - a->start, pitch - downPitch);
                owner.updateDrag (std::move (working));

                // 音高变了就重新试听
                if (const auto* moved = editing::findNote (owner.project, anchor); moved != nullptr && moved->pitch != anchorPitch)
                {
                    anchorPitch = moved->pitch;
                    owner.preview (moved->pitch, moved->velocity);
                }
                break;
            }

            case Mode::resize:
            {
                const auto* a = editing::findNote (base, anchor);
                if (a == nullptr)
                    break;

                const auto targetEnd = a->getEnd() + (beat - downBeat);
                const auto snappedEnd = noSnap ? targetEnd : editing::snapNearest (targetEnd, owner.grid);
                const auto minLength = (noSnap || owner.grid <= 0.0) ? editing::minNoteLength : owner.grid;

                auto working = base;
                editing::resizeNotes (working, base, owner.selection, snappedEnd - a->getEnd(), minLength);
                owner.updateDrag (std::move (working));
                break;
            }

            case Mode::marquee:
            {
                marquee = juce::Rectangle<float> (e.mouseDownPosition, e.position);

                auto ids = editing::notesInRange (owner.project, v.xToBeat (marquee.getX()), v.xToBeat (marquee.getRight()),
                                                  v.yToPitch (marquee.getBottom()), v.yToPitch (marquee.getY()));
                ids.insert (marqueeBase.begin(), marqueeBase.end());
                owner.setSelection (std::move (ids));
                repaint();
                break;
            }

            case Mode::erase:
                eraseAt (e.position);
                break;

            case Mode::curvePoint:
            {
                auto working = base;

                if (auto* n = editing::findNote (working, focusNote))
                {
                    const auto [t, st] = mouseToCurve (*n, e.position, noSnap);
                    editing::moveCurvePoint (n->curve, curveIndex, t, st, n->length);
                    previewPoint (*n);
                    owner.updateDrag (std::move (working));
                }
                break;
            }

            case Mode::curveTension:
            {
                auto working = base;

                if (auto* n = editing::findNote (working, focusNote); n != nullptr
                    && curveIndex + 1 < (int) n->curve.points.size())
                {
                    // 往下拖，曲线往下弯；往上拖，往上弯（与这一段是升是降无关）
                    const auto& a = n->curve.points[(size_t) curveIndex];
                    const auto& b = n->curve.points[(size_t) curveIndex + 1];
                    const auto direction = b.semitones > a.semitones ? 1.0f : -1.0f;
                    const auto dy = e.position.y - e.mouseDownPosition.y;
                    editing::setCurveTension (n->curve, curveIndex, tensionStart + direction * dy / 80.0f);
                    owner.updateDrag (std::move (working));
                }
                break;
            }

            case Mode::vibratoDepth:
            case Mode::envelopePoint:
            case Mode::vibratoSpeed:
            {
                auto working = base;
                auto* n = editing::findNote (working, focusNote);

                if (n == nullptr)
                    break;

                auto vib = dragRun.settings;
                const auto t = juce::jlimit (dragRun.start, dragRun.end, owner.view.xToBeat (e.position.x) - n->start);
                const auto offset = (vibratoLineY (*n, t) - e.position.y) / owner.view.rowHeight; // 鼠标处想要的深度（半音）

                if (mode == Mode::vibratoSpeed)
                {
                    // 左右拖：往右更快，往左更慢（每 80 像素一倍）
                    const auto factor = std::pow (2.0, (e.position.x - e.mouseDownPosition.x) / 80.0);

                    const auto rate = dragRun.settings.rate * factor;

                    if (vib.sync)
                        vib.rate = editing::snapSyncRate (vib, rate);
                    else
                        vib.rate = noSnap ? rate : std::round (rate * 10.0) / 10.0;
                }
                else if (vib.shape == Vibrato::Shape::trill)
                {
                    // trill：线的高度是相邻音的音程，吸附到半音；拖到零线下方换成另一边的音。不能是 0
                    const auto sign = (dragRun.settings.depth < 0.0 ? -1.0 : 1.0) * (offset < 0.0 ? -1.0 : 1.0);
                    auto interval = noSnap ? std::round (std::abs (offset) * 20.0) / 20.0 : (double) std::lround (std::abs (offset));
                    interval = std::max (interval, noSnap ? 0.05 : 1.0);
                    vib.depth = juce::jlimit (-12.0, 12.0, sign * interval);
                }
                else if (mode == Mode::envelopePoint)
                {
                    auto depth = std::max (0.0, offset);

                    if (! noSnap)
                        depth = std::round (depth * 20.0) / 20.0;

                    editing::moveEnvelopePoint (vib, dragRun.length(), dragEnvelopePoint,
                                                (t - dragRun.start) / std::max (1.0e-9, dragRun.length()), depth);
                }
                else
                {
                    // 拖琥珀线（包括往上拉加颤音）：整段的深度跟着鼠标上下移动（一行 = 一个半音），包络的形状不变
                    auto depth = dragRun.settings.depth + (e.mouseDownPosition.y - e.position.y) / owner.view.rowHeight;

                    if (! noSnap)
                        depth = std::round (depth * 20.0) / 20.0;

                    vib.depth = juce::jlimit (0.0, 12.0, depth);
                }

                vib.normalise();
                editing::setRunVibrato (n->curve, dragRun, vib);
                owner.updateDrag (std::move (working));
                break;
            }

            case Mode::slice:
                sliceY1 = e.position.y;
                repaint();
                break;

            case Mode::none:
                break;
        }
    }

    //==============================================================================
    // Slice 工具

    double sliceBeatAt (float x, bool noSnap) const
    {
        const auto beat = owner.view.xToBeat (x);
        return noSnap ? beat : editing::snapNearest (beat, owner.grid);
    }

    /** 竖线 [y0, y1] 在 beat 处经过的音符（按音符本体在这个时刻的位置判断，弯过的音符也算）。 */
    editing::IdSet notesUnderSlice (double beat, float y0, float y1) const
    {
        const auto half = (float) owner.view.rowHeight * 0.5f;
        const auto top = juce::jmin (y0, y1) - half, bottom = juce::jmax (y0, y1) + half;
        editing::IdSet ids;

        for (const auto& n : owner.project.notes)
        {
            if (beat <= n.start || beat >= n.getEnd())
                continue;

            const auto y = centreY (n, beat - n.start);

            if (y >= top && y <= bottom)
                ids.insert (n.id);
        }

        return ids;
    }

    void finishSlice()
    {
        const auto ids = notesUnderSlice (sliceBeat, sliceY0, sliceY1);
        const auto beat = sliceBeat;

        if (ids.empty())
            return;

        owner.sliceAndSelect (ids, beat);
    }

    void paintSliceLine (juce::Graphics& g) const
    {
        if (! owner.slicing || (mode != Mode::slice && hoverSliceBeat < 0.0))
            return;

        const auto& p = owner.palette;
        const auto beat = mode == Mode::slice ? sliceBeat : hoverSliceBeat;
        const auto x = (float) owner.view.beatToX (beat);
        const float dashes[] = { 4.0f, 3.0f };

        if (mode == Mode::slice)
        {
            // 正在拖：实线标出会切到的范围，虚线延伸到整列
            const auto half = (float) owner.view.rowHeight * 0.5f;
            const auto top = juce::jmin (sliceY0, sliceY1) - half, bottom = juce::jmax (sliceY0, sliceY1) + half;

            g.setColour (p.warm.withAlpha (0.35f));
            g.drawDashedLine ({ x, 0.0f, x, (float) getHeight() }, dashes, 2, 1.0f);
            g.setColour (p.warm);
            g.fillRect (x - 1.0f, top, 2.0f, bottom - top);
            return;
        }

        g.setColour (p.warm.withAlpha (0.55f));
        g.drawDashedLine ({ x, 0.0f, x, (float) getHeight() }, dashes, 2, 1.2f);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverSliceBeat >= 0.0)
        {
            hoverSliceBeat = -1.0;
            repaint();
        }

        if (mode == Mode::none)
        {
            clearHover();
            setFocusNote (0);
        }

        owner.setHoverHint ({});
    }

    //==============================================================================
    /** 右键音符的菜单。atBeat 是右键的位置（拍），用来确定"这一段"和切开的位置。 */
    void showNoteMenu (NoteId id, double atBeat)
    {
        const auto* note = editing::findNote (owner.project, id);

        if (note == nullptr)
            return;

        const auto& curve = note->curve;
        const auto local = atBeat - note->start;
        const auto segment = curve.segmentAt (local);
        const auto whole = curve.vibrato.wholeNote;
        const auto wholeShape = curve.vibrato.shape;

        // 这一段现在的颤音（每一段有自己的形状和参数）
        std::optional<Vibrato::Shape> segmentShape;
        if (! whole && segment >= 0 && curve.points[(size_t) segment].vibrato)
            segmentShape = curve.points[(size_t) segment].vibrato->shape;

        // 右键位置所在的那一段颤音（速度跟随曲速只改这一段）
        PitchCurve::Run clickedRun;
        const auto inRun = curve.findVibratoRun (local, note->length, clickedRun);
        const auto runs = curve.getVibratoRuns (note->length);
        const auto syncChecked = inRun ? clickedRun.settings.sync : (! runs.empty() && runs.front().settings.sync);

        const auto hasVibrato = curve.hasVibrato();
        const auto hasSine = curve.hasVibratoShape (Vibrato::Shape::sine);
        const auto hasTrill = curve.hasVibratoShape (Vibrato::Shape::trill);
        const auto targets = owner.selection.count (id) != 0 ? owner.selection : editing::IdSet { id };
        const auto presets = owner.presetLibrary.load();

        enum { vibSegmentId = 1, vibWholeId, trillSegmentId, trillWholeId, syncId, removeId, clearId, glideId,
               savePresetId, noPresetsId, sliceId, deleteId, shapeBase = 100, presetBase = 1000, deleteBase = 5000 };

        // 切开的位置：吸附到网格，离音符两端太近时不能切
        const auto sliceAt = editing::snapNearest (atBeat, owner.grid);
        const auto canSlice = std::any_of (targets.begin(), targets.end(), [&] (NoteId t)
        {
            const auto* n = editing::findNote (owner.project, t);
            return n != nullptr && editing::canSlice (*n, sliceAt);
        });

        juce::PopupMenu menu;

        if (targets.size() >= 2)
        {
            menu.addItem (glideId, tr ("Glide into legato notes (G)"));
            menu.addSeparator();
        }

        const auto noSegment = segment < 0;
        menu.addSectionHeader (tr ("Vibrato and trill"));
        menu.addItem (vibSegmentId, noSegment ? tr ("Vibrato on this segment (add points first)") : tr ("Vibrato on this segment"),
                      ! noSegment, segmentShape == Vibrato::Shape::sine);
        menu.addItem (vibWholeId, tr ("Vibrato on whole note"), true, whole && wholeShape == Vibrato::Shape::sine);
        menu.addItem (trillSegmentId, noSegment ? tr ("Trill on this segment (add points first)") : tr ("Trill on this segment"),
                      ! noSegment, segmentShape == Vibrato::Shape::trill);
        menu.addItem (trillWholeId, tr ("Trill on whole note"), true, whole && wholeShape == Vibrato::Shape::trill);
        menu.addItem (syncId, tr ("Lock speed to tempo"), hasVibrato, syncChecked);
        menu.addItem (removeId, hasSine && hasTrill ? tr ("Remove vibrato and trill") : hasTrill ? tr ("Remove trill") : tr ("Remove vibrato"),
                      hasVibrato);
        menu.addSeparator();
        menu.addSectionHeader (tr ("Bend"));

        struct ShapeItem { const char* name; editing::Shape shape; };
        static const ShapeItem shapes[] = {
            { TRK ("Scoop (slide in from below)"), editing::Shape::scoop },
            { TRK ("Overshoot (go past, settle back)"), editing::Shape::overshoot },
            { TRK ("Bend and release"), editing::Shape::bendRelease },
            { TRK ("Fall (drop at the end)"), editing::Shape::fall },
            { TRK ("Doit (flick up at the end)"), editing::Shape::doit },
        };

        for (int i = 0; i < (int) std::size (shapes); ++i)
            menu.addItem (shapeBase + i, tr (shapes[i].name));

        // 预设：套用到所有选中的音符（按音符长度伸缩）
        juce::PopupMenu presetMenu;
        const auto shownPresets = juce::jmin ((int) presets.size(), deleteBase - presetBase);

        for (int i = 0; i < shownPresets; ++i)
            presetMenu.addItem (presetBase + i, presets[(size_t) i].name);

        if (presets.empty())
            presetMenu.addItem (noPresetsId, tr ("No presets yet"), false);

        presetMenu.addSeparator();
        presetMenu.addItem (savePresetId, curve.isFlat() ? tr ("Save curve as preset (draw a curve first)") : tr ("Save curve as preset..."),
                            ! curve.isFlat());

        if (! presets.empty())
        {
            juce::PopupMenu deleteMenu;

            for (int i = 0; i < shownPresets; ++i)
                deleteMenu.addItem (deleteBase + i, presets[(size_t) i].name);

            presetMenu.addSubMenu (tr ("Delete preset"), deleteMenu);
        }

        menu.addSeparator();
        menu.addSubMenu (tr ("Presets"), presetMenu);
        menu.addItem (clearId, tr ("Clear curve"), ! curve.isFlat());
        menu.addSeparator();
        menu.addItem (sliceId, tr ("Slice here (hold S to slice several)"), canSlice);
        menu.addItem (deleteId, targets.size() > 1 ? tr ("Delete selected notes") : tr ("Delete note"));

        juce::Component::SafePointer<NoteArea> safeThis (this);

        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(),
                            [safeThis, id, segment, segmentShape, inRun, clickedRun, syncChecked, targets, presets, sliceAt] (int result)
        {
            auto* self = safeThis.getComponent();

            if (self == nullptr || result <= 0)
                return;

            auto& o = self->owner;
            const auto spb = self->secondsPerBeat();
            const auto range = o.project.bendRange;
            const auto* clicked = editing::findNote (o.project, id);

            auto forTargets = [&] (const juce::String& name, const std::function<void (Note&)>& change)
            {
                o.edit (name, [&] (Project& p)
                {
                    for (auto target : targets)
                        if (auto* n = editing::findNote (p, target))
                            change (*n);
                });
            };

            if (result >= deleteBase)
            {
                const auto& preset = presets[(size_t) (result - deleteBase)];
                o.presetLibrary.remove (preset.name);
                o.flashMessage (trf ("Deleted preset \"{0}\"", preset.name));
                return;
            }

            if (result >= presetBase)
            {
                const auto& preset = presets[(size_t) (result - presetBase)];
                forTargets ("Apply preset", [&] (Note& n) { editing::applyPreset (n.curve, preset.curve, preset.length, n.length); });
                return;
            }

            if (result >= shapeBase)
            {
                static const editing::Shape shapeOrder[] = { editing::Shape::scoop, editing::Shape::overshoot, editing::Shape::bendRelease,
                                                             editing::Shape::fall, editing::Shape::doit };
                const auto shape = shapeOrder[result - shapeBase];
                forTargets ("Apply bend", [&] (Note& n) { editing::applyShape (n.curve, shape, n.length, range); });
                return;
            }

            switch (result)
            {
                case glideId:
                    o.glideSelection();
                    break;

                case sliceId:
                    o.sliceAndSelect (targets, sliceAt);
                    break;

                case deleteId:
                    o.edit ("Delete notes", [&] (Project& p) { editing::deleteNotes (p, targets); });
                    break;

                case vibSegmentId:
                case trillSegmentId:
                {
                    // 只作用于右键的那个音符（"这一段"只对它有意义）。再选一次同样的就是去掉
                    const auto shape = result == trillSegmentId ? Vibrato::Shape::trill : Vibrato::Shape::sine;
                    const auto on = segmentShape != shape;
                    const auto name = shape == Vibrato::Shape::trill ? "Change trill" : "Change vibrato";

                    o.edit (name, [&] (Project& p)
                    {
                        if (auto* n = editing::findNote (p, id))
                            editing::setSegmentVibrato (n->curve, segment, on ? std::optional (shape) : std::nullopt);
                    });
                    break;
                }

                case vibWholeId:
                case trillWholeId:
                {
                    const auto shape = result == trillWholeId ? Vibrato::Shape::trill : Vibrato::Shape::sine;
                    const auto on = clicked == nullptr || ! (clicked->curve.vibrato.wholeNote && clicked->curve.vibrato.shape == shape);
                    const auto name = shape == Vibrato::Shape::trill ? "Change trill" : "Change vibrato";

                    forTargets (name, [&] (Note& n)
                    {
                        editing::setWholeNoteVibrato (n.curve, on ? std::optional (shape) : std::nullopt);
                    });
                    break;
                }

                case syncId:
                {
                    const auto sync = ! syncChecked;

                    if (inRun)
                    {
                        // 右键在某一段颤音上：只改这一段
                        o.edit ("Change vibrato", [&] (Project& p)
                        {
                            if (auto* n = editing::findNote (p, id))
                            {
                                auto v = clickedRun.settings;
                                editing::setVibratoSync (v, sync, spb);
                                editing::setRunVibrato (n->curve, clickedRun, v);
                            }
                        });
                    }
                    else
                    {
                        forTargets ("Change vibrato", [&] (Note& n)
                        {
                            editing::forEachVibrato (n.curve, [&] (Vibrato& v) { editing::setVibratoSync (v, sync, spb); });
                        });
                    }
                    break;
                }

                case savePresetId:
                    if (clicked != nullptr)
                        o.savePreset (clicked->curve, clicked->length);
                    break;

                case removeId: forTargets ("Remove vibrato", [] (Note& n) { editing::removeVibrato (n.curve); }); break;
                case clearId:  forTargets ("Clear curve", [] (Note& n) { n.curve = {}; }); break;
                default: break;
            }
        });
    }

    /** 开始拖动节点、弯曲手柄或颤音控件（调用前已 beginDrag）。n 是 base 里的音符。 */
    void beginHandleDrag (Note& n, CurveHit hit)
    {
        // 往上拉加颤音：先加一段深度为 0 的颤音，接着就是拖它的琥珀线
        if (hit.control == VibratoControl::addLine)
        {
            editing::addVibrato (n.curve, hit.segment, 0.0);
            owner.updateDrag (base);

            const auto runs = n.curve.getVibratoRuns (n.length);
            const auto found = std::find_if (runs.begin(), runs.end(), [&] (const PitchCurve::Run& r) { return r.firstPoint == hit.segment; });

            if (found == runs.end())
                return;

            hit.control = VibratoControl::depthLine;
            hit.vibratoRun = (int) (found - runs.begin());
            beginHandleDrag (n, hit);
            curveEditName = "Add vibrato";
            return;
        }

        if (hit.control != VibratoControl::none)
        {
            const auto runs = n.curve.getVibratoRuns (n.length);

            if (hit.vibratoRun < 0 || hit.vibratoRun >= (int) runs.size())
                return;

            dragRun = runs[(size_t) hit.vibratoRun];
            dragRunIndex = hit.vibratoRun;
            dragEnvelopePoint = hit.envelopePoint;
            mode = hit.control == VibratoControl::depthLine ? Mode::vibratoDepth
                 : hit.control == VibratoControl::envelopePoint ? Mode::envelopePoint : Mode::vibratoSpeed;
            curveEditName = dragRun.settings.shape == Vibrato::Shape::trill ? "Change trill" : "Change vibrato";
            return;
        }

        if (hit.segment >= 0)
        {
            mode = Mode::curveTension;
            curveIndex = hit.segment;
            tensionStart = n.curve.points[(size_t) hit.segment].tension;
            curveEditName = "Change curve shape";
            return;
        }

        if (hit.point >= 0)
        {
            mode = Mode::curvePoint;
            curveIndex = hit.point;
            curveEditName = "Move bend point";
            previewPoint (n);
        }
    }

    /** 试听正在拖的节点的音高（最接近的半音；插件内播放时不试听）。 */
    void previewPoint (const Note& n)
    {
        if (curveIndex < 0 || curveIndex >= (int) n.curve.points.size() || owner.processor.isPlayingInternally())
            return;

        const auto pitch = juce::jlimit (0, 127, n.pitch + (int) std::lround (n.curve.points[(size_t) curveIndex].semitones));

        if (pitch != previewedPitch)
        {
            previewedPitch = pitch;
            owner.preview (pitch, n.velocity);
        }
    }

    /** 在琥珀线上 x 处加一个包络点（改的是 n，也就是 base 里的音符）。返回指向新点的命中；加不了时 control 为 none。 */
    CurveHit addEnvelopePoint (Note& n, const CurveHit& lineHit, float x)
    {
        const auto runs = n.curve.getVibratoRuns (n.length);

        if (lineHit.vibratoRun < 0 || lineHit.vibratoRun >= (int) runs.size())
            return {};

        const auto& run = runs[(size_t) lineHit.vibratoRun];

        if (run.settings.shape != Vibrato::Shape::sine)
            return {};

        auto vib = run.settings;
        const auto u = (owner.view.xToBeat (x) - n.start - run.start) / std::max (1.0e-9, run.length());
        const auto index = editing::insertEnvelopePoint (vib, run.length(), u);

        if (index < 0)
            return {};

        editing::setRunVibrato (n.curve, run, vib);

        auto hit = lineHit;
        hit.control = VibratoControl::envelopePoint;
        hit.envelopePoint = index;
        return hit;
    }

    void deleteEnvelopePoint (const CurveHit& hit)
    {
        owner.edit ("Change vibrato", [&] (Project& p)
        {
            auto* n = editing::findNote (p, hit.id);

            if (n == nullptr)
                return;

            const auto runs = n->curve.getVibratoRuns (n->length);

            if (hit.vibratoRun < 0 || hit.vibratoRun >= (int) runs.size())
                return;

            const auto& run = runs[(size_t) hit.vibratoRun];
            auto vib = run.settings;

            if (editing::removeEnvelopePoint (vib, run.length(), hit.envelopePoint))
                editing::setRunVibrato (n->curve, run, vib);
        });
    }

    void deleteCurvePoint (NoteId id, int index)
    {
        owner.edit ("Delete bend point", [&] (Project& p)
        {
            if (auto* n = editing::findNote (p, id))
                editing::removeCurvePoint (n->curve, index);
        });
    }

    /** 悬停在节点或手柄上时的光标。 */
    static juce::MouseCursor cursorFor (const CurveHit& hit)
    {
        if (hit.point >= 0)            return juce::MouseCursor::DraggingHandCursor;
        if (hit.control == VibratoControl::depthLine)    return juce::MouseCursor::UpDownResizeCursor;
        if (hit.control == VibratoControl::envelopePoint)    return juce::MouseCursor::DraggingHandCursor;
        if (hit.control == VibratoControl::addLine)    return juce::MouseCursor::UpDownResizeCursor;
        if (hit.control == VibratoControl::speedPost)    return juce::MouseCursor::LeftRightResizeCursor;
        if (hit.segment >= 0)          return juce::MouseCursor::UpDownResizeCursor;
        return juce::MouseCursor::NormalCursor;
    }

    void setFocusNote (NoteId id)
    {
        if (id != focusNote)
        {
            focusNote = id;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        switch (mode)
        {
            case Mode::slice:
                finishSlice();
                repaint();
                break;

            case Mode::rightPending:
                // 右键单击：音符上弹出菜单（右键选区里的音符时，菜单作用于整个选区）
                if (pressedNote != 0)
                {
                    if (owner.selection.count (pressedNote) == 0)
                        owner.setSelection ({ pressedNote });

                    showNoteMenu (pressedNote, downBeat);
                }
                break;

            case Mode::shiftPending:
                if (pressedNote != 0)
                {
                    auto sel = owner.selection;
                    if (! sel.erase (pressedNote))
                        sel.insert (pressedNote);
                    owner.setSelection (sel);
                }
                break;

            case Mode::move:
                owner.endDrag (duplicating ? "Duplicate notes" : "Move notes");
                break;

            case Mode::resize:
                if (const auto* n = editing::findNote (owner.project, anchor))
                    owner.defaultLength = n->length; // 下一个新音符沿用这个时长（与 FL 相同）
                owner.endDrag (owner.dragOrigin.notes.size() < owner.project.notes.size() ? "Add note" : "Resize notes");
                break;

            case Mode::trimStart:
                owner.endDrag ("Resize notes");
                break;

            case Mode::erase:
                owner.endDrag ("Delete notes");
                break;

            case Mode::vibratoDepth:
            {
                // 深度拖到 0 后松开 = 去掉这一段颤音，重新显示"往上拉"的线
                auto working = owner.project;

                if (auto* n = editing::findNote (working, focusNote))
                {
                    editing::removeSilentVibrato (n->curve);
                    owner.updateDrag (std::move (working));
                }

                owner.endDrag (curveEditName);
                break;
            }

            case Mode::curvePoint:
            case Mode::curveTension:
            case Mode::envelopePoint:
            case Mode::vibratoSpeed:
                owner.endDrag (curveEditName);
                break;

            case Mode::marquee:
                marquee = {};
                repaint();
                break;

            case Mode::none:
                // 只按了一下节点或手柄，没有拖动
                if (owner.dragging)
                    owner.endDrag (curveEditName);
                break;
        }

        owner.stopPreview();
        previewedPitch = -1;
        mode = Mode::none;
        duplicating = false;
        updateHover (e.position, e.mods);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        owner.lastMouseBeat = owner.view.xToBeat (e.position.x);
        updateHover (e.position, e.mods);
    }

    /** 悬停：决定显示手柄的音符、琥珀线和光标。按下 / 松开 S 时也会调用。 */
    void updateHover (juce::Point<float> pos, juce::ModifierKeys mods)
    {
        if (owner.slicing)
        {
            clearHover();
            setFocusNote (0);

            if (const auto beat = sliceBeatAt (pos.x, mods.isAltDown()); beat != hoverSliceBeat)
            {
                hoverSliceBeat = beat;
                repaint();
            }

            setMouseCursor (juce::MouseCursor::IBeamCursor);
            return;
        }

        if (hoverSliceBeat >= 0.0)
        {
            hoverSliceBeat = -1.0;
            repaint();
        }

        const auto curveHit = editableCurveHit (pos);
        const auto hit = hitTest (pos);
        const auto* current = editing::findNote (owner.project, focusNote);

        // 显示手柄的音符：节点或手柄下的音符 > 鼠标下的音符 > 鼠标正在移向琥珀线的音符 > 唯一选中的音符
        NoteId focus = 0;

        if (curveHit.id != 0)
            focus = curveHit.id;
        else if (hit.id != 0)
            focus = hit.id;
        else if (current != nullptr && nearVibratoLine (*current, pos))
            focus = current->id;

        setFocusNote (focus);

        // 琥珀线跟着鼠标所在的段走（在琥珀线上时保持不变）
        std::optional<VibratoLine> line;

        if (const auto* n = editing::findNote (owner.project, handleNote()))
            line = vibratoLineAt (*n, owner.view.xToBeat (pos.x) - n->start);

        if (line != hoverLine || curveHit != hoverControl)
        {
            hoverLine = line;
            hoverControl = curveHit;
            repaint();
        }

        if (curveHit.id != 0)
            setMouseCursor (cursorFor (curveHit));
        else if (hit.id != 0)
            setMouseCursor (hit.edge != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::CrosshairCursor);

        owner.setHoverHint (hintFor (curveHit, hit));
    }

    /** 底部提示栏：鼠标下的控件怎么用（界面上看不出来的操作）。 */
    static juce::String hintFor (const CurveHit& curveHit, const Hit& hit)
    {
        if (curveHit.id != 0)
        {
            switch (curveHit.control)
            {
                case VibratoControl::depthLine:     return tr ("Drag  Depth (trill: the other note)    Double-click  Add a depth point    Right-click  Vibrato menu");
                case VibratoControl::envelopePoint: return tr ("Drag  Shape the vibrato depth    Right-click  Delete the point");
                case VibratoControl::addLine:       return tr ("Drag up  Add vibrato here");
                case VibratoControl::speedPost:     return tr ("Drag left or right  Vibrato speed");
                case VibratoControl::none:          break;
            }

            if (curveHit.point >= 0)
                return tr ("Drag  Move the point    Right-click  Delete it");

            if (curveHit.segment >= 0)
                return tr ("Drag up or down  Change the curve shape");
        }

        if (hit.id != 0)
        {
            if (hit.edge > 0) return tr ("Drag  Change the length    Alt  No snap");
            if (hit.edge < 0) return tr ("Drag  Change where the note starts    Alt  No snap");
            return tr ("Drag  Move    Double-click  Add bend point    Right-click  Note menu    Alt+wheel  Velocity");
        }

        return {};
    }

    void clearHover()
    {
        if (hoverLine || hoverControl != CurveHit {})
        {
            hoverLine.reset();
            hoverControl = {};
            repaint();
        }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // Alt + 滚轮：调整音符力度（选中了它就调整整个选区）
        if (e.mods.isAltDown())
        {
            if (const auto hit = hitTest (e.position); hit.id != 0)
            {
                const auto ids = owner.selection.count (hit.id) != 0 ? owner.selection : editing::IdSet { hit.id };
                const auto delta = (float) (wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX) * 0.5f;

                owner.edit ("Change velocity", [&] (Project& p)
                {
                    const auto original = p;
                    editing::changeVelocities (p, original, ids, delta);
                });
                return;
            }
        }

        owner.mouseWheelMove (e.getEventRelativeTo (&owner), wheel);
    }

    void eraseAt (juce::Point<float> pos)
    {
        if (const auto hit = hitTest (pos); hit.id != 0)
        {
            auto working = owner.project;
            editing::deleteNotes (working, { hit.id });
            owner.updateDrag (std::move (working));
        }
    }

    PianoRoll& owner;
    Mode mode = Mode::none;
    Project base;               ///< 拖动计算的基准（新建音符时包含新音符）
    NoteId anchor = 0;
    int anchorPitch = -1;
    double downBeat = 0.0;
    int downPitch = 0;
    editing::IdSet marqueeBase;
    juce::Rectangle<float> marquee;

    NoteId pressedNote = 0;     ///< 右键或 Shift 按下的音符（还不知道是单击还是拖动）
    int previewedPitch = -1;    ///< 拖节点时正在试听的音高
    NoteId lastAddedNote = 0;   ///< 最近一次在空白处画出的音符（在空白处双击时不加节点）
    bool duplicating = false;   ///< Shift + 拖动：拖走原来的音符，原处留下副本

    // 节点和手柄
    NoteId focusNote = 0;       ///< 显示手柄的音符（悬停或正在编辑）
    int curveIndex = -1;        ///< 正在拖动的节点 / 段
    float tensionStart = 0.0f;
    juce::String curveEditName;

    // 悬停：加颤音的琥珀线、鼠标下的节点或颤音控件（高亮）
    std::optional<VibratoLine> hoverLine;
    CurveHit hoverControl;

    // 切开（按住 S）
    double sliceBeat = 0.0, hoverSliceBeat = -1.0;
    float sliceY0 = 0.0f, sliceY1 = 0.0f;

    // 拖动颤音手柄
    PitchCurve::Run dragRun;  ///< 正在拖动控件的那一段颤音（拖动开始时的样子）
    int dragRunIndex = -1, dragEnvelopePoint = -1;
};

//==============================================================================
// 力度区：每个音符一根竖条，拖动改变力度
struct PianoRoll::VelocityLane final : public juce::Component
{
    explicit VelocityLane (PianoRoll& o) : owner (o) {}

    float velocityToY (float velocity) const { return (float) getHeight() - 4.0f - velocity * ((float) getHeight() - 10.0f); }
    float yToVelocity (float y) const        { return juce::jlimit (0.01f, 1.0f, ((float) getHeight() - 4.0f - y) / ((float) getHeight() - 10.0f)); }
    float noteX (const Note& n) const        { return (float) owner.view.beatToX (n.start) + 4.0f; }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        const auto w = (float) getWidth(), h = (float) getHeight();

        g.setColour (p.line);
        g.fillRect (0.0f, h - 4.0f, w, 1.0f);

        for (const auto& n : owner.project.notes)
        {
            const auto x = noteX (n);
            if (x < -6.0f || x > w + 6.0f)
                continue;

            const bool selected = owner.selection.count (n.id) != 0;
            const auto colour = selected ? p.warm : p.noteColour (n.velocity);
            const auto y = velocityToY (n.velocity);

            g.setColour (colour.withMultipliedAlpha (0.7f));
            g.fillRect (x - 1.0f, y, 2.0f, h - 4.0f - y);
            g.setColour (colour);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        }
    }

    const Note* noteNear (float x) const
    {
        const Note* best = nullptr;
        float bestDistance = 8.0f;
        bool bestSelected = false;

        for (const auto& n : owner.project.notes)
        {
            const auto d = std::abs (noteX (n) - x);
            const bool selected = owner.selection.count (n.id) != 0;

            // 距离相近时优先选中的音符
            if (d < bestDistance || (d <= bestDistance + 0.5f && selected && ! bestSelected))
            {
                best = &n;
                bestDistance = d;
                bestSelected = selected;
            }
        }

        return best;
    }

    void mouseEnter (const juce::MouseEvent&) override { owner.setHoverHint (tr ("Drag a bar  Change velocity (selected notes change together)")); }
    void mouseExit (const juce::MouseEvent&) override  { owner.setHoverHint ({}); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();
        const auto* n = noteNear (e.position.x);

        if (n == nullptr)
            return;

        grabbed = n->id;
        grabbedVelocity = n->velocity;
        ids = owner.selection.count (grabbed) != 0 ? owner.selection : editing::IdSet { grabbed };
        owner.beginDrag();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (grabbed == 0)
            return;

        auto working = owner.dragOrigin;
        editing::changeVelocities (working, owner.dragOrigin, ids, yToVelocity (e.position.y) - grabbedVelocity);
        owner.updateDrag (std::move (working));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (grabbed != 0)
            owner.endDrag ("Change velocity");

        grabbed = 0;
    }

    PianoRoll& owner;
    NoteId grabbed = 0;
    float grabbedVelocity = 0.0f;
    editing::IdSet ids;
};

//==============================================================================
// 音量、声像栏：每个音符画自己的包络，操作与音高曲线相同：双击加点，拖点移动，右键删点；
// 拖音符的线整条抬高或降低；右键其他地方弹出菜单（淡入、淡出等）。选中的音符一起编辑
struct PianoRoll::EnvelopeLane final : public juce::Component
{
    explicit EnvelopeLane (PianoRoll& o) : owner (o) {}

    bool isVolume() const noexcept { return owner.lane == LaneKind::volume; }

    static Envelope& envelopeOf (Note& n, bool volume)             { return volume ? n.volume : n.pan; }
    static const Envelope& envelopeOf (const Note& n, bool volume) { return volume ? n.volume : n.pan; }

    // 值（分贝 / 声像） ↔ 栏里的高度比例（0 = 底部，1 = 顶部）。音量按推子位置（0 dB 在约 70% 处）；声像：上面是左
    static double toLevel (double value, bool volume)
    {
        return volume ? envelope::faderFromDecibels (value) : (1.0 - std::clamp (value, -1.0, 1.0)) * 0.5;
    }

    static double fromLevel (double level, bool volume)
    {
        level = std::clamp (level, 0.0, 1.0);
        return volume ? envelope::decibelsFromFader (level) : 1.0 - level * 2.0;
    }

    /** 音符 n 的包络在 time（相对音符起点）处的值。 */
    double valueAt (const Note& n, double time) const
    {
        time = juce::jlimit (0.0, n.length, time);
        return isVolume() ? envelope::volumeAt (n.volume, time) : n.pan.evaluate (time);
    }

    float valueToY (double value) const { return 4.0f + (float) (1.0 - toLevel (value, isVolume())) * ((float) getHeight() - 8.0f); }
    double yToValue (float y) const     { return fromLevel (1.0 - (y - 4.0) / std::max (1.0, (double) getHeight() - 8.0), isVolume()); }

    static juce::String formatValue (double value, bool volume)
    {
        if (volume)
            return value <= envelope::volumeMin ? juce::String ("-inf dB") : juce::String (value, 1) + " dB";

        const auto percent = juce::roundToInt (std::abs (value) * 100.0);
        return percent == 0 ? juce::String ("C") : (value < 0.0 ? "L " : "R ") + juce::String (percent) + "%";
    }

    //==============================================================================
    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        const auto& v = owner.view;
        const auto w = (float) getWidth(), h = (float) getHeight();
        const auto volume = isVolume();

        // 参考线：0 dB / 居中
        const float dashes[] = { 3.0f, 4.0f };
        g.setColour (p.lineStrong);
        g.drawDashedLine ({ 0.0f, valueToY (0.0), w, valueToY (0.0) }, dashes, 2, 1.0f);

        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (p.muted.withAlpha (0.8f));

        if (volume)
        {
            g.drawText ("0 dB", juce::Rectangle<float> (w - 40.0f, valueToY (0.0) - 13.0f, 36.0f, 12.0f), juce::Justification::centredRight);
        }
        else
        {
            g.drawText ("L", juce::Rectangle<float> (w - 16.0f, 2.0f, 12.0f, 12.0f), juce::Justification::centredRight);
            g.drawText ("R", juce::Rectangle<float> (w - 16.0f, h - 14.0f, 12.0f, 12.0f), juce::Justification::centredRight);
        }

        for (const auto& n : owner.project.notes)
        {
            const auto x0 = (float) v.beatToX (n.start), x1 = (float) v.beatToX (n.getEnd());

            if (x1 < 0.0f || x0 > w)
                continue;

            const auto selected = owner.selection.count (n.id) != 0;
            const auto focused = n.id == hoverNote || n.id == dragNote;
            const auto& env = envelopeOf (n, volume);
            const auto colour = selected ? p.warm : p.noteColour (n.velocity).withMultipliedAlpha (focused ? 1.0f : 0.7f);

            juce::Path line;
            const auto from = juce::jmax (x0, -2.0f), to = juce::jmin (x1, w + 2.0f);
            auto yAt = [&] (float x) { return valueToY (valueAt (n, v.xToBeat (x) - n.start)); };
            line.startNewSubPath (from, yAt (from));

            for (auto x = from + 3.0f; x < to; x += 3.0f)
                line.lineTo (x, yAt (x));

            line.lineTo (to, yAt (to));
            g.setColour (colour);
            g.strokePath (line, juce::PathStrokeType (focused || selected ? 2.2f : 1.5f));

            // 音符的起止：两端的小竖线
            g.fillRect (x0, yAt (x0) - 3.0f, 1.5f, 6.0f);

            if (! (focused || selected))
                continue;

            for (int i = 0; i < (int) env.points.size(); ++i)
            {
                const auto& pt = env.points[(size_t) i];

                if (pt.time > n.length + 1.0e-9)
                    continue;

                const auto active = n.id == dragNote && i == dragPoint;
                const auto dot = juce::Rectangle<float> (active ? 9.0f : 7.0f, active ? 9.0f : 7.0f)
                                     .withCentre ({ (float) v.beatToX (n.start + pt.time), valueToY (pt.semitones) });
                g.setColour (p.bg);
                g.fillEllipse (dot.expanded (1.5f));
                g.setColour (colour);
                g.fillEllipse (dot);
            }
        }

        // 拖动时显示数值
        if (dragNote != 0 && dragLabel.isNotEmpty())
        {
            g.setFont (juce::FontOptions (11.5f, juce::Font::bold));
            const auto box = juce::Rectangle<float> (70.0f, 16.0f).withCentre (dragLabelAt).constrainedWithin (getLocalBounds().toFloat());
            g.setColour (p.bg.withAlpha (0.9f));
            g.fillRoundedRectangle (box, 5.0f);
            g.setColour (p.text);
            g.drawText (dragLabel, box, juce::Justification::centred, false);
        }

        // MPE 模式下所有音符在同一个实例里，没法分开调音量
        if (owner.processor.isMpeMode())
        {
            g.setColour (p.muted);
            g.setFont (juce::FontOptions (11.5f));
            g.drawFittedText (tr ("Volume and pan curves have no effect in MPE mode."), getLocalBounds().reduced (8, 0),
                              juce::Justification::centredLeft, 1);
        }
    }

    //==============================================================================
    /** 鼠标处的音符：x 落在音符里；优先选中的音符，其次线离鼠标最近的。 */
    const Note* noteAt (juce::Point<float> pos) const
    {
        const auto& v = owner.view;
        const auto beat = v.xToBeat (pos.x);
        const Note* best = nullptr;
        auto bestScore = std::numeric_limits<float>::max();

        for (const auto& n : owner.project.notes)
        {
            if (beat < n.start || beat > n.getEnd())
                continue;

            const auto distance = std::abs (valueToY (valueAt (n, beat - n.start)) - pos.y);
            const auto score = distance - (owner.selection.count (n.id) != 0 ? 1000.0f : 0.0f);

            if (score < bestScore)
            {
                bestScore = score;
                best = &n;
            }
        }

        return best;
    }

    /** pos 处的包络点（先看悬停的音符和选中的音符）。 */
    std::pair<const Note*, int> pointAt (juce::Point<float> pos) const
    {
        const auto& v = owner.view;

        for (const auto& n : owner.project.notes)
        {
            if (n.id != hoverNote && owner.selection.count (n.id) == 0)
                continue;

            const auto& env = envelopeOf (n, isVolume());

            for (int i = (int) env.points.size(); --i >= 0;)
            {
                const auto& pt = env.points[(size_t) i];
                const juce::Point<float> at ((float) v.beatToX (n.start + pt.time), valueToY (pt.semitones));

                if (pt.time <= n.length + 1.0e-9 && at.getDistanceFrom (pos) <= 6.0f)
                    return { &n, i };
            }
        }

        return { nullptr, -1 };
    }

    double timeIn (const Note& n, float x, bool noSnap) const
    {
        auto beat = owner.view.xToBeat (x);

        if (! noSnap)
            beat = editing::snapNearest (beat, owner.grid);

        return juce::jlimit (0.0, n.length, beat - n.start);
    }

    /** 被编辑的音符：按住的音符；它是选中的音符时，选中的全部一起编辑。 */
    editing::IdSet targetsFor (NoteId id) const
    {
        return owner.selection.count (id) != 0 ? owner.selection : editing::IdSet { id };
    }

    /** 把按住的音符的新包络交给其他一起编辑的音符（按长度伸缩）。 */
    void spread (Project& working, const Note& edited) const
    {
        const auto& env = envelopeOf (edited, isVolume());

        for (auto id : dragTargets)
            if (auto* other = editing::findNote (working, id); other != nullptr && id != edited.id)
                envelopeOf (*other, isVolume()) = editing::stretchEnvelope (env, edited.length, other->length);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();
        dragNote = 0;
        dragPoint = -1;
        dragLabel = {};

        const auto [pointNote, point] = pointAt (e.position);
        const auto* note = pointNote != nullptr ? pointNote : noteAt (e.position);

        if (note == nullptr)
            return;

        const auto id = note->id;

        if (e.mods.isPopupMenu())
        {
            if (pointNote != nullptr)
            {
                const auto targets = targetsFor (id);
                const auto volume = isVolume();
                owner.edit (volume ? "Change volume" : "Change pan", [&] (Project& p)
                {
                    if (auto* n = editing::findNote (p, id))
                    {
                        editing::removeCurvePoint (envelopeOf (*n, volume), point);

                        for (auto t : targets)
                            if (auto* other = editing::findNote (p, t); other != nullptr && t != id)
                                envelopeOf (*other, volume) = editing::stretchEnvelope (envelopeOf (*n, volume), n->length, other->length);
                    }
                });
            }
            else
            {
                showMenu (id);
            }
            return;
        }

        dragNote = id;
        dragTargets = targetsFor (id);
        owner.beginDrag();
        base = owner.dragOrigin;

        auto* n = editing::findNote (base, id);
        auto& env = envelopeOf (*n, isVolume());

        if (pointNote != nullptr)
        {
            dragPoint = point;
        }
        else if (e.getNumberOfClicks() >= 2)
        {
            // 双击：在这里加一个点，接着可以拖
            dragPoint = editing::insertCurvePoint (env, timeIn (*n, e.position.x, e.mods.isAltDown()), yToValue (e.position.y));
            auto working = base;
            spread (working, *n);
            base = working;
            owner.updateDrag (base);
        }

        downValue = yToValue (e.position.y);
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragNote == 0)
            return;

        auto working = base;
        auto* n = editing::findNote (working, dragNote);

        if (n == nullptr)
            return;

        const auto volume = isVolume();
        auto& env = envelopeOf (*n, volume);
        const auto value = yToValue (e.position.y);

        if (dragPoint >= 0)
        {
            const auto t = timeIn (*n, e.position.x, e.mods.isAltDown());
            editing::moveCurvePoint (env, dragPoint, t, value, n->length);
            spread (working, *n);
            dragLabel = formatValue (value, volume);
        }
        else
        {
            // 拖线：整条抬高或降低（选中的音符各自移动同样多）
            const auto delta = value - downValue;

            if (e.getDistanceFromDragStart() < 2)
                return;

            for (auto id : dragTargets)
            {
                if (auto* target = editing::findNote (working, id))
                {
                    auto& targetEnv = envelopeOf (*target, volume);

                    if (targetEnv.points.empty())
                        targetEnv.points = { { 0.0, 0.0 } };

                    for (auto& pt : targetEnv.points)
                        pt.semitones += delta;

                    volume ? envelope::normaliseVolume (targetEnv) : envelope::normalisePan (targetEnv);
                }
            }

            dragLabel = formatValue (valueAt (*n, owner.view.xToBeat (e.position.x) - n->start), volume);
        }

        dragLabelAt = e.position.translated (0.0f, -14.0f);
        owner.updateDrag (std::move (working));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragNote != 0)
            owner.endDrag (isVolume() ? "Change volume" : "Change pan");

        dragNote = 0;
        dragPoint = -1;
        dragLabel = {};
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto [pointNote, point] = pointAt (e.position);
        const auto* note = pointNote != nullptr ? pointNote : noteAt (e.position);
        const auto id = note != nullptr ? note->id : 0;

        if (id != hoverNote)
        {
            hoverNote = id;
            repaint();
        }

        setMouseCursor (pointNote != nullptr ? juce::MouseCursor::DraggingHandCursor
                        : note != nullptr ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);

        if (pointNote != nullptr)
            owner.setHoverHint (tr ("Drag  Move the point    Alt  No snap    Right-click  Delete it"));
        else if (note != nullptr)
            owner.setHoverHint (isVolume() ? tr ("Drag  Raise or lower the volume    Double-click  Add a point    Right-click  Fade in, fade out, reset")
                                           : tr ("Drag  Move the pan left or right    Double-click  Add a point    Right-click  Reset"));
        else
            owner.setHoverHint (isVolume() ? tr ("Each note's line here is its volume curve")
                                           : tr ("Each note's line here is its pan curve (top is left)"));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverNote != 0)
        {
            hoverNote = 0;
            repaint();
        }

        owner.setHoverHint ({});
    }

    void showMenu (NoteId id)
    {
        enum { resetId = 1, fadeInId, fadeOutId, swellId };
        const auto volume = isVolume();
        const auto targets = targetsFor (id);

        juce::PopupMenu menu;

        if (volume)
        {
            menu.addItem (fadeInId, tr ("Fade in"));
            menu.addItem (fadeOutId, tr ("Fade out"));
            menu.addItem (swellId, tr ("Swell (louder, then softer)"));
            menu.addSeparator();
        }

        menu.addItem (resetId, volume ? tr ("Reset volume") : tr ("Reset pan"));

        juce::Component::SafePointer<PianoRoll> safeOwner (&owner);
        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [safeOwner, targets, volume] (int result)
        {
            auto* o = safeOwner.getComponent();

            if (o == nullptr || result <= 0)
                return;

            o->edit (volume ? "Change volume" : "Change pan", [&] (Project& p)
            {
                for (auto t : targets)
                {
                    auto* n = editing::findNote (p, t);

                    if (n == nullptr)
                        continue;

                    auto& env = envelopeOf (*n, volume);

                    switch (result)
                    {
                        case fadeInId:  editing::applyVolumeShape (env, editing::VolumeShape::fadeIn, n->length); break;
                        case fadeOutId: editing::applyVolumeShape (env, editing::VolumeShape::fadeOut, n->length); break;
                        case swellId:   editing::applyVolumeShape (env, editing::VolumeShape::swell, n->length); break;
                        default:        env = {}; break;
                    }
                }
            });
        });
    }

    PianoRoll& owner;
    Project base;
    NoteId dragNote = 0, hoverNote = 0;
    int dragPoint = -1;
    double downValue = 0.0;
    editing::IdSet dragTargets;
    juce::String dragLabel;
    juce::Point<float> dragLabelAt;
};

//==============================================================================
// 栏的页签：VEL（力度）、VOL（音量包络）、PAN（声像包络），竖着排在琴键下方
struct PianoRoll::LaneTabs final : public juce::Component,
                                   public juce::SettableTooltipClient
{
    explicit LaneTabs (PianoRoll& o) : owner (o) {}

    static constexpr LaneKind kinds[] = { LaneKind::velocity, LaneKind::volume, LaneKind::pan };
    static constexpr const char* names[] = { "VEL", "VOL", "PAN" };

    juce::Rectangle<float> rowBounds (int i) const
    {
        const auto rowH = (float) getHeight() / 3.0f;
        return { 0.0f, rowH * (float) i, (float) getWidth(), rowH };
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;

        for (int i = 0; i < 3; ++i)
        {
            const auto on = owner.lane == kinds[i];
            g.setColour (on ? p.accent : (i == hovered ? p.text : p.muted));
            g.setFont (juce::FontOptions (11.0f, on ? juce::Font::bold : juce::Font::plain));
            g.drawText (names[i], rowBounds (i).withTrimmedRight (7.0f), juce::Justification::centredRight);
        }
    }

    int rowAt (float y) const { return juce::jlimit (0, 2, (int) (y / juce::jmax (1.0f, (float) getHeight() / 3.0f))); }

    void mouseDown (const juce::MouseEvent& e) override { owner.setLane (kinds[rowAt (e.position.y)]); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        setHovered (rowAt (e.position.y));
        owner.setHoverHint (tr ("Click  Switch between velocity (VEL), volume (VOL) and pan (PAN)"));
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        setHovered (-1);
        owner.setHoverHint ({});
    }

    void setHovered (int row)
    {
        if (row != hovered)
        {
            hovered = row;
            repaint();
        }
    }

    PianoRoll& owner;
    int hovered = -1;
};

//==============================================================================
// 全部操作说明：盖在卷帘上的卡片；单击任意处、Esc、F1 或 ? 关闭
struct PianoRoll::ShortcutsPanel final : public juce::Component
{
    struct Item { const char* keys; const char* action; };
    struct Section { const char* title; std::vector<Item> items; };

    explicit ShortcutsPanel (PianoRoll& o) : owner (o)
    {
        setWantsKeyboardFocus (true);
    }

    static const std::vector<Section>& getSections()
    {
        // 源文件为 UTF-8；箭头等字符用 fromUTF8 转换
        static const std::vector<Section> sections {
            { TRK ("Notes"), {
                { TRK ("Click empty space"), TRK ("Add a note") },
                { TRK ("Drag after clicking"), TRK ("Set the new note's length") },
                { TRK ("Drag a note"), TRK ("Move it (time and pitch)") },
                { TRK ("Drag a note's right end"), TRK ("Change its length") },
                { TRK ("Drag a note's left end"), TRK ("Change where it starts") },
                { TRK ("Shift + drag a note"), TRK ("Duplicate it") },
                { TRK ("Alt + drag"), TRK ("Move or resize without snapping") },
                { TRK ("Right-drag"), TRK ("Delete notes") },
                { TRK ("Hold S, click or drag"), TRK ("Cut a note / cut every note the line crosses") },
                { TRK ("Drag in the VEL lane"), TRK ("Change velocity") },
                { TRK ("Alt + wheel on a note"), TRK ("Change velocity") },
                { TRK ("Click a key"), TRK ("Preview the pitch") },
            } },
            { TRK ("Selection"), {
                { TRK ("Ctrl + drag"), TRK ("Box select") },
                { TRK ("Shift + click"), TRK ("Add to / remove from selection") },
                { TRK ("Ctrl + A"), TRK ("Select all") },
                { TRK ("Esc"), TRK ("Clear selection") },
            } },
            { TRK ("Editing"), {
                { TRK ("Delete"), TRK ("Delete selected notes") },
                { TRK ("Ctrl + C / X / V"), TRK ("Copy / cut / paste at the mouse") },
                { TRK ("Ctrl + B  or  Ctrl + D"), TRK ("Duplicate after the selection") },
                { "\xe2\x86\x91 \xe2\x86\x93", TRK ("Transpose (Shift: octave)") },
                { "\xe2\x86\x90 \xe2\x86\x92", TRK ("Move by the snap step (Shift: bar)") },
                { TRK ("Ctrl + Z / Ctrl + Y"), TRK ("Undo / redo") },
            } },
            { TRK ("Pitch curves"), {
                { TRK ("Double-click a note"), TRK ("Add a bend point (keep dragging)") },
                { TRK ("Drag a point"), TRK ("Move it (snaps to semitones, grid)") },
                { TRK ("Drag a small circle"), TRK ("Change the curve shape") },
                { TRK ("Right-click a point"), TRK ("Delete it") },
                { TRK ("Drag the line above a note"), TRK ("Add vibrato (drag up for more depth)") },
                { TRK ("Drag the amber line"), TRK ("Vibrato depth (trill: the other note)") },
                { TRK ("Double-click the amber line"), TRK ("Add a depth point (right-click deletes it)") },
                { TRK ("Drag the blue handle"), TRK ("Vibrato speed (left slower, right faster)") },
                { TRK ("Right-click a note"), TRK ("Vibrato, trill, bends, presets, slice") },
                { TRK ("G  (2+ notes selected)"), TRK ("Glide: merge a line or chords into legato notes") },
            } },
            { TRK ("Volume and pan"), {
                { TRK ("Click VOL or PAN"), TRK ("Show each note's volume or pan curve") },
                { TRK ("Double-click in the lane"), TRK ("Add a point (right-click deletes it)") },
                { TRK ("Drag a note's line"), TRK ("Raise or lower the whole curve") },
                { TRK ("Right-click in the lane"), TRK ("Fade in, fade out, swell, reset") },
            } },
            { TRK ("Playback"), {
                { TRK ("Space  or  the play button"), TRK ("Play inside Curvable (works while FL is stopped)") },
                { TRK ("Click the ruler"), TRK ("Set where playback starts") },
                { TRK ("Drag in the ruler"), TRK ("Set the loop (drag its ends to change it)") },
                { TRK ("Double-click the ruler"), TRK ("Remove the loop") },
            } },
            { TRK ("View and files"), {
                { TRK ("Wheel"), TRK ("Scroll up / down") },
                { TRK ("Shift + wheel"), TRK ("Scroll left / right") },
                { TRK ("Ctrl + wheel"), TRK ("Zoom horizontally") },
                { TRK ("Ctrl + Shift + wheel"), TRK ("Zoom vertically") },
                { TRK ("Drop a MIDI file"), TRK ("Add its notes (pitch bends become curves)") },
                { TRK ("Drag the MIDI button"), TRK ("Export to FL or another DAW") },
                { TRK ("F1  or  ?"), TRK ("Show this list") },
            } },
        };
        return sections;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        g.fillAll (p.bg.withAlpha (0.82f));

        // 按固定的逻辑尺寸排版，窗口太小时整体缩小
        constexpr float cardW = 760.0f, cardH = 830.0f;
        const auto area = getLocalBounds().toFloat().reduced (12.0f);
        const auto scale = juce::jmin (1.0f, area.getWidth() / cardW, area.getHeight() / cardH);
        const auto card = juce::Rectangle<float> (cardW * scale, cardH * scale).withCentre (area.getCentre());

        neu::raised (g, card, 20.0f * scale, p, 6);

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (scale).translated (card.getX(), card.getY()));

        g.setColour (p.text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawFittedText (tr ("Mouse & keyboard"), 32, 22, 400, 26, juce::Justification::centredLeft, 1);
        g.setColour (p.muted);
        g.setFont (juce::FontOptions (12.5f));
        g.drawFittedText (tr ("Click anywhere or press Esc to close"), (int) cardW - 352, 22, 320, 26, juce::Justification::centredRight, 1);

        // 两栏：左栏 Notes + Selection，右栏 Editing + View & tools
        const auto& sections = getSections();
        constexpr float rowH = 21.0f, colW = 340.0f, keysW = 150.0f;

        auto drawColumn = [&] (float x, std::initializer_list<int> indices)
        {
            float y = 66.0f;

            for (auto i : indices)
            {
                const auto& s = sections[(size_t) i];
                g.setColour (p.accent);
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                g.drawText (tr (s.title).toUpperCase(), juce::Rectangle<float> (x, y, colW, rowH), juce::Justification::centredLeft);
                y += rowH + 2.0f;

                for (const auto& item : s.items)
                {
                    g.setColour (p.text);
                    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                    g.drawFittedText (tr (item.keys), juce::Rectangle<float> (x, y, keysW, rowH).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
                    g.setColour (p.muted);
                    g.setFont (juce::FontOptions (13.0f));
                    g.drawFittedText (tr (item.action), juce::Rectangle<float> (x + keysW + 10.0f, y, colW - keysW - 10.0f, rowH).toNearestInt(),
                                      juce::Justification::centredLeft, 1, 0.75f);
                    y += rowH;
                }

                y += 12.0f;
            }
        };

        drawColumn (32.0f, { 0, 1, 2 });
        drawColumn (32.0f + colW + 24.0f, { 3, 4, 5, 6 });

        g.restoreState();
    }

    void mouseDown (const juce::MouseEvent&) override { owner.showShortcuts (false); }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey || key.getKeyCode() == juce::KeyPress::F1Key || key.getTextCharacter() == '?')
        {
            owner.showShortcuts (false);
            return true;
        }

        return true; // 打开时不把按键交给卷帘，避免误编辑
    }

    PianoRoll& owner;
};

//==============================================================================
// 保存曲线预设时输入名字：盖在卷帘上的小卡片。Enter 保存，Esc 或点卡片外面取消
struct PianoRoll::NamePrompt final : public juce::Component
{
    explicit NamePrompt (PianoRoll& o) : owner (o)
    {
        editor.setFont (juce::FontOptions (15.0f));
        editor.setIndents (10, 7);
        editor.onReturnKey = [this] { confirm(); };
        editor.onEscapeKey = [this] { close(); };
        editor.onTextChange = [this] { repaint(); };
        addAndMakeVisible (editor);

        saveButton.onClick = [this] { confirm(); };
        cancelButton.onClick = [this] { close(); };
        addAndMakeVisible (saveButton);
        addAndMakeVisible (cancelButton);
    }

    void show (PitchCurve c, double length)
    {
        curve = std::move (c);
        noteLength = length;

        // 默认名字：Curve 1、Curve 2……（跳过已有的）
        const auto existing = owner.presetLibrary.load();
        juce::String name;

        for (int i = 1;; ++i)
        {
            name = trf ("Curve {0}", i);

            if (std::none_of (existing.begin(), existing.end(), [&] (const CurvePreset& p) { return p.name == name; }))
                break;
        }

        saveButton.setButtonText (tr ("Save"));
        cancelButton.setButtonText (tr ("Cancel"));
        editor.setColour (juce::TextEditor::textColourId, owner.palette.text);
        editor.setText (name, false);
        editor.selectAll();
        setVisible (true);
        toFront (false);
        editor.grabKeyboardFocus();
        repaint();
    }

    juce::Rectangle<float> getCard() const
    {
        return juce::Rectangle<float> (380.0f, 156.0f).withCentre (getLocalBounds().toFloat().getCentre());
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        g.fillAll (p.bg.withAlpha (0.6f));

        const auto card = getCard();
        neu::raised (g, card, 18.0f, p, 6);

        g.setColour (p.text);
        g.setFont (juce::FontOptions (16.0f, juce::Font::bold));
        g.drawText (tr ("Save curve as preset"), card.reduced (24.0f, 0.0f).withY (card.getY() + 16.0f).withHeight (24.0f),
                    juce::Justification::centredLeft);

        // 同名提示：保存会替换
        const auto name = editor.getText().trim();
        const auto existing = owner.presetLibrary.contains (name);
        g.setColour (existing ? p.warm : p.muted);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (existing ? tr ("A preset with this name will be replaced") : tr ("Saved for all projects (stretches to each note)"),
                    card.reduced (24.0f, 0.0f).withY (card.getY() + 84.0f).withHeight (18.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
    }

    void resized() override
    {
        const auto card = getCard().toNearestInt();
        editor.setBounds (card.getX() + 24, card.getY() + 48, card.getWidth() - 48, 32);
        auto buttons = juce::Rectangle<int> (card.getX() + 24, card.getBottom() - 46, card.getWidth() - 48, 32);
        saveButton.setBounds (buttons.removeFromRight (90));
        buttons.removeFromRight (8);
        cancelButton.setBounds (buttons.removeFromRight (90));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! getCard().contains (e.position))
            close();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
            close();

        return true; // 打开时不把按键交给卷帘
    }

    void confirm()
    {
        const auto name = editor.getText().trim();

        if (name.isEmpty())
        {
            editor.grabKeyboardFocus();
            return;
        }

        const auto replaced = owner.presetLibrary.contains (name);

        if (owner.presetLibrary.save ({ name, curve, noteLength }))
            owner.flashMessage (replaced ? trf ("Replaced preset \"{0}\"", name) : trf ("Saved preset \"{0}\"", name));
        else
            owner.flashMessage (trf ("Could not save the preset ({0})", owner.presetLibrary.getFile().getFullPathName()));

        close();
    }

    void close()
    {
        setVisible (false);

        if (owner.isShowing())
            owner.grabKeyboardFocus();
    }

    PianoRoll& owner;
    juce::TextEditor editor;
    juce::TextButton saveButton { "Save" }, cancelButton { "Cancel" };
    PitchCurve curve;
    double noteLength = 1.0;
};

//==============================================================================
// MIDI 按钮：单击弹出菜单，按住拖出去就是导出（拖到 FL 的 Playlist、其他 DAW 或文件夹）
struct PianoRoll::MidiButton final : public juce::TextButton
{
    explicit MidiButton (PianoRoll& o) : juce::TextButton ("MIDI"), owner (o)
    {
        setWantsKeyboardFocus (false);
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        onClick = [this]
        {
            // 拖出去之后松开鼠标也会触发单击，这时不弹菜单
            if (std::exchange (dragged, false))
                return;

            owner.showMidiMenu();
        };
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = false;
        juce::TextButton::mouseDown (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        juce::TextButton::mouseDrag (e);

        if (! dragged && e.getDistanceFromDragStart() > 6)
        {
            dragged = true;
            owner.dragMidiOut();
        }
    }

    PianoRoll& owner;
    bool dragged = false;
};

//==============================================================================
// 播放、循环、跟随：底部工具栏左边的三个圆形图标按钮
struct PianoRoll::TransportButton final : public juce::Button
{
    enum class Kind { play, loop, follow };

    TransportButton (const Palette& p, Kind k) : juce::Button ("Transport"), palette (p), kind (k)
    {
        setWantsKeyboardFocus (false);
    }

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override
    {
        const auto r = getLocalBounds().toFloat().reduced (4.0f);
        const auto on = getToggleState();

        if (isDown || (on && kind != Kind::play))
            neu::inset (g, r, r.getHeight() / 2.0f, palette, 3);
        else
            neu::raised (g, r, r.getHeight() / 2.0f, palette, 3);

        const auto c = r.getCentre();
        auto colour = on ? (kind == Kind::loop ? palette.warm : palette.accent) : (isHighlighted ? palette.text : palette.muted);
        g.setColour (colour);

        switch (kind)
        {
            case Kind::play:
            {
                if (on)
                {
                    g.fillRoundedRectangle (juce::Rectangle<float> (10.0f, 10.0f).withCentre (c), 2.0f); // 停止
                }
                else
                {
                    juce::Path triangle;
                    triangle.addTriangle (c.x - 4.0f, c.y - 6.0f, c.x - 4.0f, c.y + 6.0f, c.x + 6.5f, c.y);
                    g.fillPath (triangle);
                }
                break;
            }

            case Kind::loop:
            {
                // 两个首尾相接的箭头围成一圈
                juce::Path loop;
                const auto box = juce::Rectangle<float> (16.0f, 10.0f).withCentre (c);
                loop.addRoundedRectangle (box, 5.0f);
                g.strokePath (loop, juce::PathStrokeType (1.7f));
                juce::Path head;
                head.addTriangle (box.getCentreX() + 1.0f, box.getY() - 3.2f, box.getCentreX() + 1.0f, box.getY() + 3.2f,
                                  box.getCentreX() + 5.0f, box.getY());
                g.fillPath (head);
                break;
            }

            case Kind::follow:
            {
                // 播放头竖线 + 向右的箭头
                g.fillRect (c.x - 6.0f, c.y - 7.0f, 1.8f, 14.0f);
                juce::Path arrow;
                arrow.startNewSubPath (c.x - 2.0f, c.y);
                arrow.lineTo (c.x + 6.0f, c.y);
                g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                juce::Path head;
                head.addTriangle (c.x + 3.0f, c.y - 3.5f, c.x + 3.0f, c.y + 3.5f, c.x + 7.5f, c.y);
                g.fillPath (head);
                break;
            }
        }
    }

    const Palette& palette;
    Kind kind;
};

void PianoRoll::savePreset (const PitchCurve& curve, double noteLength)
{
    showShortcuts (false);
    namePrompt->show (curve, noteLength);
}

//==============================================================================
PianoRoll::PianoRoll (CurvableProcessor& p, const Palette& pal)
    : processor (p), palette (pal),
      keyboard (std::make_unique<Keyboard> (*this)),
      ruler (std::make_unique<Ruler> (*this)),
      noteArea (std::make_unique<NoteArea> (*this)),
      velocityLane (std::make_unique<VelocityLane> (*this)),
      envelopeLane (std::make_unique<EnvelopeLane> (*this)),
      laneTabs (std::make_unique<LaneTabs> (*this)),
      shortcutsPanel (std::make_unique<ShortcutsPanel> (*this)),
      namePrompt (std::make_unique<NamePrompt> (*this)),
      midiButton (std::make_unique<MidiButton> (*this)),
      playButton (std::make_unique<TransportButton> (pal, TransportButton::Kind::play)),
      loopButton (std::make_unique<TransportButton> (pal, TransportButton::Kind::loop)),
      followButton (std::make_unique<TransportButton> (pal, TransportButton::Kind::follow)),
      presetLibrary (p.getHostingContext().getSettings().getFile().getSiblingFile (CurvePresetLibrary::fileName))
{
    setWantsKeyboardFocus (true);

    for (auto* c : std::initializer_list<juce::Component*> { keyboard.get(), ruler.get(), noteArea.get(), velocityLane.get(), laneTabs.get() })
        addAndMakeVisible (c);

    addChildComponent (envelopeLane.get());

    for (auto* sb : { &hScroll, &vScroll })
    {
        sb->setAutoHide (false);
        sb->setWantsKeyboardFocus (false);
        sb->addListener (this);
        addAndMakeVisible (sb);
    }

    CurvableLookAndFeel::setStyle (helpButton, "round");
    helpButton.setWantsKeyboardFocus (false);
    helpButton.onClick = [this] { showShortcuts (! shortcutsPanel->isVisible()); };
    addAndMakeVisible (helpButton);

    addChildComponent (shortcutsPanel.get());
    addChildComponent (namePrompt.get());

    CurvableLookAndFeel::setStyle (snapButton, "pill");
    snapButton.setWantsKeyboardFocus (false);
    snapButton.onClick = [this] { showSnapMenu(); };
    addAndMakeVisible (snapButton);

    CurvableLookAndFeel::setStyle (bendButton, "pill");
    bendButton.setWantsKeyboardFocus (false);
    bendButton.onClick = [this] { showBendRangeMenu(); };
    addAndMakeVisible (bendButton);

    CurvableLookAndFeel::setStyle (*midiButton, "pill");
    addAndMakeVisible (*midiButton);

    playButton->onClick = [this] { togglePlayback(); };
    loopButton->onClick = [this] { toggleLoop(); };
    followButton->onClick = [this]
    {
        follow = ! follow;
        processor.getHostingContext().getSettings().setValue ("followPlayhead", follow);
        refreshTransportButtons();
    };

    for (auto* b : { playButton.get(), loopButton.get(), followButton.get() })
        addAndMakeVisible (b);

    follow = processor.getHostingContext().getSettings().getBoolValue ("followPlayhead", true);

    laneTabs->setTooltip (tr ("Below the notes: velocity, or each note's volume or pan curve"));
    updateTexts();
    i18n::addListener (this);



    processor.projectChanged.addChangeListener (this);
    pullProject();
    startTimerHz (30);
}

PianoRoll::~PianoRoll()
{
    i18n::removeListener (this);
    stopTimer();
    stopPreview();
    processor.stopInternally();
    processor.projectChanged.removeChangeListener (this);
}

void PianoRoll::updateTexts()
{
    helpButton.setTooltip (tr ("All mouse and keyboard actions (F1)"));
    snapButton.setButtonText (trf ("Snap  {0}", getGridName()));
    bendButton.setTooltip (tr ("Pitch bend range. Set your synth's bend range to the same value."));
    midiButton->setTooltip (tr ("Drag into FL Studio (or any DAW) to export the notes with their bends. Click for more options."));
    playButton->setTooltip (tr ("Play inside Curvable, even while FL is stopped (Space). Click the ruler to set where it starts."));
    loopButton->setTooltip (tr ("Loop playback. Drag in the ruler to set the loop, double-click the ruler to remove it."));
    followButton->setTooltip (tr ("Follow the playhead"));
    refreshBendButton();
}

void PianoRoll::languageChanged()
{
    updateTexts();
    resized();
    repaint();
}

namespace
{
    /** 按钮文字的宽度（换了语言后按钮跟着变宽，不会把文字挤掉）。 */
    int textWidth (juce::TextButton& b)
    {
        const auto font = b.getLookAndFeel().getTextButtonFont (b, juce::jmax (20, b.getHeight()));
        return juce::GlyphArrangement::getStringWidthInt (font, b.getButtonText());
    }
}

//==============================================================================
juce::Rectangle<int> PianoRoll::getGridBounds() const
{
    return noteArea->getBounds();
}

void PianoRoll::resized()
{
    auto area = getLocalBounds();
    auto bottom = area.removeFromBottom (bottomBarHeight);
    area.removeFromBottom (bottomBarGap);
    panelBounds = area;

    auto inner = panelBounds.reduced (panelPadding);
    auto keysColumn = inner.removeFromLeft (keysWidth);
    inner.removeFromLeft (keysGap);

    // 右侧留出竖直滚动条，标尺与力度区与音符区左右对齐
    auto vScrollColumn = inner.removeFromRight (scrollBarSize);
    inner.removeFromRight (scrollBarGap);

    ruler->setBounds (inner.removeFromTop (rulerHeight));
    velocityLane->setBounds (inner.removeFromBottom (velocityHeight));
    envelopeLane->setBounds (velocityLane->getBounds());
    inner.removeFromBottom (velocityGap);
    hScroll.setBounds (inner.removeFromBottom (scrollBarSize));
    inner.removeFromBottom (scrollBarGap);
    noteArea->setBounds (inner);
    vScroll.setBounds (vScrollColumn.withY (inner.getY()).withHeight (inner.getHeight()));

    keysBounds = keysColumn.withY (inner.getY()).withHeight (inner.getHeight());
    keyboard->setBounds (keysBounds);
    laneTabs->setBounds (keysColumn.getX(), velocityLane->getY(), keysColumn.getWidth(), velocityLane->getHeight());

    // 底部工具栏：左边是播放、循环、跟随（没有工具按钮：所有操作都在同一个编辑模式里，切开是按住 S）
    transportBounds = bottom.removeFromLeft (3 * 44 + 8).withSizeKeepingCentre (3 * 44 + 8, 44);
    {
        auto t = transportBounds;
        playButton->setBounds (t.removeFromLeft (44));
        loopButton->setBounds (t.removeFromLeft (44));
        followButton->setBounds (t.removeFromLeft (44));
    }
    bottom.removeFromLeft (12);
    const auto snapWidth = juce::jmax (124, textWidth (snapButton) + 40);
    const auto bendWidth = juce::jmax (112, textWidth (bendButton) + 40);
    snapButton.setBounds (bottom.removeFromLeft (snapWidth).withSizeKeepingCentre (snapWidth, 44));
    bendButton.setBounds (bottom.removeFromLeft (bendWidth).withSizeKeepingCentre (bendWidth, 44));
    midiButton->setBounds (bottom.removeFromLeft (84).withSizeKeepingCentre (84, 44));
    helpButton.setBounds (bottom.removeFromRight (44).withSizeKeepingCentre (44, 44));

    shortcutsPanel->setBounds (getLocalBounds());
    namePrompt->setBounds (getLocalBounds());

    if (! viewInitialised && noteArea->getHeight() > 0)
    {
        viewInitialised = true;
        centreOnContent();
    }

    clampView();

    // 阴影背景：只在尺寸或配色变化时重画
    const auto scale = juce::Component::getApproximateScaleFactorForComponent (this);
    chrome = juce::Image (juce::Image::ARGB, juce::jmax (1, juce::roundToInt (getWidth() * scale)),
                          juce::jmax (1, juce::roundToInt (getHeight() * scale)), true);
    {
        juce::Graphics g (chrome);
        g.addTransform (juce::AffineTransform::scale (scale));
        neu::inset (g, panelBounds.toFloat(), 20.0f, palette, 5);
        neu::raised (g, keysBounds.toFloat(), 12.0f, palette, 3);
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.drawImage (chrome, getLocalBounds().toFloat());

    // 底部工具栏右侧：状态或操作提示
    auto statusArea = getLocalBounds().removeFromBottom (bottomBarHeight)
                          .withTrimmedLeft (midiButton->getRight() + 12)
                          .withRight (helpButton.getX() - 8);
    const auto warning = getWarning();
    const auto infoFlash = ! flashIsError && warning.isNotEmpty() && warning == flashText;
    g.setColour (statusIsError || (statusText.isEmpty() && warning.isNotEmpty() && ! infoFlash) ? palette.error : palette.muted);
    g.setFont (juce::FontOptions (12.5f));
    const auto dropText = midiDropBeat >= 0.0 ? trf ("Drop to add the notes at bar {0}", (int) (midiDropBeat / 4.0) + 1) : juce::String();
    g.drawFittedText (dropText.isNotEmpty() ? dropText : statusText.isNotEmpty() ? statusText : (warning.isNotEmpty() ? warning : getHint()),
                      statusArea, juce::Justification::centredRight, 1, 0.9f);
}

juce::String PianoRoll::getHint() const
{
    // 只提示界面上看不出来的操作；完整列表在 ? 里
    if (slicing)
        return tr ("Holding S  Click a note to cut it    Drag up or down  Cut several    Alt  No snap");

    if (hoverHint.isNotEmpty())
        return hoverHint;

    if (! selection.empty())
        return tr ("Shift+drag  Duplicate    Shift+click  Add or remove    \xe2\x86\x91\xe2\x86\x93  Transpose    Del  Delete");

    return tr ("Double-click note  Add bend point    Drag line above  Vibrato    Right-click  Menu    Hold S  Slice");
}

juce::String PianoRoll::getWarning() const
{
    if (flashText.isNotEmpty() && juce::Time::getMillisecondCounter() < flashUntil)
        return flashText;

    if (! overflowNotes.empty())
        return trf ("{0} notes (red outline) have to share synth copies: too many different bends at once (limit {1}). While they overlap, they follow the newest one's bend.",
                    (int) overflowNotes.size(), Schedule::maxVoices);

    for (const auto& n : project.notes)
        if (n.curve.getMaxAbsOffset (n.length) > project.bendRange + 1.0e-9)
            return tr ("Some bends go past the bend range. Raise Bend (and your synth's range to match).");

    return {};
}

void PianoRoll::repaintContent()
{
    // 声部分配与发声时相同（Schedule::compile），标出声部不够用的音符
    overflowNotes.clear();

    if (project.notes.size() > (size_t) Schedule::maxVoices)
        for (const auto& n : Schedule::compile (project).notes)
            if (n.overflow)
                overflowNotes.insert (n.id);

    noteArea->repaint();
    velocityLane->repaint();
    envelopeLane->repaint();
    repaint (getLocalBounds().removeFromBottom (bottomBarHeight)); // 提示随选择变化

}

void PianoRoll::showShortcuts (bool shouldShow)
{
    shortcutsPanel->setVisible (shouldShow);

    if (shouldShow)
    {
        shortcutsPanel->toFront (false);
        shortcutsPanel->grabKeyboardFocus();
    }
    else if (isShowing())
    {
        grabKeyboardFocus();
    }
}

void PianoRoll::paletteChanged()
{
    resized(); // 重画阴影背景
    repaint();
}

void PianoRoll::setStatus (const juce::String& text, bool isError)
{
    if (text == statusText && isError == statusIsError)
        return;

    statusText = text;
    statusIsError = isError;
    repaint (getLocalBounds().removeFromBottom (bottomBarHeight));
}

//==============================================================================
void PianoRoll::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // 拖动期间的变化是我们自己造成的，不需要再读一遍
    if (! dragging)
        pullProject();
}

void PianoRoll::pullProject()
{
    project = processor.getProject();
    selection = editing::existingOnly (project, selection);
    refreshBendButton();
    refreshTransportButtons();
    updateScrollBars();
    repaintContent();
}

void PianoRoll::edit (const juce::String& name, const std::function<void (Project&)>& change)
{
    const auto before = project;
    auto working = project;
    change (working);

    project = working;
    processor.setProject (std::move (working), false);
    processor.commitEdit (before, name);

    selection = editing::existingOnly (project, selection);
    repaintContent();
}

void PianoRoll::beginDrag()
{
    dragging = true;
    dragOrigin = project;
}

void PianoRoll::updateDrag (Project working)
{
    project = working;
    processor.setProject (std::move (working), false);
    repaintContent();
}

void PianoRoll::endDrag (const juce::String& name)
{
    dragging = false;
    processor.commitEdit (dragOrigin, name);
    selection = editing::existingOnly (project, selection);
    repaintContent();
}

void PianoRoll::setSelection (editing::IdSet ids)
{
    if (ids == selection)
        return;

    selection = std::move (ids);
    repaintContent();
}

//==============================================================================
void PianoRoll::clampView()
{
    view.clamp (noteArea->getWidth(), noteArea->getHeight());
    updateScrollBars();
    keyboard->repaint();
    ruler->repaint();
    repaintContent();
}

void PianoRoll::updateScrollBars()
{
    // 横向：内容之后再留 4 小节可滚动的空白，当前视图总在范围内
    const auto visibleBeats = view.visibleBeats (juce::jmax (1, noteArea->getWidth()));
    const auto end = juce::jmax (project.getContentEnd() + 16.0, view.startBeat + visibleBeats, 16.0);
    hScroll.setRangeLimits (0.0, end, juce::dontSendNotification);
    hScroll.setCurrentRange (view.startBeat, visibleBeats, juce::dontSendNotification);

    // 纵向：以"从最高音往下数的行"为单位（0 = 音高 127）
    const auto visibleRows = juce::jmax (1, noteArea->getHeight()) / view.rowHeight;
    vScroll.setRangeLimits (0.0, juce::jmax (128.0, visibleRows), juce::dontSendNotification);
    vScroll.setCurrentRange (127.0 - view.topPitch, visibleRows, juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* bar, double newRangeStart)
{
    if (bar == &hScroll)
        view.startBeat = newRangeStart;
    else
        view.topPitch = 127.0 - newRangeStart;

    clampView();
}

void PianoRoll::scrollBy (double beats, double rows)
{
    view.startBeat += beats;
    view.topPitch += rows;
    clampView();
}

void PianoRoll::centreOnContent()
{
    const auto visibleRows = noteArea->getHeight() / view.rowHeight;
    double centre = 66.0;

    if (! project.notes.empty())
    {
        int lowest = 127, highest = 0;
        for (const auto& n : project.notes)
        {
            lowest = juce::jmin (lowest, n.pitch);
            highest = juce::jmax (highest, n.pitch);
        }
        centre = (lowest + highest) / 2.0;
    }

    view.topPitch = std::round (centre + visibleRows / 2.0);
    view.startBeat = 0.0;
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto local = e.getEventRelativeTo (noteArea.get()).position;
    const auto dy = (double) wheel.deltaY, dx = (double) wheel.deltaX;

    if (e.mods.isCommandDown() && e.mods.isShiftDown())
        view.zoomY (std::pow (2.0, dy * 2.0), local.y);
    else if (e.mods.isCommandDown())
        view.zoomX (std::pow (2.0, dy * 2.5), local.x);
    else if (e.mods.isShiftDown())
        view.startBeat -= dy * view.visibleBeats (noteArea->getWidth()) * 0.6;
    else
    {
        // 触控板的横向滑动直接用于左右滚动
        view.startBeat -= dx * view.visibleBeats (noteArea->getWidth()) * 0.6;
        view.topPitch += dy * 30.0;
    }

    clampView();
}

//==============================================================================
void PianoRoll::setSlicing (bool shouldSlice)
{
    if (slicing == shouldSlice)
        return;

    slicing = shouldSlice;

    // 光标、竖线和手柄跟着变（鼠标不动也要更新）
    if (noteArea->isMouseOver())
        noteArea->updateHover (noteArea->getMouseXYRelative().toFloat(), juce::ModifierKeys::getCurrentModifiers());
    else
        noteArea->hoverSliceBeat = -1.0;

    noteArea->repaint();
    repaint (getLocalBounds().removeFromBottom (bottomBarHeight));
}

bool PianoRoll::keyStateChanged (bool)
{
    // 松开 S：回到编辑
    if (slicing && ! juce::KeyPress::isKeyCurrentlyDown ('S'))
        setSlicing (false);

    return false;
}

void PianoRoll::focusLost (FocusChangeType)
{
    setSlicing (false);
}

juce::String PianoRoll::getGridName() const
{
    for (const auto& option : gridOptions)
        if (std::abs (option.beats - grid) < 1.0e-9)
            return tr (option.name);

    return tr ("Custom");
}

void PianoRoll::showSnapMenu()
{
    juce::PopupMenu menu;
    int id = 1;

    for (const auto& option : gridOptions)
        menu.addItem (id++, tr (option.name), true, std::abs (option.beats - grid) < 1.0e-9);

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&snapButton), [safeThis] (int result)
    {
        if (auto* self = safeThis.getComponent(); self != nullptr && result > 0)
        {
            self->grid = gridOptions[result - 1].beats;
            self->snapButton.setButtonText (trf ("Snap  {0}", self->getGridName()));
            self->resized();
            self->noteArea->repaint();
        }
    });
}

void PianoRoll::refreshBendButton()
{
    const auto range = project.bendRange;
    const auto text = std::abs (range - std::round (range)) < 1.0e-9 ? juce::String ((int) std::round (range))
                                                                     : juce::String (range, 1);
    bendButton.setButtonText (trf ("Bend \xc2\xb1{0}", text));
}

void PianoRoll::showBendRangeMenu()
{
    static constexpr int ranges[] = { 1, 2, 3, 4, 5, 7, 12, 24, 36, 48, 96 };

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("Pitch bend range (semitones)"));
    menu.addSectionHeader (tr ("Set your synth to the same value"));

    for (int i = 0; i < (int) std::size (ranges); ++i)
        menu.addItem (i + 1, juce::String::fromUTF8 ("\xc2\xb1") + juce::String (ranges[i]),
                      true, std::abs (project.bendRange - ranges[i]) < 1.0e-9);

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bendButton), [safeThis] (int result)
    {
        if (auto* self = safeThis.getComponent(); self != nullptr && result > 0)
        {
            auto p = self->processor.getProject();
            p.bendRange = ranges[result - 1];
            self->processor.setProject (std::move (p));
        }
    });
}

//==============================================================================
void PianoRoll::deleteSelection()
{
    if (selection.empty())
        return;

    const auto ids = selection;
    edit ("Delete notes", [&] (Project& p) { editing::deleteNotes (p, ids); });
}

void PianoRoll::copySelection (bool cut)
{
    if (selection.empty())
        return;

    clipboard = editing::copyNotes (project, selection);

    if (cut)
        deleteSelection();
}

void PianoRoll::paste()
{
    if (clipboard.empty())
        return;

    const auto at = lastMouseBeat >= 0.0 ? editing::snapFloor (lastMouseBeat, grid) : 0.0;
    editing::IdSet added;
    edit ("Paste", [&] (Project& p) { added = editing::pasteNotes (p, clipboard, at); });
    setSelection (std::move (added));
}

void PianoRoll::duplicateSelection()
{
    if (selection.empty())
        return;

    const auto ids = selection;
    editing::IdSet added;
    edit ("Duplicate", [&] (Project& p) { added = editing::duplicateNotes (p, ids, grid > 0.0 ? grid : 1.0); });
    setSelection (std::move (added));
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const auto code = key.getKeyCode();
    const auto cmd = mods.isCommandDown();
    const auto ids = selection;

    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)
    {
        deleteSelection();
        return true;
    }

    // Space：插件内播放（设置里可以交给 FL）
    if (code == juce::KeyPress::spaceKey && ! mods.isAnyModifierKeyDown())
    {
        if (processor.getHostingContext().getSettings().getBoolValue (spaceToHostKey, false))
            return false;

        togglePlayback();
        return true;
    }

    if (cmd && (code == 'Z' || code == 'z'))
    {
        if (mods.isShiftDown())
            processor.getUndoManager().redo();
        else
            processor.getUndoManager().undo();
        return true;
    }

    if (cmd && (code == 'Y' || code == 'y'))   { processor.getUndoManager().redo(); return true; }
    if (cmd && (code == 'A' || code == 'a'))   { setSelection (editing::allNotes (project)); return true; }
    if (cmd && (code == 'C' || code == 'c'))   { copySelection (false); return true; }
    if (cmd && (code == 'X' || code == 'x'))   { copySelection (true); return true; }
    if (cmd && (code == 'V' || code == 'v'))   { paste(); return true; }
    if (cmd && (code == 'B' || code == 'b' || code == 'D' || code == 'd')) { duplicateSelection(); return true; }

    if (! cmd && (code == 'S' || code == 's')) { setSlicing (true); return true; } // 按住：切开
    if (! cmd && (code == 'G' || code == 'g')) { glideSelection(); return true; }
    if (code == juce::KeyPress::escapeKey)     { setSelection ({}); return true; }
    if (code == juce::KeyPress::F1Key || key.getTextCharacter() == '?') { showShortcuts (true); return true; }

    if (! ids.empty() && (code == juce::KeyPress::upKey || code == juce::KeyPress::downKey))
    {
        const auto semis = (code == juce::KeyPress::upKey ? 1 : -1) * (mods.isShiftDown() ? 12 : 1);
        edit ("Transpose", [&] (Project& p) { editing::transposeNotes (p, ids, semis); });
        return true;
    }

    if (! ids.empty() && (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey))
    {
        const auto step = mods.isShiftDown() ? 4.0 : (grid > 0.0 ? grid : 1.0);
        const auto delta = code == juce::KeyPress::rightKey ? step : -step;
        edit ("Move notes", [&] (Project& p) { editing::shiftNotes (p, ids, delta); });
        return true;
    }

    return false;
}

//==============================================================================
void PianoRoll::preview (int pitch, float velocity)
{
    stopPreview();
    previewPitch = pitch;
    processor.previewNote (pitch, juce::jmax (1, juce::roundToInt (velocity * 127.0f)));
}

void PianoRoll::stopPreview()
{
    if (previewPitch >= 0)
        processor.previewNote (previewPitch, 0);

    previewPitch = -1;
}

void PianoRoll::setHoverHint (const juce::String& text)
{
    if (text != hoverHint)
    {
        hoverHint = text;
        repaint (getLocalBounds().removeFromBottom (bottomBarHeight));
    }
}

void PianoRoll::flashMessage (const juce::String& text, bool isError)
{
    flashText = text;
    flashIsError = isError;
    flashUntil = juce::Time::getMillisecondCounter() + 4000;
    repaint (getLocalBounds().removeFromBottom (bottomBarHeight));
}

void PianoRoll::setLane (LaneKind kind)
{
    lane = kind;
    velocityLane->setVisible (lane == LaneKind::velocity);
    envelopeLane->setVisible (lane != LaneKind::velocity);
    laneTabs->repaint();
    envelopeLane->repaint();
}

void PianoRoll::togglePlayback()
{
    if (processor.isPlayingInternally())
        stopPlayback();
    else
        startPlayback();
}

void PianoRoll::startPlayback()
{
    // AUTO 模式下 FL 正在播放：由 FL 带着走，插件内播放不起作用
    if (processor.isHostPlaying() && project.mode == PlaybackMode::automatic)
    {
        flashMessage (tr ("FL Studio is playing. Stop it to play inside Curvable."));
        return;
    }

    stopPreview();

    const auto loop = project.loopOn && project.hasLoop();
    processor.playInternally (startBeat, loop ? project.loopStart : 0.0, loop ? project.loopEnd : 0.0);

    if (! loop && startBeat >= roundUpToBar (project.getContentEnd()) - 1.0e-6)
        flashMessage (tr ("Nothing to play after the start marker. Click the ruler to move it."));

    refreshTransportButtons();
}

void PianoRoll::stopPlayback()
{
    processor.stopInternally();
    refreshTransportButtons();
}

void PianoRoll::setStartBeat (double beat)
{
    startBeat = juce::jmax (0.0, beat);

    // 播放中单击标尺：从那里接着播
    if (processor.isPlayingInternally())
        startPlayback();

    ruler->repaint();
}

void PianoRoll::setLoop (double start, double end, bool on)
{
    auto p = processor.getProject();

    if (end > start)
    {
        p.loopStart = juce::jmax (0.0, start);
        p.loopEnd = end;
        p.loopOn = on;
    }
    else
    {
        p.loopStart = p.loopEnd = 0.0;
        p.loopOn = false;
    }

    project.loopStart = p.loopStart;
    project.loopEnd = p.loopEnd;
    project.loopOn = p.loopOn;
    processor.setProject (std::move (p));

    // 正在插件内播放：按新的循环区从当前位置接着播
    if (processor.isPlayingInternally())
    {
        const auto at = processor.getPlayheadBeat();
        const auto loop = project.loopOn && project.hasLoop();
        processor.playInternally (at >= 0.0 ? at : startBeat, loop ? project.loopStart : 0.0, loop ? project.loopEnd : 0.0);
    }

    refreshTransportButtons();
    ruler->repaint();
    noteArea->repaint();
}

void PianoRoll::toggleLoop()
{
    // 有选中的音符：循环它们所在的小节；否则开关现有的循环区；都没有时循环整段内容
    if (! selection.empty())
    {
        auto from = std::numeric_limits<double>::max(), to = 0.0;

        for (const auto& n : project.notes)
            if (selection.count (n.id) != 0)
            {
                from = std::min (from, n.start);
                to = std::max (to, n.getEnd());
            }

        from = std::floor (from / 4.0 + 1.0e-9) * 4.0;
        setLoop (from, juce::jmax (from + 4.0, roundUpToBar (to)), true);
        return;
    }

    if (project.hasLoop())
    {
        setLoop (project.loopStart, project.loopEnd, ! project.loopOn);
        return;
    }

    setLoop (0.0, juce::jmax (4.0, roundUpToBar (project.getContentEnd())), true);
}

void PianoRoll::refreshTransportButtons()
{
    playButton->setToggleState (processor.isPlayingInternally(), juce::dontSendNotification);
    loopButton->setToggleState (project.loopOn && project.hasLoop(), juce::dontSendNotification);
    followButton->setToggleState (follow, juce::dontSendNotification);
}

void PianoRoll::followPlayhead (double beat)
{
    if (! follow || beat < 0.0 || dragging)
        return;

    const auto visible = view.visibleBeats (juce::jmax (1, noteArea->getWidth()));

    // 播放头走出视野（或到了右边缘）：翻一页，播放头放在左边一点
    if (beat < view.startBeat || beat > view.startBeat + visible * 0.95)
    {
        view.startBeat = juce::jmax (0.0, beat - visible * 0.05);
        clampView();
    }
}

void PianoRoll::sliceAndSelect (const editing::IdSet& ids, double atBeat)
{
    editing::IdSet added;
    edit ("Slice notes", [&] (Project& p) { added = editing::sliceNotes (p, ids, atBeat); });

    // 选中切开后的两半，方便接着处理
    auto sel = ids;
    sel.insert (added.begin(), added.end());
    setSelection (editing::existingOnly (project, sel));
}

void PianoRoll::glideSelection()
{
    if (selection.size() < 2)
    {
        flashMessage (tr ("Select two or more notes or chords to glide between (Shift+click or Ctrl+drag)."));
        return;
    }

    // 先在副本上试一次，不能合并时不留下撤销记录
    auto trial = project;
    const auto result = editing::glideChords (trial, selection);

    using Error = editing::ChordGlideResult::Error;

    if (result.error == Error::oneChordOnly)
    {
        flashMessage (tr ("All selected notes start together. Select notes or chords at two or more different times."));
        return;
    }

    if (result.error == Error::chordTooLarge)
    {
        flashMessage (tr ("Can't glide chords with more than 12 notes."));
        return;
    }

    if (result.error != Error::none)
        return;

    const auto ids = selection;
    edit ("Glide notes", [&] (Project& p) { editing::glideChords (p, ids); });
    setSelection (editing::existingOnly (project, result.ids));

    // 偏移超出弯音范围时提醒（之后底部会一直显示，直到解决）
    double needed = 0.0;
    for (auto id : result.ids)
        if (const auto* n = editing::findNote (project, id))
            needed = std::max (needed, n->curve.getMaxAbsOffset (n->length));

    if (needed > project.bendRange + 1.0e-9)
        flashMessage (trf ("This glide needs Bend \xc2\xb1{0} or more. Raise Bend (and your synth's range to match).",
                           (int) std::ceil (needed - 1.0e-9)));
}

void PianoRoll::timerCallback()
{
    // 提醒到期后恢复原来的提示
    if (flashText.isNotEmpty() && juce::Time::getMillisecondCounter() >= flashUntil)
    {
        flashText = {};
        repaint (getLocalBounds().removeFromBottom (bottomBarHeight));
    }

    if (playButton->getToggleState() != processor.isPlayingInternally())
        refreshTransportButtons();

    const auto beat = processor.getPlayheadBeat();

    if (std::abs (beat - playheadBeat) < 1.0e-6)
        return;

    // 播放刚停：标尺上重新显示起点
    if ((beat < 0.0) != (playheadBeat < 0.0))
        ruler->repaint();

    followPlayhead (beat);

    // 只重画播放头经过的窄条
    auto strip = [this] (double b, juce::Component& c)
    {
        if (b >= 0.0)
            c.repaint (juce::roundToInt (view.beatToX (b)) - 7, 0, 14, c.getHeight());
    };

    strip (playheadBeat, *noteArea);
    strip (playheadBeat, *ruler);
    playheadBeat = beat;
    strip (playheadBeat, *noteArea);
    strip (playheadBeat, *ruler);
}

//==============================================================================
// MIDI 导入导出
namespace
{
    constexpr auto dragLayoutKey = "midiDragLayout";

    juce::File midiTempFolder()
    {
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Curvable MIDI");
    }
}

double PianoRoll::dropBeat (int x) const
{
    const auto beat = view.xToBeat ((double) (x - noteArea->getX()));
    return juce::jmax (0.0, std::floor (beat / 4.0) * 4.0);
}

bool PianoRoll::isInterestedInFileDrag (const juce::StringArray& files)
{
    return std::any_of (files.begin(), files.end(), [] (const juce::String& f) { return midi::isMidiFile (juce::File (f)); });
}

void PianoRoll::fileDragEnter (const juce::StringArray&, int x, int) { fileDragMove ({}, x, 0); }

void PianoRoll::fileDragMove (const juce::StringArray&, int x, int)
{
    if (const auto beat = dropBeat (x); beat != midiDropBeat)
    {
        midiDropBeat = beat;
        repaint();
    }
}

void PianoRoll::fileDragExit (const juce::StringArray&)
{
    midiDropBeat = -1.0;
    repaint();
}

void PianoRoll::filesDropped (const juce::StringArray& files, int x, int)
{
    midiDropBeat = -1.0;
    repaint();

    for (const auto& f : files)
    {
        if (midi::isMidiFile (juce::File (f)))
        {
            importMidiFile (juce::File (f), dropBeat (x));
            return;
        }
    }
}

void PianoRoll::paintOverChildren (juce::Graphics& g)
{
    if (midiDropBeat < 0.0)
        return;

    // 拖入 MIDI 时：标出音符会从哪里开始
    const auto area = noteArea->getBounds().toFloat();
    const auto x = area.getX() + (float) view.beatToX (midiDropBeat);

    g.setColour (palette.accent.withAlpha (0.08f));
    g.fillRect (area.withLeft (juce::jlimit (area.getX(), area.getRight(), x)));
    g.setColour (palette.accent);
    g.fillRect (juce::Rectangle<float> (x - 1.0f, area.getY(), 2.0f, area.getHeight()).getIntersection (area));
}

void PianoRoll::importMidiFile (const juce::File& file, double atBeat)
{
    const auto imported = midi::importMidi (file, project.bendRange);

    if (imported.error.isNotEmpty())
    {
        flashMessage (imported.error);
        return;
    }

    editing::IdSet added;

    edit ("Import MIDI", [&] (Project& p)
    {
        for (const auto& n : imported.notes)
        {
            const auto id = p.addNote (n.start + atBeat, n.length, n.pitch, n.velocity);
            editing::findNote (p, id)->curve = n.curve;
            added.insert (id);
        }
    });

    setSelection (added);

    if (imported.bentNotes > 0)
        flashMessage (trf ("Added {0} notes from {1} ({2} with pitch bends)", (int) imported.notes.size(), file.getFileName(), imported.bentNotes));
    else
        flashMessage (trf ("Added {0} notes from {1}", (int) imported.notes.size(), file.getFileName()));
}

void PianoRoll::browseForMidiImport()
{
    fileChooser = std::make_unique<juce::FileChooser> (tr ("Add notes from a MIDI file"),
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mid;*.midi");
    juce::Component::SafePointer<PianoRoll> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
    {
        auto* self = safeThis.getComponent();

        if (self == nullptr || chooser.getResult() == juce::File())
            return;

        // 加在内容的后面（空工程从头开始）
        self->importMidiFile (chooser.getResult(), roundUpToBar (self->project.getContentEnd()));
    });
}

midi::Layout PianoRoll::getDragLayout() const
{
    return processor.getHostingContext().getSettings().getValue (dragLayoutKey) == "mpe" ? midi::Layout::mpe
                                                                                         : midi::Layout::channelPerVoice;
}

juce::String PianoRoll::describeExport (const midi::ExportResult& r, midi::Layout layout)
{
    juce::StringArray problems;

    if (r.sharedNotes > 0)
        problems.add (trf ("{0} notes had to share a channel with a different bend", r.sharedNotes));

    if (r.clippedNotes > 0)
        problems.add (layout == midi::Layout::mpe ? trf ("{0} notes bend further than \xc2\xb1" "48", r.clippedNotes)
                                                  : trf ("{0} notes bend further than the bend range", r.clippedNotes));

    return problems.joinIntoString (tr (", "));
}

void PianoRoll::dragMidiOut()
{
    if (project.notes.empty())
    {
        flashMessage (tr ("There are no notes to export yet."));
        return;
    }

    const auto layout = getDragLayout();
    const auto result = midi::exportProject (project, layout, processor.getHostBpm());

    // 旧的临时文件：一天以后清理（宿主可能还在读刚拖出去的文件）
    auto folder = midiTempFolder();
    folder.createDirectory();

    for (const auto& old : folder.findChildFiles (juce::File::findFiles, false, "*.mid"))
        if (old.getLastModificationTime() < juce::Time::getCurrentTime() - juce::RelativeTime::days (1))
            old.deleteFile();

    // 文件名就是宿主里显示的名字（FL 用它命名 pattern）；被占用时换一个
    auto file = folder.getChildFile (layout == midi::Layout::mpe ? "Curvable MPE.mid" : "Curvable.mid");

    if (file.exists() && ! file.deleteFile())
        file = file.getNonexistentSibling();

    juce::FileOutputStream out (file);

    if (! out.openedOk() || ! result.file.writeTo (out))
    {
        flashMessage (trf ("Could not write the MIDI file ({0})", file.getFullPathName()));
        return;
    }

    out.flush();

    if (const auto problems = describeExport (result, layout); problems.isNotEmpty())
        flashMessage (trf ("Exported, but {0}.", problems));

    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, midiButton.get());
}

void PianoRoll::saveMidiAs (midi::Layout layout)
{
    if (project.notes.empty())
    {
        flashMessage (tr ("There are no notes to export yet."));
        return;
    }

    const auto name = layout == midi::Layout::mpe ? "Curvable MPE.mid" : "Curvable.mid";
    fileChooser = std::make_unique<juce::FileChooser> (tr ("Save MIDI file"),
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name),
                                                       "*.mid");
    juce::Component::SafePointer<PianoRoll> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis, layout] (const juce::FileChooser& chooser)
    {
        auto* self = safeThis.getComponent();
        auto file = chooser.getResult();

        if (self == nullptr || file == juce::File())
            return;

        if (! file.hasFileExtension ("mid;midi"))
            file = file.withFileExtension ("mid");

        const auto result = midi::exportProject (self->project, layout, self->processor.getHostBpm());
        file.deleteFile();
        juce::FileOutputStream out (file);

        if (! out.openedOk() || ! result.file.writeTo (out))
        {
            self->flashMessage (trf ("Could not write {0}", file.getFullPathName()));
            return;
        }

        const auto problems = describeExport (result, layout);
        self->flashMessage (problems.isNotEmpty() ? trf ("Saved {0} ({1})", file.getFileName(), problems) : trf ("Saved {0}", file.getFileName()));
    });
}

void PianoRoll::showMidiMenu()
{
    const auto dragLayout = getDragLayout();
    enum { importId = 1, saveVoicesId, saveMpeId, dragVoicesId, dragMpeId };

    juce::PopupMenu menu;
    menu.addSectionHeader (tr ("Drag this button into your DAW to export"));
    menu.addItem (dragVoicesId, tr ("Drag exports one channel per voice"), true, dragLayout == midi::Layout::channelPerVoice);
    menu.addItem (dragMpeId, tr ("Drag exports MPE"), true, dragLayout == midi::Layout::mpe);
    menu.addSeparator();
    menu.addItem (saveVoicesId, tr ("Save MIDI file (one channel per voice)..."), ! project.notes.empty());
    menu.addItem (saveMpeId, tr ("Save MPE MIDI file..."), ! project.notes.empty());
    menu.addSeparator();
    menu.addItem (importId, tr ("Add notes from a MIDI file..."));

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (midiButton.get()), [safeThis] (int result)
    {
        auto* self = safeThis.getComponent();

        if (self == nullptr || result <= 0)
            return;

        auto& settings = self->processor.getHostingContext().getSettings();

        switch (result)
        {
            case importId:     self->browseForMidiImport(); break;
            case saveVoicesId: self->saveMidiAs (midi::Layout::channelPerVoice); break;
            case saveMpeId:    self->saveMidiAs (midi::Layout::mpe); break;
            case dragVoicesId: settings.setValue (dragLayoutKey, "voices"); break;
            case dragMpeId:    settings.setValue (dragLayoutKey, "mpe"); break;
            default: break;
        }
    });
}

} // namespace curvable
