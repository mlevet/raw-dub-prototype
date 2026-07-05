#pragma once
#include "StepPattern.h"
#include "PointCurve.h"

namespace RawDub
{
// One slot in Skank's instrument pattern bank: the step data (on/off,
// pitch, Ghost/Normal/Accent, length - same as every instrument) plus
// the SawMix curve, which is pattern-scoped musical material, not
// synth state (see project_raw_dub_song_architecture memory). Kick and
// Bass don't need this wrapper yet since they have no curves - their
// banks are just plain StepPattern.
struct SkankPatternSlot
{
    explicit SkankPatternSlot (int initialActiveLength) : steps (initialActiveLength) {}

    StepPattern steps;
    PointCurve sawMixCurve;
};
}
