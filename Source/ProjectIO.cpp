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

juce::File ProjectIO::getProjectFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("RawDubPrototype");
    dir.createDirectory();
    return dir.getChildFile ("project.json");
}

bool ProjectIO::save (AudioEngine& engine)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("tempo", engine.getTempoBpm());

    auto* kickParams = new juce::DynamicObject();
    kickParams->setProperty ("tune",  (double) engine.kick.tuneHz.load());
    kickParams->setProperty ("punch", (double) engine.kick.punchMs.load());
    kickParams->setProperty ("decay", (double) engine.kick.decayMs.load());
    kickParams->setProperty ("drive", (double) engine.kick.drive.load());

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

    auto* bassObj = new juce::DynamicObject();
    bassObj->setProperty ("params", juce::var (bassParams));
    bassObj->setProperty ("pattern", patternToVar (engine.bassPattern));
    root->setProperty ("bass", juce::var (bassObj));

    juce::var rootVar (root);
    return getProjectFile().replaceWithText (juce::JSON::toString (rootVar));
}

bool ProjectIO::load (AudioEngine& engine)
{
    auto file = getProjectFile();
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
        patternFromVar (bassVar.getProperty ("pattern", juce::var()), engine.bassPattern);
    }

    return true;
}
}
