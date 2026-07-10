#include "ProjectIO.h"

namespace RawDub
{
namespace
{
juce::var curveToVar (const PointCurve& curve);
void curveFromVar (const juce::var& v, PointCurve& curve);
juce::var overrideMapToVar (const AudioEngine::OverrideMap& map);
void overrideMapFromVar (const juce::var& v, AudioEngine::OverrideMap& map);

juce::var patternToVar (const StepPattern& pattern)
{
    juce::Array<juce::var> onArr, levelArr, offsetArr;
    for (int i = 0; i < StepPattern::maxLength; ++i)
    {
        onArr.add (pattern.isOn (i));
        levelArr.add ((int) pattern.getLevel (i));
        offsetArr.add (pattern.getSemitoneOffset (i));
    }

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("activeLength", pattern.getActiveLength());
    obj->setProperty ("on", onArr);
    obj->setProperty ("levels", levelArr);
    obj->setProperty ("offsets", offsetArr);

    // Sparse, generic per-parameter curves (see StepPattern::curves) -
    // "any continuous parameter can become curve-capable," so this is a
    // map keyed by paramId, not a fixed field per parameter. Usually
    // empty or small - only params someone actually requested a curve
    // for are present at all.
    if (! pattern.getCurves().empty())
    {
        auto* curvesObj = new juce::DynamicObject();
        for (const auto& [id, curve] : pattern.getCurves())
            curvesObj->setProperty (juce::String (id), curveToVar (curve));
        obj->setProperty ("curves", juce::var (curvesObj));
    }

    return juce::var (obj);
}

void patternFromVar (const juce::var& v, StepPattern& pattern)
{
    if (! v.isObject())
        return;

    pattern.setActiveLength ((int) v.getProperty ("activeLength", pattern.getActiveLength()));

    auto* onArr = v.getProperty ("on", juce::var()).getArray();
    auto* levelArr = v.getProperty ("levels", juce::var()).getArray();
    auto* offsetArr = v.getProperty ("offsets", juce::var()).getArray();
    if (onArr == nullptr)
        return;

    for (int i = 0; i < StepPattern::maxLength && i < onArr->size(); ++i)
    {
        pattern.setOn (i, (bool) (*onArr)[i]);
        if (levelArr != nullptr && i < levelArr->size())
            pattern.setLevel (i, (StepLevel) (int) (*levelArr)[i]);
        if (offsetArr != nullptr && i < offsetArr->size())
            pattern.setSemitoneOffset (i, (int) (*offsetArr)[i]);
    }

    // "curves" is absent on older/legacy project files (or any pattern
    // that never had one requested) - that's just "no curves," not an
    // error, so there's nothing to restore and every param falls back to
    // its base/override value exactly as it always has.
    auto curvesVar = v.getProperty ("curves", juce::var());
    if (auto* curvesObj = curvesVar.getDynamicObject())
    {
        for (const auto& prop : curvesObj->getProperties())
        {
            int paramId = prop.name.toString().getIntValue();
            auto& curve = pattern.getOrCreateCurve (paramId, 0.5f);
            curveFromVar (prop.value, curve);
        }
    }
}

juce::var curveToVar (const PointCurve& curve)
{
    juce::Array<juce::var> points;
    int count = curve.getPointCount();
    for (int i = 0; i < count; ++i)
    {
        auto* pt = new juce::DynamicObject();
        pt->setProperty ("pos", (double) curve.getPointPosition (i));
        pt->setProperty ("val", (double) curve.getPointValue (i));
        points.add (juce::var (pt));
    }
    return points;
}

void curveFromVar (const juce::var& v, PointCurve& curve)
{
    curve.resetToValue (0.5f);

    auto* arr = v.getArray();
    if (arr == nullptr || arr->size() < 2)
        return;

    // first/last saved points are always the anchors (position 0 and 1
    // by construction) - just restore their values; interior points
    // get re-inserted in order
    curve.setPointValue (0, (float) (double) (*arr)[0].getProperty ("val", 0.5));
    curve.setPointValue (1, (float) (double) (*arr)[arr->size() - 1].getProperty ("val", 0.5));
    for (int i = 1; i < arr->size() - 1; ++i)
    {
        auto ptVar = (*arr)[i];
        float pos = (float) (double) ptVar.getProperty ("pos", 0.5);
        float val = (float) (double) ptVar.getProperty ("val", 0.5);
        curve.insertPoint (pos, val);
    }
}

// Sparse per-instrument section-override storage (see AudioEngine::
// OverrideMap) - nested object keyed by paramId-as-string, same "absent
// entry = default/inactive" convention patternToVar already uses for
// StepPattern::curves. Only entries someone actually touched exist at
// all, so an untouched Global Pattern's override maps serialize as
// empty objects, not 30-ish always-present zeroed fields.
juce::var overrideMapToVar (const AudioEngine::OverrideMap& map)
{
    auto* obj = new juce::DynamicObject();
    for (const auto& [id, ov] : map)
    {
        auto* entry = new juce::DynamicObject();
        entry->setProperty ("active", ov.active.load());
        entry->setProperty ("value", (double) ov.value.load());
        entry->setProperty ("hasCurve", ov.hasCurve.load());
        entry->setProperty ("curve", curveToVar (ov.curve));
        obj->setProperty (juce::String (id), juce::var (entry));
    }
    return juce::var (obj);
}

void overrideMapFromVar (const juce::var& v, AudioEngine::OverrideMap& map)
{
    map.clear();
    auto* obj = v.getDynamicObject();
    if (obj == nullptr)
        return;

    for (const auto& prop : obj->getProperties())
    {
        int paramId = prop.name.toString().getIntValue();
        auto& ov = map[paramId];
        ov.active.store ((bool) prop.value.getProperty ("active", false));
        ov.value.store ((float) (double) prop.value.getProperty ("value", 0.0));
        ov.hasCurve.store ((bool) prop.value.getProperty ("hasCurve", false));
        curveFromVar (prop.value.getProperty ("curve", juce::var()), ov.curve);
    }
}

// Instrument pattern banks - see project_raw_dub_song_architecture
// memory. Saves every slot in the bank plus which one is currently
// selected, not just the live pattern - so switching banks and saving
// preserves every slot's own material.
juce::var kickBankToVar (AudioEngine& engine)
{
    juce::Array<juce::var> arr;
    for (auto& p : engine.kickBank)
        arr.add (patternToVar (p));
    return arr;
}

juce::var bassBankToVar (AudioEngine& engine)
{
    juce::Array<juce::var> arr;
    for (auto& p : engine.bassBank)
        arr.add (patternToVar (p));
    return arr;
}

juce::var snareBankToVar (AudioEngine& engine)
{
    juce::Array<juce::var> arr;
    for (auto& p : engine.snareBank)
        arr.add (patternToVar (p));
    return arr;
}

juce::var hihatBankToVar (AudioEngine& engine)
{
    juce::Array<juce::var> arr;
    for (auto& p : engine.hihatBank)
        arr.add (patternToVar (p));
    return arr;
}

juce::var skankBankToVar (AudioEngine& engine)
{
    juce::Array<juce::var> arr;
    for (auto& slot : engine.skankBank)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("pattern", patternToVar (slot.steps));
        obj->setProperty ("sawMixCurve", curveToVar (slot.sawMixCurve));

        juce::Array<juce::var> chordArr;
        for (int i = 0; i < StepPattern::maxLength; ++i)
            chordArr.add (slot.getChordIsMinor (i));
        obj->setProperty ("chordIsMinor", chordArr);

        arr.add (juce::var (obj));
    }
    return arr;
}
}

juce::File ProjectIO::getDefaultProjectFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RawDubPrototype");
    dir.createDirectory();
    return dir.getChildFile ("project.json");
}

bool ProjectIO::save (AudioEngine& engine, const juce::File& file)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("tempo", engine.getTempoBpm());

    // Send effect on the whole mix, not a sequenced instrument - no
    // pattern bank, just its five params plus bypass (see DubDelay.h).
    auto* delayParams = new juce::DynamicObject();
    delayParams->setProperty ("timeMs",   (double) engine.delay.timeMs.load());
    delayParams->setProperty ("feedback", (double) engine.delay.feedback.load());
    delayParams->setProperty ("toneHz",   (double) engine.delay.toneHz.load());
    delayParams->setProperty ("drive",    (double) engine.delay.drive.load());
    delayParams->setProperty ("wet",      (double) engine.delay.wet.load());
    delayParams->setProperty ("bypass",   engine.delay.bypass.load());
    root->setProperty ("delay", juce::var (delayParams));

    auto* kickParams = new juce::DynamicObject();
    kickParams->setProperty ("tune",  (double) engine.kick.tuneHz.load());
    kickParams->setProperty ("punch", (double) engine.kick.punchMs.load());
    kickParams->setProperty ("decay", (double) engine.kick.decayMs.load());
    kickParams->setProperty ("drive", (double) engine.kick.drive.load());
    kickParams->setProperty ("volume", (double) engine.kick.volume.load());
    kickParams->setProperty ("delaySend", (double) engine.kick.delaySend.load());

    auto* kickObj = new juce::DynamicObject();
    kickObj->setProperty ("params", juce::var (kickParams));
    kickObj->setProperty ("currentPatternIndex", engine.getCurrentKickPatternIndex());
    kickObj->setProperty ("patternBank", kickBankToVar (engine));
    root->setProperty ("kick", juce::var (kickObj));

    auto* bassParams = new juce::DynamicObject();
    bassParams->setProperty ("tune",      (double) engine.bass.tuneHz.load());
    bassParams->setProperty ("drive",     (double) engine.bass.drive.load());
    bassParams->setProperty ("cutoff",    (double) engine.bass.cutoffHz.load());
    bassParams->setProperty ("resonance", (double) engine.bass.resonance.load());
    bassParams->setProperty ("length",    (double) engine.bass.decayMs.load());
    bassParams->setProperty ("volume",    (double) engine.bass.volume.load());
    bassParams->setProperty ("delaySend", (double) engine.bass.delaySend.load());
    bassParams->setProperty ("useAM",     engine.bass.useAMMode.load());
    bassParams->setProperty ("amRatio",   (double) engine.bass.amRatio.load());
    bassParams->setProperty ("amDepth",   (double) engine.bass.amDepth.load());
    bassParams->setProperty ("pitchEnvAmount", (double) engine.bass.pitchEnvAmount.load());
    bassParams->setProperty ("pitchEnvDecayMs", (double) engine.bass.pitchEnvDecayMs.load());
    bassParams->setProperty ("filterEnvAmount", (double) engine.bass.filterEnvAmount.load());
    bassParams->setProperty ("filterEnvDecayMs", (double) engine.bass.filterEnvDecayMs.load());
    bassParams->setProperty ("driveTransientAmount", (double) engine.bass.driveTransientAmount.load());

    auto* bassObj = new juce::DynamicObject();
    bassObj->setProperty ("params", juce::var (bassParams));
    bassObj->setProperty ("currentPatternIndex", engine.getCurrentBassPatternIndex());
    bassObj->setProperty ("patternBank", bassBankToVar (engine));
    root->setProperty ("bass", juce::var (bassObj));

    auto* skankParams = new juce::DynamicObject();
    skankParams->setProperty ("tune",   (double) engine.skank.tuneHz.load());
    skankParams->setProperty ("sawMix", (double) engine.skank.sawMix.load());
    skankParams->setProperty ("decay",  (double) engine.skank.decayMs.load());
    skankParams->setProperty ("drive",  (double) engine.skank.drive.load());
    skankParams->setProperty ("minor",  engine.skank.minorChord.load());
    skankParams->setProperty ("volume", (double) engine.skank.volume.load());
    skankParams->setProperty ("delaySend", (double) engine.skank.delaySend.load());

    auto* skankObj = new juce::DynamicObject();
    skankObj->setProperty ("params", juce::var (skankParams));
    skankObj->setProperty ("currentPatternIndex", engine.getCurrentSkankPatternIndex());
    skankObj->setProperty ("patternBank", skankBankToVar (engine));
    root->setProperty ("skank", juce::var (skankObj));

    auto* snareParams = new juce::DynamicObject();
    snareParams->setProperty ("tune",     (double) engine.snare.tuneHz.load());
    snareParams->setProperty ("noiseMix", (double) engine.snare.noiseMix.load());
    snareParams->setProperty ("cutoff",   (double) engine.snare.cutoffHz.load());
    snareParams->setProperty ("resonance",(double) engine.snare.resonance.load());
    snareParams->setProperty ("decay",    (double) engine.snare.decayMs.load());
    snareParams->setProperty ("drive",    (double) engine.snare.drive.load());
    snareParams->setProperty ("volume",   (double) engine.snare.volume.load());
    snareParams->setProperty ("delaySend",(double) engine.snare.delaySend.load());

    auto* snareObj = new juce::DynamicObject();
    snareObj->setProperty ("params", juce::var (snareParams));
    snareObj->setProperty ("currentPatternIndex", engine.getCurrentSnarePatternIndex());
    snareObj->setProperty ("patternBank", snareBankToVar (engine));
    root->setProperty ("snare", juce::var (snareObj));

    auto* hihatParams = new juce::DynamicObject();
    hihatParams->setProperty ("cutoff",   (double) engine.hihat.cutoffHz.load());
    hihatParams->setProperty ("resonance",(double) engine.hihat.resonance.load());
    hihatParams->setProperty ("decay",    (double) engine.hihat.decayMs.load());
    hihatParams->setProperty ("drive",    (double) engine.hihat.drive.load());
    hihatParams->setProperty ("volume",   (double) engine.hihat.volume.load());
    hihatParams->setProperty ("delaySend",(double) engine.hihat.delaySend.load());

    auto* hihatObj = new juce::DynamicObject();
    hihatObj->setProperty ("params", juce::var (hihatParams));
    hihatObj->setProperty ("currentPatternIndex", engine.getCurrentHiHatPatternIndex());
    hihatObj->setProperty ("patternBank", hihatBankToVar (engine));
    root->setProperty ("hihat", juce::var (hihatObj));

    // Global Patterns - see project_raw_dub_song_architecture memory.
    // No musical data, just five saved indices per slot, plus optional
    // section-level voicing overrides (see AudioEngine::OverrideMap -
    // one sparse map per instrument, covering every curve/override-
    // capable param on it now, not just Bass Drive/Cutoff/Resonance and
    // Skank Decay). Overrides are independent of "used" (edited through
    // their own controls, not created by Save), so they serialize
    // regardless of whether the slot has a saved combination.
    juce::Array<juce::var> globalPatternsArr;
    for (int i = 0; i < AudioEngine::globalPatternBankSize; ++i)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("used", engine.isGlobalPatternUsed (i));
        // recall into a scratch state would be needed to read these
        // back out cleanly, but the struct is a simple public
        // aggregate on AudioEngine, so read it directly instead
        const auto& gp = engine.globalPatterns[(size_t) i];
        if (engine.isGlobalPatternUsed (i))
        {
            obj->setProperty ("kick", gp.kickIndex);
            obj->setProperty ("bass", gp.bassIndex);
            obj->setProperty ("skank", gp.skankIndex);
            obj->setProperty ("snare", gp.snareIndex);
            obj->setProperty ("hihat", gp.hihatIndex);
        }
        obj->setProperty ("bassOverrides", overrideMapToVar (gp.bassOverrides));
        obj->setProperty ("kickOverrides", overrideMapToVar (gp.kickOverrides));
        obj->setProperty ("skankOverrides", overrideMapToVar (gp.skankOverrides));
        obj->setProperty ("snareOverrides", overrideMapToVar (gp.snareOverrides));
        obj->setProperty ("hihatOverrides", overrideMapToVar (gp.hihatOverrides));
        globalPatternsArr.add (juce::var (obj));
    }
    root->setProperty ("globalPatterns", globalPatternsArr);

    juce::var rootVar (root);
    return file.replaceWithText (juce::JSON::toString (rootVar));
}

bool ProjectIO::load (AudioEngine& engine, const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto rootVar = juce::JSON::parse (file.loadFileAsString());
    if (! rootVar.isObject())
        return false;

    engine.setTempoBpm ((double) rootVar.getProperty ("tempo", engine.getTempoBpm()));

    // absent "delay" on older project files is just "no delay saved
    // yet," not an error - every property below already falls back to
    // the engine's current (shipped-default) value
    auto delayVar = rootVar.getProperty ("delay", juce::var());
    if (delayVar.isObject())
    {
        engine.delay.timeMs.store   ((float) (double) delayVar.getProperty ("timeMs",   (double) engine.delay.timeMs.load()));
        engine.delay.feedback.store ((float) (double) delayVar.getProperty ("feedback", (double) engine.delay.feedback.load()));
        engine.delay.toneHz.store   ((float) (double) delayVar.getProperty ("toneHz",   (double) engine.delay.toneHz.load()));
        engine.delay.drive.store    ((float) (double) delayVar.getProperty ("drive",    (double) engine.delay.drive.load()));
        engine.delay.wet.store      ((float) (double) delayVar.getProperty ("wet",      (double) engine.delay.wet.load()));
        engine.delay.bypass.store   ((bool) delayVar.getProperty ("bypass", engine.delay.bypass.load()));
    }
    // a loaded project's delay buffer should never carry over old audio
    // energy from whatever was playing before Open was clicked
    engine.delay.clearBuffer();

    auto kickVar = rootVar.getProperty ("kick", juce::var());
    if (kickVar.isObject())
    {
        auto p = kickVar.getProperty ("params", juce::var());
        engine.kick.tuneHz.store  ((float) (double) p.getProperty ("tune",  (double) engine.kick.tuneHz.load()));
        engine.kick.punchMs.store ((float) (double) p.getProperty ("punch", (double) engine.kick.punchMs.load()));
        engine.kick.decayMs.store ((float) (double) p.getProperty ("decay", (double) engine.kick.decayMs.load()));
        engine.kick.drive.store   ((float) (double) p.getProperty ("drive", (double) engine.kick.drive.load()));
        engine.kick.volume.store  ((float) (double) p.getProperty ("volume", (double) engine.kick.volume.load()));
        engine.kick.delaySend.store ((float) (double) p.getProperty ("delaySend", (double) engine.kick.delaySend.load()));

        auto* bankArr = kickVar.getProperty ("patternBank", juce::var()).getArray();
        if (bankArr != nullptr)
        {
            for (int i = 0; i < AudioEngine::bankSize && i < bankArr->size(); ++i)
                patternFromVar ((*bankArr)[i], engine.kickBank[(size_t) i]);
            engine.setCurrentKickPatternIndex ((int) kickVar.getProperty ("currentPatternIndex", 0));
        }
        else
        {
            // legacy single-pattern save, from before pattern banks existed
            patternFromVar (kickVar.getProperty ("pattern", juce::var()), engine.kickBank[0]);
            engine.setCurrentKickPatternIndex (0);
        }
    }

    auto bassVar = rootVar.getProperty ("bass", juce::var());
    if (bassVar.isObject())
    {
        auto p = bassVar.getProperty ("params", juce::var());
        engine.bass.tuneHz.store    ((float) (double) p.getProperty ("tune",      (double) engine.bass.tuneHz.load()));
        engine.bass.drive.store     ((float) (double) p.getProperty ("drive",     (double) engine.bass.drive.load()));
        engine.bass.cutoffHz.store  ((float) (double) p.getProperty ("cutoff",    (double) engine.bass.cutoffHz.load()));
        engine.bass.resonance.store ((float) (double) p.getProperty ("resonance", (double) engine.bass.resonance.load()));
        engine.bass.decayMs.store   ((float) (double) p.getProperty ("length",    (double) engine.bass.decayMs.load()));
        engine.bass.volume.store   ((float) (double) p.getProperty ("volume",    (double) engine.bass.volume.load()));
        engine.bass.delaySend.store ((float) (double) p.getProperty ("delaySend", (double) engine.bass.delaySend.load()));
        engine.bass.useAMMode.store ((bool) p.getProperty ("useAM", engine.bass.useAMMode.load()));
        engine.bass.amRatio.store  ((float) (double) p.getProperty ("amRatio",   (double) engine.bass.amRatio.load()));
        engine.bass.amDepth.store  ((float) (double) p.getProperty ("amDepth",   (double) engine.bass.amDepth.load()));
        engine.bass.pitchEnvAmount.store ((float) (double) p.getProperty ("pitchEnvAmount", (double) engine.bass.pitchEnvAmount.load()));
        engine.bass.pitchEnvDecayMs.store ((float) (double) p.getProperty ("pitchEnvDecayMs", (double) engine.bass.pitchEnvDecayMs.load()));
        engine.bass.filterEnvAmount.store ((float) (double) p.getProperty ("filterEnvAmount", (double) engine.bass.filterEnvAmount.load()));
        engine.bass.filterEnvDecayMs.store ((float) (double) p.getProperty ("filterEnvDecayMs", (double) engine.bass.filterEnvDecayMs.load()));
        engine.bass.driveTransientAmount.store ((float) (double) p.getProperty ("driveTransientAmount", (double) engine.bass.driveTransientAmount.load()));

        auto* bankArr = bassVar.getProperty ("patternBank", juce::var()).getArray();
        if (bankArr != nullptr)
        {
            for (int i = 0; i < AudioEngine::bankSize && i < bankArr->size(); ++i)
                patternFromVar ((*bankArr)[i], engine.bassBank[(size_t) i]);
            engine.setCurrentBassPatternIndex ((int) bassVar.getProperty ("currentPatternIndex", 0));
        }
        else
        {
            patternFromVar (bassVar.getProperty ("pattern", juce::var()), engine.bassBank[0]);
            engine.setCurrentBassPatternIndex (0);
        }
    }

    auto skankVar = rootVar.getProperty ("skank", juce::var());
    if (skankVar.isObject())
    {
        auto p = skankVar.getProperty ("params", juce::var());
        engine.skank.tuneHz.store    ((float) (double) p.getProperty ("tune",   (double) engine.skank.tuneHz.load()));
        engine.skank.sawMix.store    ((float) (double) p.getProperty ("sawMix", (double) engine.skank.sawMix.load()));
        engine.skank.decayMs.store   ((float) (double) p.getProperty ("decay",  (double) engine.skank.decayMs.load()));
        engine.skank.drive.store     ((float) (double) p.getProperty ("drive",  (double) engine.skank.drive.load()));
        engine.skank.minorChord.store ((bool) p.getProperty ("minor", engine.skank.minorChord.load()));
        engine.skank.volume.store    ((float) (double) p.getProperty ("volume", (double) engine.skank.volume.load()));
        engine.skank.delaySend.store ((float) (double) p.getProperty ("delaySend", (double) engine.skank.delaySend.load()));

        auto* bankArr = skankVar.getProperty ("patternBank", juce::var()).getArray();
        if (bankArr != nullptr)
        {
            for (int i = 0; i < AudioEngine::bankSize && i < bankArr->size(); ++i)
            {
                auto slotVar = (*bankArr)[i];
                patternFromVar (slotVar.getProperty ("pattern", juce::var()), engine.skankBank[(size_t) i].steps);
                curveFromVar (slotVar.getProperty ("sawMixCurve", juce::var()), engine.skankBank[(size_t) i].sawMixCurve);

                // absent entirely = legacy save from before per-step chord
                // quality existed - every step defaults to major (false),
                // already correct from construction
                auto* chordArr = slotVar.getProperty ("chordIsMinor", juce::var()).getArray();
                if (chordArr != nullptr)
                    for (int step = 0; step < StepPattern::maxLength && step < chordArr->size(); ++step)
                        engine.skankBank[(size_t) i].setChordIsMinor (step, (bool) (*chordArr)[step]);
            }
            engine.setCurrentSkankPatternIndex ((int) skankVar.getProperty ("currentPatternIndex", 0));
        }
        else
        {
            // legacy single-pattern save
            patternFromVar (skankVar.getProperty ("pattern", juce::var()), engine.skankBank[0].steps);
            curveFromVar (skankVar.getProperty ("sawMixCurve", juce::var()), engine.skankBank[0].sawMixCurve);
            engine.setCurrentSkankPatternIndex (0);
        }
    }

    auto snareVar = rootVar.getProperty ("snare", juce::var());
    if (snareVar.isObject())
    {
        auto p = snareVar.getProperty ("params", juce::var());
        engine.snare.tuneHz.store    ((float) (double) p.getProperty ("tune",      (double) engine.snare.tuneHz.load()));
        engine.snare.noiseMix.store  ((float) (double) p.getProperty ("noiseMix",  (double) engine.snare.noiseMix.load()));
        engine.snare.cutoffHz.store  ((float) (double) p.getProperty ("cutoff",    (double) engine.snare.cutoffHz.load()));
        engine.snare.resonance.store ((float) (double) p.getProperty ("resonance", (double) engine.snare.resonance.load()));
        engine.snare.decayMs.store   ((float) (double) p.getProperty ("decay",     (double) engine.snare.decayMs.load()));
        engine.snare.drive.store     ((float) (double) p.getProperty ("drive",     (double) engine.snare.drive.load()));
        engine.snare.volume.store    ((float) (double) p.getProperty ("volume",    (double) engine.snare.volume.load()));
        engine.snare.delaySend.store ((float) (double) p.getProperty ("delaySend", (double) engine.snare.delaySend.load()));

        auto* bankArr = snareVar.getProperty ("patternBank", juce::var()).getArray();
        if (bankArr != nullptr)
        {
            for (int i = 0; i < AudioEngine::bankSize && i < bankArr->size(); ++i)
                patternFromVar ((*bankArr)[i], engine.snareBank[(size_t) i]);
            engine.setCurrentSnarePatternIndex ((int) snareVar.getProperty ("currentPatternIndex", 0));
        }
    }

    auto hihatVar = rootVar.getProperty ("hihat", juce::var());
    if (hihatVar.isObject())
    {
        auto p = hihatVar.getProperty ("params", juce::var());
        engine.hihat.cutoffHz.store  ((float) (double) p.getProperty ("cutoff",    (double) engine.hihat.cutoffHz.load()));
        engine.hihat.resonance.store ((float) (double) p.getProperty ("resonance", (double) engine.hihat.resonance.load()));
        engine.hihat.decayMs.store   ((float) (double) p.getProperty ("decay",     (double) engine.hihat.decayMs.load()));
        engine.hihat.drive.store     ((float) (double) p.getProperty ("drive",     (double) engine.hihat.drive.load()));
        engine.hihat.volume.store    ((float) (double) p.getProperty ("volume",    (double) engine.hihat.volume.load()));
        engine.hihat.delaySend.store ((float) (double) p.getProperty ("delaySend", (double) engine.hihat.delaySend.load()));

        auto* bankArr = hihatVar.getProperty ("patternBank", juce::var()).getArray();
        if (bankArr != nullptr)
        {
            for (int i = 0; i < AudioEngine::bankSize && i < bankArr->size(); ++i)
                patternFromVar ((*bankArr)[i], engine.hihatBank[(size_t) i]);
            engine.setCurrentHiHatPatternIndex ((int) hihatVar.getProperty ("currentPatternIndex", 0));
        }
    }

    auto* globalPatternsArr = rootVar.getProperty ("globalPatterns", juce::var()).getArray();
    if (globalPatternsArr != nullptr)
    {
        for (int i = 0; i < AudioEngine::globalPatternBankSize && i < globalPatternsArr->size(); ++i)
        {
            auto slotVar = (*globalPatternsArr)[i];
            auto& gp = engine.globalPatterns[(size_t) i];
            gp.used = (bool) slotVar.getProperty ("used", false);
            gp.kickIndex = (int) slotVar.getProperty ("kick", 0);
            gp.bassIndex = (int) slotVar.getProperty ("bass", 0);
            gp.skankIndex = (int) slotVar.getProperty ("skank", 0);
            gp.snareIndex = (int) slotVar.getProperty ("snare", 0);
            gp.hihatIndex = (int) slotVar.getProperty ("hihat", 0);
            // absent entirely = legacy save from before overrides existed
            // (or before they covered this instrument) - empty map is
            // already correct (no overrides configured)
            overrideMapFromVar (slotVar.getProperty ("bassOverrides", juce::var()), gp.bassOverrides);
            overrideMapFromVar (slotVar.getProperty ("kickOverrides", juce::var()), gp.kickOverrides);
            overrideMapFromVar (slotVar.getProperty ("skankOverrides", juce::var()), gp.skankOverrides);
            overrideMapFromVar (slotVar.getProperty ("snareOverrides", juce::var()), gp.snareOverrides);
            overrideMapFromVar (slotVar.getProperty ("hihatOverrides", juce::var()), gp.hihatOverrides);
        }
    }
    // absent entirely = legacy save from before Global Patterns existed - leave all slots unused, nothing to migrate

    return true;
}
}
