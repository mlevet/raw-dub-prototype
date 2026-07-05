#include "ProjectIO.h"

namespace RawDub
{
namespace
{
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

juce::var skankBankToVar (AudioEngine& engine)
{
    juce::Array<juce::var> arr;
    for (auto& slot : engine.skankBank)
    {
        auto* obj = new juce::DynamicObject();
        obj->setProperty ("pattern", patternToVar (slot.steps));
        obj->setProperty ("sawMixCurve", curveToVar (slot.sawMixCurve));
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

    auto* kickParams = new juce::DynamicObject();
    kickParams->setProperty ("tune",  (double) engine.kick.tuneHz.load());
    kickParams->setProperty ("punch", (double) engine.kick.punchMs.load());
    kickParams->setProperty ("decay", (double) engine.kick.decayMs.load());
    kickParams->setProperty ("drive", (double) engine.kick.drive.load());
    kickParams->setProperty ("volume", (double) engine.kick.volume.load());

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
    bassParams->setProperty ("useAM",     engine.bass.useAMMode.load());
    bassParams->setProperty ("amRatio",   (double) engine.bass.amRatio.load());
    bassParams->setProperty ("amDepth",   (double) engine.bass.amDepth.load());

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

    auto* skankObj = new juce::DynamicObject();
    skankObj->setProperty ("params", juce::var (skankParams));
    skankObj->setProperty ("currentPatternIndex", engine.getCurrentSkankPatternIndex());
    skankObj->setProperty ("patternBank", skankBankToVar (engine));
    root->setProperty ("skank", juce::var (skankObj));

    // Global Patterns - see project_raw_dub_song_architecture memory.
    // No musical data, just three saved indices per slot, plus optional
    // section-level voicing overrides (Bass Drive/Cutoff - see
    // AudioEngine::ParamOverride). Overrides are independent of "used"
    // (edited through their own controls, not created by Save), so they
    // serialize regardless of whether the slot has a saved combination.
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
        }
        obj->setProperty ("bassDriveOverrideActive", gp.bassDriveOverride.active.load());
        obj->setProperty ("bassDriveOverrideValue", (double) gp.bassDriveOverride.value.load());
        obj->setProperty ("bassCutoffOverrideActive", gp.bassCutoffOverride.active.load());
        obj->setProperty ("bassCutoffOverrideValue", (double) gp.bassCutoffOverride.value.load());
        obj->setProperty ("skankDecayOverrideActive", gp.skankDecayOverride.active.load());
        obj->setProperty ("skankDecayOverrideValue", (double) gp.skankDecayOverride.value.load());
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

    auto kickVar = rootVar.getProperty ("kick", juce::var());
    if (kickVar.isObject())
    {
        auto p = kickVar.getProperty ("params", juce::var());
        engine.kick.tuneHz.store  ((float) (double) p.getProperty ("tune",  (double) engine.kick.tuneHz.load()));
        engine.kick.punchMs.store ((float) (double) p.getProperty ("punch", (double) engine.kick.punchMs.load()));
        engine.kick.decayMs.store ((float) (double) p.getProperty ("decay", (double) engine.kick.decayMs.load()));
        engine.kick.drive.store   ((float) (double) p.getProperty ("drive", (double) engine.kick.drive.load()));
        engine.kick.volume.store  ((float) (double) p.getProperty ("volume", (double) engine.kick.volume.load()));

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
        engine.bass.useAMMode.store ((bool) p.getProperty ("useAM", engine.bass.useAMMode.load()));
        engine.bass.amRatio.store  ((float) (double) p.getProperty ("amRatio",   (double) engine.bass.amRatio.load()));
        engine.bass.amDepth.store  ((float) (double) p.getProperty ("amDepth",   (double) engine.bass.amDepth.load()));

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

        auto* bankArr = skankVar.getProperty ("patternBank", juce::var()).getArray();
        if (bankArr != nullptr)
        {
            for (int i = 0; i < AudioEngine::bankSize && i < bankArr->size(); ++i)
            {
                auto slotVar = (*bankArr)[i];
                patternFromVar (slotVar.getProperty ("pattern", juce::var()), engine.skankBank[(size_t) i].steps);
                curveFromVar (slotVar.getProperty ("sawMixCurve", juce::var()), engine.skankBank[(size_t) i].sawMixCurve);
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
            // absent entirely = legacy save from before overrides existed - defaults (no override) are already correct
            gp.bassDriveOverride.active.store ((bool) slotVar.getProperty ("bassDriveOverrideActive", false));
            gp.bassDriveOverride.value.store ((float) (double) slotVar.getProperty ("bassDriveOverrideValue", 0.0));
            gp.bassCutoffOverride.active.store ((bool) slotVar.getProperty ("bassCutoffOverrideActive", false));
            gp.bassCutoffOverride.value.store ((float) (double) slotVar.getProperty ("bassCutoffOverrideValue", 0.0));
            gp.skankDecayOverride.active.store ((bool) slotVar.getProperty ("skankDecayOverrideActive", false));
            gp.skankDecayOverride.value.store ((float) (double) slotVar.getProperty ("skankDecayOverrideValue", 0.0));
        }
    }
    // absent entirely = legacy save from before Global Patterns existed - leave all slots unused, nothing to migrate

    return true;
}
}
