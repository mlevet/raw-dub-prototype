#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

namespace RawDub
{
// Minimal single-file project save/load - a development/listening tool,
// not a production feature. One fixed file, no browser, no presets, no
// arrangement. Goal: build a groove, save it, change synth code, rebuild,
// reload the same groove, compare honestly.
namespace ProjectIO
{
    juce::File getProjectFile();
    bool save (AudioEngine& engine);
    bool load (AudioEngine& engine);
}
}
