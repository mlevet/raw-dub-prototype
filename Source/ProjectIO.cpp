#include "ProjectIO.h"

namespace RawDub
{
namespace
{
juce::var patternToVar (StepPattern& pattern)
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
    kickObj->setProperty ("pattern", patternToVar (engine.kickPattern));
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
    bassObj->setProperty ("pattern", patternToVar (engine.bassPattern));
    root->setProperty ("bass", juce::var (bassObj));

    auto* skankParams = new juce::DynamicObject();
    skankParams->setProperty ("tune",   (double) engine.skank.tuneHz.load());
    skankParams->setProperty ("sawMix", (double) engine.skank.sawMix.load());
    skankParams->setProperty ("decay",  (double) engine.skank.decayMs.load());
    skankParams->setProperty ("drive",  (double) engine.skank.drive.load());
    skankParams->setProperty ("minor",  engine.skank.minorChord.load());
    skankParams->setProperty ("volume", (double) engine.skank.volume.load());

    juce::Array<juce::var> sawMixCurveArr;
    int sawMixPointCount = engine.skank.getSawMixCurvePointCount();
    for (int i = 0; i < sawMixPointCount; ++i)
    {
        auto* pt = new juce::DynamicObject();
        pt->setProperty ("pos", (double) engine.skank.getSawMixCurvePointPosition (i));
        pt->setProperty ("val", (double) engine.skank.getSawMixCurvePointValue (i));
        sawMixCurveArr.add (juce::var (pt));
    }

    auto* skankObj = new juce::DynamicObject();
    skankObj->setProperty ("params", juce::var (skankParams));
    skankObj->setProperty ("pattern", patternToVar (engine.skankPattern));
    skankObj->setProperty ("sawMixCurve", sawMixCurveArr);
    root->setProperty ("skank", juce::var (skankObj));

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
        patternFromVar (kickVar.getProperty ("pattern", juce::var()), engine.kickPattern);
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
        patternFromVar (bassVar.getProperty ("pattern", juce::var()), engine.bassPattern);
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
        patternFromVar (skankVar.getProperty ("pattern", juce::var()), engine.skankPattern);

        auto* curveArr = skankVar.getProperty ("sawMixCurve", juce::var()).getArray();
        if (curveArr != nullptr && curveArr->size() >= 2)
        {
            // first/last saved points are always the anchors (position 0
            // and 1 by construction) - just restore their values; any
            // interior points get re-inserted in order
            engine.skank.resetSawMixLaneToValue (0.5f);
            engine.skank.setSawMixCurvePointValue (0, (float) (double) (*curveArr)[0].getProperty ("val", 0.5));
            engine.skank.setSawMixCurvePointValue (1, (float) (double) (*curveArr)[curveArr->size() - 1].getProperty ("val", 0.5));
            for (int i = 1; i < curveArr->size() - 1; ++i)
            {
                auto ptVar = (*curveArr)[i];
                float pos = (float) (double) ptVar.getProperty ("pos", 0.5);
                float val = (float) (double) ptVar.getProperty ("val", 0.5);
                engine.skank.insertSawMixCurvePoint (pos, val);
            }
        }
    }

    return true;
}
}
