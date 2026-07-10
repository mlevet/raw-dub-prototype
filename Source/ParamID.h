#pragma once

namespace RawDub
{
// Identifies a continuous, curve-capable parameter for pattern-level
// curve storage (StepPattern::curves) - one enum per instrument, since
// each instrument's pattern owns its own curves map, so IDs only need to
// be unique WITHIN one instrument, never globally. See
// project_raw_dub_song_architecture memory: "any continuous parameter
// can become curve-capable" - the only real fork is continuous vs.
// discrete, decided once, when a parameter is given a slider instead of
// a button/mode-switch. Each list below is simply that instrument's
// continuous parameters, not a curated "these got picked" subset - a
// future continuous parameter joins its instrument's enum the moment it
// exists, with no separate decision required.
//
// Deliberately excludes Volume on every instrument - it's read
// continuously every audio block (a live mixing control, headed to a
// future mixer page), not resolved once at trigger time like every
// other parameter here, so it doesn't fit the trigger-time-snapshot
// curve mechanism every other param uses (same reason it never gets a
// section override either).
//
// Delay Send is DIFFERENT from Volume despite also being continuously
// read: it's still resolved fresh every STEP (not just on-steps, not
// snapshotted per-note like Tune/Drive/etc) via AudioEngine::advanceStep,
// stored into a per-instrument resolved-value atom AudioEngine reads
// every block - see AudioEngine.h's resolvedKickSend etc. This works
// without touching any synth's trigger()/renderAdd() because Delay Send
// was already a block-level mix-routing scalar applied entirely inside
// AudioEngine (kickScratch[i] * kickSend), never inside the synth - only
// WHERE that scalar comes from changed (resolved, not a raw knob read),
// not its granularity.
enum class DelayParamID
{
    Feedback,
    Tone,
    Drive,
    Wet,
    // Time is deliberately absent - DubDelay::process reads it as a
    // plain, non-interpolated integer buffer offset (see its own
    // comment), so changing it already causes an audible jump even from
    // the slider; animating it via a fast-moving curve would turn that
    // into a rhythmic glitch every step. Fixing that would mean an
    // interpolated/crossfaded delay-line read, a separate DSP change,
    // not something folded in silently here - Time stays a plain knob
    // until that's actually done.
    //
    // Delay has no StepPattern of its own (it's not step-triggered) -
    // AudioEngine::delayPattern is a StepPattern used purely as a curve
    // + Length container (its step on/off/level/pitch data is never
    // touched), sampled by the GLOBAL step counter over its own
    // adjustable Length, the same fixed-scale-loop model every
    // instrument's pattern already gives curves - see AudioEngine.h.
};
enum class BassParamID
{
    Tune,
    Drive,
    Cutoff,
    Resonance,
    Length,
    AmDepth,
    PitchEnvAmount,
    PitchEnvDecay,
    FilterEnvAmount,
    FilterEnvDecay,
    DriveTransientAmount,
    DelaySend,
};

enum class KickParamID
{
    Tune,
    Punch,
    Decay,
    Drive,
    DelaySend,
};

enum class SkankParamID
{
    Tune,
    Decay,
    Drive,
    DelaySend,
    SawMix,
};

enum class SnareParamID
{
    Tune,
    NoiseMix,
    Cutoff,
    Resonance,
    Decay,
    Drive,
    DelaySend,
};

enum class HiHatParamID
{
    Cutoff,
    Resonance,
    Decay,
    Drive,
    DelaySend,
};
}
