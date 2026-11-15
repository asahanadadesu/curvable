#include "model/ProjectSerializer.h"

namespace curvable
{

namespace
{
    namespace ids
    {
        const juce::Identifier version  { "version" };
        const juce::Identifier mode     { "mode" };
        const juce::Identifier nextId   { "nextId" };
        const juce::Identifier note     { "Note" };
        const juce::Identifier cue      { "Cue" };
        const juce::Identifier id       { "id" };
        const juce::Identifier start    { "start" };
        const juce::Identifier length   { "length" };
        const juce::Identifier end      { "end" };
        const juce::Identifier pitch    { "pitch" };
        const juce::Identifier velocity { "velocity" };
        const juce::Identifier key      { "key" };
        const juce::Identifier point    { "Point" };
        const juce::Identifier time     { "t" };
        const juce::Identifier offset   { "st" };
        const juce::Identifier tension  { "tension" };
        const juce::Identifier bendRange { "bendRange" };
        const juce::Identifier vibrato  { "Vibrato" };
        const juce::Identifier depth    { "depth" };
        const juce::Identifier rate     { "rate" };
        const juce::Identifier sync     { "sync" };
        const juce::Identifier fade     { "fade" };
        const juce::Identifier wholeNote { "wholeNote" };
        const juce::Identifier vib      { "vib" };
        const juce::Identifier shape    { "shape" };
    }

    // 枚举按名字保存，以后调整枚举顺序也不会读错
    juce::String modeToString (PlaybackMode mode)
    {
        switch (mode)
        {
            case PlaybackMode::automatic: return "auto";
            case PlaybackMode::trigger:   return "trigger";
            case PlaybackMode::off:       return "off";
        }

        return "auto";
    }

    PlaybackMode modeFromString (const juce::String& s)
    {
        if (s == "trigger") return PlaybackMode::trigger;
        if (s == "off")     return PlaybackMode::off;
        return PlaybackMode::automatic;
    }

    // 64 位 id 存成字符串：ValueTree 的 var 在 XML 中对 int64 的支持不稳定
    juce::String idToString (NoteId id)              { return juce::String ((juce::uint64) id); }
    NoteId idFromVar (const juce::var& v)            { return (NoteId) v.toString().getLargeIntValue(); }
}

juce::ValueTree ProjectSerializer::toValueTree (const Project& project)
{
    juce::ValueTree tree (type);
    tree.setProperty (ids::version, currentVersion, nullptr);
    tree.setProperty (ids::mode, modeToString (project.mode), nullptr);
    tree.setProperty (ids::nextId, idToString (project.nextId), nullptr);
    tree.setProperty (ids::bendRange, project.bendRange, nullptr);

    for (const auto& n : project.notes)
    {
        juce::ValueTree note (ids::note);
        note.setProperty (ids::id, idToString (n.id), nullptr);
        note.setProperty (ids::start, n.start, nullptr);
        note.setProperty (ids::length, n.length, nullptr);
        note.setProperty (ids::pitch, n.pitch, nullptr);
        note.setProperty (ids::velocity, n.velocity, nullptr);

        writeCurve (n.curve, note);
        tree.appendChild (note, nullptr);
    }

    for (const auto& c : project.cues)
    {
        juce::ValueTree cue (ids::cue);
        cue.setProperty (ids::key, c.key, nullptr);
        cue.setProperty (ids::start, c.start, nullptr);
        cue.setProperty (ids::end, c.end, nullptr);
        tree.appendChild (cue, nullptr);
    }

    return tree;
}

namespace
{
    juce::ValueTree vibratoToTree (const Vibrato& v, bool includeWholeNote)
    {
        juce::ValueTree vib (ids::vibrato);

        if (v.shape == Vibrato::Shape::trill)
            vib.setProperty (ids::shape, "trill", nullptr);

        if (includeWholeNote)
            vib.setProperty (ids::wholeNote, v.wholeNote, nullptr);

        vib.setProperty (ids::depth, v.depth, nullptr);
        vib.setProperty (ids::rate, v.rate, nullptr);
        vib.setProperty (ids::sync, v.sync, nullptr);
        vib.setProperty (ids::fade, v.fade, nullptr);
        return vib;
    }

    Vibrato vibratoFromTree (const juce::ValueTree& vib)
    {
        const Vibrato defaults;
        Vibrato v;
        v.shape     = vib.getProperty (ids::shape).toString() == "trill" ? Vibrato::Shape::trill : Vibrato::Shape::sine;
        v.wholeNote = vib.getProperty (ids::wholeNote, false);
        v.depth     = vib.getProperty (ids::depth, defaults.depth);
        v.rate      = vib.getProperty (ids::rate, defaults.rate);
        v.sync      = vib.getProperty (ids::sync, false);
        v.fade      = vib.getProperty (ids::fade, defaults.fade);
        return v;
    }
}

void ProjectSerializer::writeCurve (const PitchCurve& curve, juce::ValueTree& parent)
{
    for (const auto& p : curve.points)
    {
        juce::ValueTree point (ids::point);
        point.setProperty (ids::time, p.time, nullptr);
        point.setProperty (ids::offset, p.semitones, nullptr);

        if (p.tension != 0.0f)
            point.setProperty (ids::tension, p.tension, nullptr);

        // 这一段自己的颤音参数
        if (p.vibrato)
            point.appendChild (vibratoToTree (*p.vibrato, false), nullptr);

        parent.appendChild (point, nullptr);
    }

    // 整个音符的颤音参数在去掉颤音后也保存（再次加上时沿用）；从没改过的默认值不保存
    if (const auto& v = curve.vibrato; ! (v == Vibrato {}))
        parent.appendChild (vibratoToTree (v, true), nullptr);
}

PitchCurve ProjectSerializer::readCurve (const juce::ValueTree& parent)
{
    PitchCurve curve;

    if (const auto vib = parent.getChildWithName (ids::vibrato); vib.isValid())
        curve.vibrato = vibratoFromTree (vib);

    for (const auto& pointTree : parent)
    {
        if (! pointTree.hasType (ids::point))
            continue;

        CurvePoint point { (double) pointTree.getProperty (ids::time, 0.0),
                           (double) pointTree.getProperty (ids::offset, 0.0),
                           (float) (double) pointTree.getProperty (ids::tension, 0.0),
                           std::nullopt };

        if (const auto vib = pointTree.getChildWithName (ids::vibrato); vib.isValid())
        {
            point.vibrato = vibratoFromTree (vib);
        }
        else if ((bool) pointTree.getProperty (ids::vib, false))
        {
            // 早期的格式：节点只有一个标记，参数用整个音符的那一组
            point.vibrato = curve.vibrato;
        }

        curve.points.push_back (point);
    }

    curve.normalise();
    return curve;
}

std::optional<Project> ProjectSerializer::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType (type))
        return std::nullopt;

    if ((int) tree.getProperty (ids::version, 1) > currentVersion)
        return std::nullopt; // 由更新版本的 Curvable 保存，不冒险读取

    Project project;
    project.notes.clear();
    project.cues.clear();
    project.mode = modeFromString (tree.getProperty (ids::mode).toString());

    const auto bendRange = (double) tree.getProperty (ids::bendRange, Project::defaultBendRange);
    project.bendRange = std::isfinite (bendRange) ? std::clamp (bendRange, Project::minBendRange, Project::maxBendRange)
                                                  : Project::defaultBendRange;

    NoteId maxId = 0;

    for (const auto& child : tree)
    {
        if (child.hasType (ids::note))
        {
            Note n;
            n.id       = idFromVar (child.getProperty (ids::id));
            n.start    = child.getProperty (ids::start, 0.0);
            n.length   = child.getProperty (ids::length, 1.0);
            n.pitch    = child.getProperty (ids::pitch, 60);
            n.velocity = (float) (double) child.getProperty (ids::velocity, 0.8);

            n.curve = readCurve (child);
            project.notes.push_back (n);
            maxId = std::max (maxId, n.id);
        }
        else if (child.hasType (ids::cue))
        {
            TriggerCue c;
            c.key   = child.getProperty (ids::key, TriggerCue::anyKey);
            c.start = child.getProperty (ids::start, 0.0);
            c.end   = child.getProperty (ids::end, 0.0);
            project.cues.push_back (c);
        }
    }

    // 旧数据或手工编辑导致 id 缺失/重复时，重新分配，保证唯一
    {
        std::vector<NoteId> seen;
        seen.reserve (project.notes.size());

        for (auto& n : project.notes)
        {
            if (n.id == 0 || std::find (seen.begin(), seen.end(), n.id) != seen.end())
                n.id = ++maxId;

            seen.push_back (n.id);
        }
    }

    project.nextId = std::max (idFromVar (tree.getProperty (ids::nextId)), maxId + 1);

    if (project.cues.empty())
        project.cues.push_back ({});

    return project;
}

} // namespace curvable
