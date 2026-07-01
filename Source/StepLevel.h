#pragma once

namespace RawDub
{
// Three musical levels instead of raw MIDI velocity - a step is
// quieter, normal, or accented, never a number.
enum class StepLevel { Ghost = 0, Normal = 1, Accent = 2 };

inline float stepLevelGain (StepLevel level)
{
    switch (level)
    {
        case StepLevel::Ghost:  return 0.5f;
        case StepLevel::Accent: return 1.15f;
        case StepLevel::Normal:
        default:                return 0.85f;
    }
}
}
