#pragma once
#include <JuceHeader.h>
#include "AudioEngine.h"

namespace RawDub
{
// Minimal multi-file project save/load - projects are individual JSON
// files, chosen via a normal file chooser (Save/Save As/Open/New in
// MainComponent). No browser, no presets, no arrangement - just enough
// to branch ideas and come back to them without overwriting anything.
namespace ProjectIO
{
    // legacy fixed location from the single-slot era - kept only so an
    // existing save there is still found and auto-loaded on first launch
    juce::File getDefaultProjectFile();
    bool save (AudioEngine& engine, const juce::File& file);
    bool load (AudioEngine& engine, const juce::File& file);
}
}
