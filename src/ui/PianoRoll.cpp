#include "ui/PianoRoll.h"
#include "plugin/PluginProcessor.h"
#include "ui/CurvableLookAndFeel.h"

namespace curvable
{

namespace
{
    constexpr int bottomBarHeight = 48;
    constexpr int bottomBarGap    = 14;
    constexpr int panelPadding    = 10;
    constexpr int keysWidth       = 60;
    constexpr int keysGap         = 10;
    constexpr int rulerHeight     = 28;
    constexpr int velocityHeight  = 56;
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
        { "Bar", 4.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
        { "1/8 triplet", 1.0 / 3.0 }, { "1/16 triplet", 1.0 / 6.0 }, { "None", 0.0 },
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

        if (owner.playheadBeat >= 0.0)
        {
            const auto x = (float) v.beatToX (owner.playheadBeat);
            juce::Path marker;
            marker.addTriangle (x - 5.0f, h - 9.0f, x + 5.0f, h - 9.0f, x, h - 2.0f);
            g.setColour (p.accent);
            g.fillPath (marker);
        }
    }

    PianoRoll& owner;
};

//==============================================================================
// 音符编辑区
struct PianoRoll::NoteArea final : public juce::Component
{
    explicit NoteArea (PianoRoll& o) : owner (o) {}

    enum class Mode { none, move, resize, marquee, erase };

    struct Hit
    {
        NoteId id = 0;
        bool edge = false;
    };

    juce::Rectangle<float> noteBounds (const Note& n) const
    {
        const auto& v = owner.view;
        const auto x0 = (float) v.beatToX (n.start), x1 = (float) v.beatToX (n.getEnd());
        const auto y = (float) v.pitchToY (n.pitch);
        const auto rh = (float) v.rowHeight;
        const auto pad = juce::jlimit (1.0f, 2.5f, rh * 0.14f);
        return { x0 + 1.0f, y + pad, juce::jmax (3.0f, x1 - x0 - 2.0f), rh - pad * 2.0f };
    }

    Hit hitTest (juce::Point<float> pos) const
    {
        const auto& notes = owner.project.notes;
        const auto rh = (float) owner.view.rowHeight;

        // 后画的在上面，所以倒序查找
        for (auto it = notes.rbegin(); it != notes.rend(); ++it)
        {
            const auto r = noteBounds (*it);
            const auto rowY = (float) owner.view.pitchToY (it->pitch);

            if (pos.x >= r.getX() && pos.x <= r.getRight() && pos.y >= rowY && pos.y < rowY + rh)
            {
                const auto edgeWidth = juce::jlimit (3.0f, 8.0f, r.getWidth() * 0.3f);
                return { it->id, pos.x >= r.getRight() - edgeWidth };
            }
        }

        return {};
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

        // 音符：凸起的胶囊，颜色表示力度；选中的加暖色描边
        for (const auto& n : owner.project.notes)
        {
            const auto r = noteBounds (n);

            if (r.getRight() < 0.0f || r.getX() > w || r.getBottom() < 0.0f || r.getY() > h)
                continue;

            const auto radius = r.getHeight() / 2.0f;
            juce::Path path;
            path.addRoundedRectangle (r, radius);

            const auto colour = p.noteColour (n.velocity);
            neu::raisedFast (g, path, colour, p, 1.2f);

            if (owner.selection.count (n.id) != 0)
            {
                g.setColour (p.warm);
                g.drawRoundedRectangle (r.expanded (1.0f), radius + 1.0f, 2.0f);
            }

            if (r.getWidth() > 34.0f && r.getHeight() >= 10.0f)
            {
                g.setColour (p.textOn (colour).withAlpha (0.8f));
                g.setFont (juce::FontOptions (juce::jmin (11.0f, r.getHeight() - 1.0f)));
                g.drawText (noteName (n.pitch), r.withTrimmedLeft (radius * 0.8f), juce::Justification::centredLeft, false);
            }
        }

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

    //==============================================================================
    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.grabKeyboardFocus();

        const auto& v = owner.view;
        downBeat = v.xToBeat (e.position.x);
        downPitch = v.yToPitch (e.position.y);
        const auto hit = hitTest (e.position);

        // 右键：删除（可拖过多个音符）
        if (e.mods.isPopupMenu())
        {
            mode = Mode::erase;
            owner.beginDrag();
            eraseAt (e.position);
            return;
        }

        if (hit.id != 0)
        {
            if (e.mods.isShiftDown())
            {
                auto sel = owner.selection;
                if (! sel.erase (hit.id))
                    sel.insert (hit.id);
                owner.setSelection (sel);
                mode = Mode::none;
                return;
            }

            if (owner.selection.count (hit.id) == 0)
                owner.setSelection ({ hit.id });

            owner.beginDrag();
            base = owner.dragOrigin;
            anchor = hit.id;
            mode = hit.edge ? Mode::resize : Mode::move;

            if (const auto* n = editing::findNote (base, anchor))
            {
                anchorPitch = n->pitch;
                owner.preview (n->pitch, n->velocity);
            }
            return;
        }

        // 空白处：Select 工具或按住 Ctrl → 框选；Draw 工具 → 添加音符
        if (owner.tool == Tool::select || e.mods.isCommandDown())
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

        owner.updateDrag (base);
        owner.setSelection ({ anchor });
        owner.preview (pitch, defaultVelocity);
        mode = Mode::resize;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto& v = owner.view;
        const auto beat = v.xToBeat (e.position.x);
        const auto pitch = v.yToPitch (e.position.y);
        const bool noSnap = e.mods.isAltDown();
        owner.lastMouseBeat = beat;

        switch (mode)
        {
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

            case Mode::none:
                break;
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        switch (mode)
        {
            case Mode::move:
                owner.endDrag ("Move notes");
                break;

            case Mode::resize:
                if (const auto* n = editing::findNote (owner.project, anchor))
                    owner.defaultLength = n->length; // 下一个新音符沿用这个时长（与 FL 相同）
                owner.endDrag (owner.dragOrigin.notes.size() < owner.project.notes.size() ? "Add note" : "Resize notes");
                break;

            case Mode::erase:
                owner.endDrag ("Delete notes");
                break;

            case Mode::marquee:
                marquee = {};
                repaint();
                break;

            case Mode::none:
                break;
        }

        owner.stopPreview();
        mode = Mode::none;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        owner.lastMouseBeat = owner.view.xToBeat (e.position.x);
        const auto hit = hitTest (e.position);

        if (hit.id != 0)
            setMouseCursor (hit.edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor);
        else
            setMouseCursor (owner.tool == Tool::draw ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
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
            { "Notes", {
                { "Click empty space", "Add a note (Draw tool)" },
                { "Drag after clicking", "Set the new note's length" },
                { "Drag a note", "Move it (time and pitch)" },
                { "Drag a note's right end", "Change its length" },
                { "Alt + drag", "Move or resize without snapping" },
                { "Right-click / right-drag", "Delete notes" },
                { "Drag in the VEL lane", "Change velocity" },
                { "Alt + wheel on a note", "Change velocity" },
                { "Click a key", "Preview the pitch" },
            } },
            { "Selection", {
                { "Drag (Select tool)", "Box select" },
                { "Ctrl + drag", "Box select with any tool" },
                { "Shift + click", "Add to / remove from selection" },
                { "Ctrl + A", "Select all" },
                { "Esc", "Clear selection" },
            } },
            { "Editing", {
                { "Delete", "Delete selected notes" },
                { "Ctrl + C / X / V", "Copy / cut / paste at the mouse" },
                { "Ctrl + B  or  Ctrl + D", "Duplicate after the selection" },
                { "\xe2\x86\x91 \xe2\x86\x93", "Transpose (Shift: octave)" },
                { "\xe2\x86\x90 \xe2\x86\x92", "Move by the snap step (Shift: bar)" },
                { "Ctrl + Z / Ctrl + Y", "Undo / redo" },
            } },
            { "View & tools", {
                { "Wheel", "Scroll up / down" },
                { "Shift + wheel", "Scroll left / right" },
                { "Ctrl + wheel", "Zoom horizontally" },
                { "Ctrl + Shift + wheel", "Zoom vertically" },
                { "P  /  E", "Draw tool / Select tool" },
                { "F1  or  ?", "Show this list" },
            } },
        };
        return sections;
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = owner.palette;
        g.fillAll (p.bg.withAlpha (0.82f));

        // 按固定的逻辑尺寸排版，窗口太小时整体缩小
        constexpr float cardW = 760.0f, cardH = 470.0f;
        const auto area = getLocalBounds().toFloat().reduced (12.0f);
        const auto scale = juce::jmin (1.0f, area.getWidth() / cardW, area.getHeight() / cardH);
        const auto card = juce::Rectangle<float> (cardW * scale, cardH * scale).withCentre (area.getCentre());

        neu::raised (g, card, 20.0f * scale, p, 6);

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (scale).translated (card.getX(), card.getY()));

        g.setColour (p.text);
        g.setFont (juce::FontOptions (18.0f, juce::Font::bold));
        g.drawText ("Mouse & keyboard", 32, 22, 400, 26, juce::Justification::centredLeft);
        g.setColour (p.muted);
        g.setFont (juce::FontOptions (12.5f));
        g.drawText ("Click anywhere or press Esc to close", (int) cardW - 332, 22, 300, 26, juce::Justification::centredRight);

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
                g.drawText (juce::String (s.title).toUpperCase(), juce::Rectangle<float> (x, y, colW, rowH), juce::Justification::centredLeft);
                y += rowH + 2.0f;

                for (const auto& item : s.items)
                {
                    g.setColour (p.text);
                    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
                    g.drawText (juce::String::fromUTF8 (item.keys), juce::Rectangle<float> (x, y, keysW, rowH), juce::Justification::centredLeft);
                    g.setColour (p.muted);
                    g.setFont (juce::FontOptions (13.0f));
                    g.drawText (item.action, juce::Rectangle<float> (x + keysW + 10.0f, y, colW - keysW - 10.0f, rowH), juce::Justification::centredLeft);
                    y += rowH;
                }

                y += 12.0f;
            }
        };

        drawColumn (32.0f, { 0, 1 });
        drawColumn (32.0f + colW + 24.0f, { 2, 3 });

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
PianoRoll::PianoRoll (CurvableProcessor& p, const Palette& pal)
    : processor (p), palette (pal),
      keyboard (std::make_unique<Keyboard> (*this)),
      ruler (std::make_unique<Ruler> (*this)),
      noteArea (std::make_unique<NoteArea> (*this)),
      velocityLane (std::make_unique<VelocityLane> (*this)),
      shortcutsPanel (std::make_unique<ShortcutsPanel> (*this))
{
    setWantsKeyboardFocus (true);

    for (auto* c : std::initializer_list<juce::Component*> { keyboard.get(), ruler.get(), noteArea.get(), velocityLane.get() })
        addAndMakeVisible (c);

    for (auto* sb : { &hScroll, &vScroll })
    {
        sb->setAutoHide (false);
        sb->setWantsKeyboardFocus (false);
        sb->addListener (this);
        addAndMakeVisible (sb);
    }

    CurvableLookAndFeel::setStyle (helpButton, "round");
    helpButton.setTooltip ("All mouse and keyboard actions (F1)");
    helpButton.setWantsKeyboardFocus (false);
    helpButton.onClick = [this] { showShortcuts (! shortcutsPanel->isVisible()); };
    addAndMakeVisible (helpButton);

    addChildComponent (shortcutsPanel.get());

    for (auto* b : { &drawButton, &selectButton })
    {
        CurvableLookAndFeel::setStyle (*b, "segment");
        b->setClickingTogglesState (true);
        b->setRadioGroupId (2001);
        b->setWantsKeyboardFocus (false);
        addAndMakeVisible (b);
    }

    drawButton.setTooltip ("Draw (P): click to add notes, drag to set their length");
    selectButton.setTooltip ("Select (E): drag to select notes");
    drawButton.onClick   = [this] { if (drawButton.getToggleState()) setTool (Tool::draw); };
    selectButton.onClick = [this] { if (selectButton.getToggleState()) setTool (Tool::select); };
    drawButton.setToggleState (true, juce::dontSendNotification);

    CurvableLookAndFeel::setStyle (snapButton, "pill");
    snapButton.setWantsKeyboardFocus (false);
    snapButton.onClick = [this] { showSnapMenu(); };
    snapButton.setButtonText ("Snap  " + getGridName());
    addAndMakeVisible (snapButton);

    processor.projectChanged.addChangeListener (this);
    pullProject();
    startTimerHz (30);
}

PianoRoll::~PianoRoll()
{
    stopTimer();
    stopPreview();
    processor.projectChanged.removeChangeListener (this);
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
    inner.removeFromBottom (velocityGap);
    hScroll.setBounds (inner.removeFromBottom (scrollBarSize));
    inner.removeFromBottom (scrollBarGap);
    noteArea->setBounds (inner);
    vScroll.setBounds (vScrollColumn.withY (inner.getY()).withHeight (inner.getHeight()));

    keysBounds = keysColumn.withY (inner.getY()).withHeight (inner.getHeight());
    keyboard->setBounds (keysBounds);

    // 底部工具栏
    toolsBounds = bottom.removeFromLeft (180).withSizeKeepingCentre (180, 42);
    auto tools = toolsBounds.reduced (4);
    drawButton.setBounds (tools.removeFromLeft (tools.getWidth() / 2));
    selectButton.setBounds (tools);

    bottom.removeFromLeft (14);
    snapButton.setBounds (bottom.removeFromLeft (130).withSizeKeepingCentre (130, 44));
    helpButton.setBounds (bottom.removeFromRight (44).withSizeKeepingCentre (44, 44));

    shortcutsPanel->setBounds (getLocalBounds());

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
        neu::inset (g, toolsBounds.toFloat(), 14.0f, palette, 3);
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.drawImage (chrome, getLocalBounds().toFloat());

    // 力度区标签
    const auto lane = velocityLane->getBounds();
    g.setColour (palette.muted);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("VEL", juce::Rectangle<int> (keysBounds.getX(), lane.getY(), keysBounds.getWidth() - 7, lane.getHeight()),
                juce::Justification::centredRight);

    // 底部工具栏右侧：状态或操作提示
    auto statusArea = getLocalBounds().removeFromBottom (bottomBarHeight)
                          .withTrimmedLeft (snapButton.getRight() + 16)
                          .withRight (helpButton.getX() - 8);
    g.setColour (statusIsError ? palette.error : palette.muted);
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (statusText.isNotEmpty() ? statusText : getHint(),
                      statusArea, juce::Justification::centredRight, 1, 0.9f);
}

juce::String PianoRoll::getHint() const
{
    // 只提示界面上看不出来的操作：工具、吸附、力度、滚动都有可见的控件，完整列表在 ? 里
    if (! selection.empty())
        return juce::String::fromUTF8 ("↑↓  Transpose    Ctrl+B  Duplicate    Del  Delete    Alt+drag  No snap");

    return "Right-click  Delete    Ctrl+wheel  Zoom    Alt+drag  No snap";
}

void PianoRoll::repaintContent()
{
    noteArea->repaint();
    velocityLane->repaint();
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
void PianoRoll::setTool (Tool newTool)
{
    tool = newTool;
    drawButton.setToggleState (tool == Tool::draw, juce::dontSendNotification);
    selectButton.setToggleState (tool == Tool::select, juce::dontSendNotification);
}

juce::String PianoRoll::getGridName() const
{
    for (const auto& option : gridOptions)
        if (std::abs (option.beats - grid) < 1.0e-9)
            return option.name;

    return "Custom";
}

void PianoRoll::showSnapMenu()
{
    juce::PopupMenu menu;
    int id = 1;

    for (const auto& option : gridOptions)
        menu.addItem (id++, option.name, true, std::abs (option.beats - grid) < 1.0e-9);

    juce::Component::SafePointer<PianoRoll> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&snapButton), [safeThis] (int result)
    {
        if (auto* self = safeThis.getComponent(); self != nullptr && result > 0)
        {
            self->grid = gridOptions[result - 1].beats;
            self->snapButton.setButtonText ("Snap  " + self->getGridName());
            self->noteArea->repaint();
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

    if (! cmd && (code == 'P' || code == 'p')) { setTool (Tool::draw); return true; }
    if (! cmd && (code == 'E' || code == 'e')) { setTool (Tool::select); return true; }
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

void PianoRoll::timerCallback()
{
    const auto beat = processor.getPlayheadBeat();

    if (std::abs (beat - playheadBeat) < 1.0e-6)
        return;

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

} // namespace curvable
