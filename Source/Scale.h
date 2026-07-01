#pragma once
#include <array>
#include <cstdlib>

namespace RawDub
{
// Natural minor scale degrees as semitone offsets from the root,
// covering the +/-12 range StepButton's pitch drag operates in.
// Root-agnostic: whatever a voice's Tune knob is set to acts as the
// root, so no separate "song key" concept exists yet - that's only
// needed once chord-quality inference (Harmony role) is real.
inline int nearestScaleDegreeOffset (int rawSemitoneOffset)
{
    static constexpr std::array<int, 15> degrees = {
        -12, -10, -9, -7, -5, -4, -2, 0, 2, 3, 5, 7, 8, 10, 12
    };

    int best = degrees.front();
    int bestDist = 999;

    for (int d : degrees)
    {
        int dist = std::abs (d - rawSemitoneOffset);
        if (dist < bestDist)
        {
            bestDist = dist;
            best = d;
        }
    }

    return best;
}
}
