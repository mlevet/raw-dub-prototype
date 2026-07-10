#include "AudioEngine.h"
#include "ParamID.h"
#include <algorithm>
#include <cmath>

namespace RawDub
{
namespace
{
// Resolution order for any continuous parameter, on any instrument,
// regardless of which one: section override (if active) wins outright -
// as either its own shaped curve or a flat value, see AudioEngine::
// ParamOverride - else pattern curve (if present, sampled at this step's
// phrase position and denormalized into the param's real range); else -1
// ("use my own knob" sentinel, same convention every synth's trigger()
// uses). An active override fully takes over for this section, curve or
// not - that's what lets a section hold its own curve variant while the
// pattern's own curve stays untouched for every other section/pattern
// that still reads it directly. See project_raw_dub_song_architecture
// memory, "any continuous parameter can become curve-capable" - this one
// function is what makes that true for every param on every instrument
// without special-casing per parameter or per instrument; only the id/
// range/override/pattern differ per call. ov may be null for params with
// no override mechanism wired at all (falls straight through to the
// pattern-curve/base check) - none currently, every in-scope param has
// one, but the null path is kept since it costs nothing and matches
// findOverride's own nullable return.
float resolveParam (const StepPattern& pattern, int paramId, float stepFraction,
                     float rangeMin, float rangeMax, const AudioEngine::ParamOverride* ov)
{
    if (ov != nullptr && ov->active.load())
    {
        if (ov->hasCurve.load())
            return rangeMin + ov->curve.sample (stepFraction) * (rangeMax - rangeMin);
        return ov->value.load();
    }
    if (const auto* curve = pattern.findCurve (paramId))
        return rangeMin + curve->sample (stepFraction) * (rangeMax - rangeMin);
    return -1.0f;
}

// One convenience overload per instrument, so call sites read `resolve
// (pattern, BassParamID::Drive, ...)` instead of casting to int at every
// call - paramId still just becomes a bare int the moment it reaches
// resolveParam/the override map, per-instrument enums exist only for
// call-site readability, not type safety across instruments (see
// ParamID.h).
float resolve (const StepPattern& p, BassParamID id, float f, float lo, float hi, const AudioEngine::ParamOverride* ov)  { return resolveParam (p, (int) id, f, lo, hi, ov); }
float resolve (const StepPattern& p, KickParamID id, float f, float lo, float hi, const AudioEngine::ParamOverride* ov)  { return resolveParam (p, (int) id, f, lo, hi, ov); }
float resolve (const StepPattern& p, SkankParamID id, float f, float lo, float hi, const AudioEngine::ParamOverride* ov) { return resolveParam (p, (int) id, f, lo, hi, ov); }
float resolve (const StepPattern& p, SnareParamID id, float f, float lo, float hi, const AudioEngine::ParamOverride* ov) { return resolveParam (p, (int) id, f, lo, hi, ov); }
float resolve (const StepPattern& p, HiHatParamID id, float f, float lo, float hi, const AudioEngine::ParamOverride* ov) { return resolveParam (p, (int) id, f, lo, hi, ov); }

// Builds the full set of resolved overrides for one trigger, per
// instrument. Ranges here must match the sliders' ranges in
// MainComponent.cpp exactly - it's what a curve's normalized 0-1 points
// actually mean for this parameter. stepFraction is the position within
// the pattern (0-1); manual/preview triggers that have no real step
// position pass 0.0 (start of the curve).
BassVoicingOverrides resolveBassVoicingOverrides (const StepPattern& pattern, const AudioEngine::GlobalPattern& gp, float stepFraction)
{
    using AE = AudioEngine;
    BassVoicingOverrides overrides;
    overrides.tuneHz               = resolve (pattern, BassParamID::Tune,               stepFraction, 30.0f,  120.0f,  AE::findOverride (gp.bassOverrides, (int) BassParamID::Tune));
    overrides.drive                = resolve (pattern, BassParamID::Drive,              stepFraction, 0.0f,   1.0f,    AE::findOverride (gp.bassOverrides, (int) BassParamID::Drive));
    overrides.cutoffHz             = resolve (pattern, BassParamID::Cutoff,             stepFraction, 100.0f, 4000.0f, AE::findOverride (gp.bassOverrides, (int) BassParamID::Cutoff));
    overrides.resonance            = resolve (pattern, BassParamID::Resonance,          stepFraction, 0.0f,   0.95f,   AE::findOverride (gp.bassOverrides, (int) BassParamID::Resonance));
    overrides.decayMs              = resolve (pattern, BassParamID::Length,             stepFraction, 50.0f,  4000.0f, AE::findOverride (gp.bassOverrides, (int) BassParamID::Length));
    overrides.amDepth              = resolve (pattern, BassParamID::AmDepth,            stepFraction, 0.0f,   1.0f,    AE::findOverride (gp.bassOverrides, (int) BassParamID::AmDepth));
    overrides.pitchEnvAmount       = resolve (pattern, BassParamID::PitchEnvAmount,     stepFraction, 0.0f,   12.0f,   AE::findOverride (gp.bassOverrides, (int) BassParamID::PitchEnvAmount));
    overrides.pitchEnvDecayMs      = resolve (pattern, BassParamID::PitchEnvDecay,      stepFraction, 10.0f,  300.0f,  AE::findOverride (gp.bassOverrides, (int) BassParamID::PitchEnvDecay));
    overrides.filterEnvAmount      = resolve (pattern, BassParamID::FilterEnvAmount,    stepFraction, 0.0f,   3000.0f, AE::findOverride (gp.bassOverrides, (int) BassParamID::FilterEnvAmount));
    overrides.filterEnvDecayMs     = resolve (pattern, BassParamID::FilterEnvDecay,     stepFraction, 10.0f,  300.0f,  AE::findOverride (gp.bassOverrides, (int) BassParamID::FilterEnvDecay));
    overrides.driveTransientAmount = resolve (pattern, BassParamID::DriveTransientAmount, stepFraction, 0.0f, 1.0f,    AE::findOverride (gp.bassOverrides, (int) BassParamID::DriveTransientAmount));
    return overrides;
}

KickVoicingOverrides resolveKickVoicingOverrides (const StepPattern& pattern, const AudioEngine::GlobalPattern& gp, float stepFraction)
{
    using AE = AudioEngine;
    KickVoicingOverrides overrides;
    overrides.tuneHz  = resolve (pattern, KickParamID::Tune,  stepFraction, 30.0f, 150.0f, AE::findOverride (gp.kickOverrides, (int) KickParamID::Tune));
    overrides.punchMs = resolve (pattern, KickParamID::Punch, stepFraction, 5.0f,  120.0f, AE::findOverride (gp.kickOverrides, (int) KickParamID::Punch));
    overrides.decayMs = resolve (pattern, KickParamID::Decay, stepFraction, 50.0f, 800.0f, AE::findOverride (gp.kickOverrides, (int) KickParamID::Decay));
    overrides.drive   = resolve (pattern, KickParamID::Drive, stepFraction, 0.0f,  1.0f,   AE::findOverride (gp.kickOverrides, (int) KickParamID::Drive));
    return overrides;
}

SkankVoicingOverrides resolveSkankVoicingOverrides (const StepPattern& pattern, const AudioEngine::GlobalPattern& gp, float stepFraction)
{
    using AE = AudioEngine;
    SkankVoicingOverrides overrides;
    overrides.tuneHz  = resolve (pattern, SkankParamID::Tune,  stepFraction, 200.0f, 800.0f, AE::findOverride (gp.skankOverrides, (int) SkankParamID::Tune));
    overrides.decayMs = resolve (pattern, SkankParamID::Decay, stepFraction, 30.0f,  500.0f, AE::findOverride (gp.skankOverrides, (int) SkankParamID::Decay));
    overrides.drive   = resolve (pattern, SkankParamID::Drive, stepFraction, 0.0f,   1.0f,   AE::findOverride (gp.skankOverrides, (int) SkankParamID::Drive));
    return overrides;
}

SnareVoicingOverrides resolveSnareVoicingOverrides (const StepPattern& pattern, const AudioEngine::GlobalPattern& gp, float stepFraction)
{
    using AE = AudioEngine;
    SnareVoicingOverrides overrides;
    overrides.tuneHz    = resolve (pattern, SnareParamID::Tune,      stepFraction, 50.0f,  500.0f,  AE::findOverride (gp.snareOverrides, (int) SnareParamID::Tune));
    overrides.noiseMix  = resolve (pattern, SnareParamID::NoiseMix,  stepFraction, 0.0f,   1.0f,    AE::findOverride (gp.snareOverrides, (int) SnareParamID::NoiseMix));
    overrides.cutoffHz  = resolve (pattern, SnareParamID::Cutoff,    stepFraction, 200.0f, 8000.0f, AE::findOverride (gp.snareOverrides, (int) SnareParamID::Cutoff));
    overrides.resonance = resolve (pattern, SnareParamID::Resonance, stepFraction, 0.0f,   0.95f,   AE::findOverride (gp.snareOverrides, (int) SnareParamID::Resonance));
    overrides.decayMs   = resolve (pattern, SnareParamID::Decay,     stepFraction, 30.0f,  500.0f,  AE::findOverride (gp.snareOverrides, (int) SnareParamID::Decay));
    overrides.drive     = resolve (pattern, SnareParamID::Drive,     stepFraction, 0.0f,   1.0f,    AE::findOverride (gp.snareOverrides, (int) SnareParamID::Drive));
    return overrides;
}

HiHatVoicingOverrides resolveHiHatVoicingOverrides (const StepPattern& pattern, const AudioEngine::GlobalPattern& gp, float stepFraction)
{
    using AE = AudioEngine;
    HiHatVoicingOverrides overrides;
    overrides.cutoffHz  = resolve (pattern, HiHatParamID::Cutoff,    stepFraction, 2000.0f, 14000.0f, AE::findOverride (gp.hihatOverrides, (int) HiHatParamID::Cutoff));
    overrides.resonance = resolve (pattern, HiHatParamID::Resonance, stepFraction, 0.0f,    0.95f,    AE::findOverride (gp.hihatOverrides, (int) HiHatParamID::Resonance));
    overrides.decayMs   = resolve (pattern, HiHatParamID::Decay,     stepFraction, 20.0f,   400.0f,   AE::findOverride (gp.hihatOverrides, (int) HiHatParamID::Decay));
    overrides.drive     = resolve (pattern, HiHatParamID::Drive,     stepFraction, 0.0f,    1.0f,     AE::findOverride (gp.hihatOverrides, (int) HiHatParamID::Drive));
    return overrides;
}
}
AudioEngine::AudioEngine()
{
    kickBank.reserve ((size_t) bankSize);
    bassBank.reserve ((size_t) bankSize);
    skankBank.reserve ((size_t) bankSize);
    snareBank.reserve ((size_t) bankSize);
    hihatBank.reserve ((size_t) bankSize);

    for (int i = 0; i < bankSize; ++i)
    {
        kickBank.emplace_back (numSteps);
        bassBank.emplace_back (StepPattern::maxLength);
        skankBank.emplace_back (StepPattern::maxLength);
        snareBank.emplace_back (numSteps);
        hihatBank.emplace_back (numSteps);
    }
}

void AudioEngine::prepare (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    kick.prepare (sampleRate);
    bass.prepare (sampleRate);
    skank.prepare (sampleRate);
    snare.prepare (sampleRate);
    hihat.prepare (sampleRate);
    delay.prepare (sampleRate);
}

void AudioEngine::play() { pendingCommand.store (TransportCmd::Play); }
void AudioEngine::stop() { pendingCommand.store (TransportCmd::Stop); }

void AudioEngine::resetToDefaults()
{
    stop();
    tempoBpm.store (120.0);

    for (auto& p : kickBank)
    {
        p.clearAll();
        p.setActiveLength (numSteps);
    }
    for (auto& p : bassBank)
    {
        p.clearAll();
        p.setActiveLength (StepPattern::maxLength);
    }
    for (auto& s : skankBank)
    {
        s.steps.clearAll();
        s.steps.setActiveLength (StepPattern::maxLength);
    }
    for (auto& p : snareBank)
    {
        p.clearAll();
        p.setActiveLength (numSteps);
    }
    for (auto& p : hihatBank)
    {
        p.clearAll();
        p.setActiveLength (numSteps);
    }
    delayPattern.clearAll();
    delayPattern.setActiveLength (numSteps);

    currentKickIndex.store (0);
    currentBassIndex.store (0);
    currentSkankIndex.store (0);
    currentSnareIndex.store (0);
    currentHiHatIndex.store (0);
    currentGlobalPatternSlot.store (0);

    for (auto& gp : globalPatterns)
        gp.reset();

    kick.resetToDefaults();
    bass.resetToDefaults();
    skank.resetToDefaults();
    snare.resetToDefaults();
    hihat.resetToDefaults();
    delay.resetToDefaults();
    // old audio energy must never linger into a freshly reset/loaded
    // project - see DubDelay::clearBuffer's comment
    delay.clearBuffer();
}

void AudioEngine::setTempoBpm (double bpm)
{
    tempoBpm.store (juce::jlimit (40.0, 220.0, bpm));
}

double AudioEngine::samplesPerStep() const
{
    const double stepsPerBeat = 4.0; // 16th notes
    const double stepsPerSecond = (tempoBpm.load() / 60.0) * stepsPerBeat;
    return sampleRate / stepsPerSecond;
}

void AudioEngine::advanceStep()
{
    // cycle length is whichever pattern is currently longer; all lengths
    // are always powers of two from {4,16,32,64}, so the shortest ones
    // divide the longest exactly and they stay in phase
    int cycleLength = juce::jmax (kickPattern().getActiveLength(),
                                  juce::jmax (bassPattern().getActiveLength(),
                                  juce::jmax (skankPattern().getActiveLength(),
                                  juce::jmax (snarePattern().getActiveLength(), hihatPattern().getActiveLength()))));
    globalStep = (globalStep + 1) % cycleLength;
    globalStepAtomic.store (globalStep);

    const auto& gp = globalPatterns[(size_t) currentGlobalPatternSlot.load()];

    int kickLen = kickPattern().getActiveLength();
    int kickStep = globalStep % kickLen;
    if (kickPattern().isOn (kickStep))
    {
        float kickFraction = kickLen > 1 ? (float) kickStep / (float) (kickLen - 1) : 0.0f;
        auto kickOverrides = resolveKickVoicingOverrides (kickPattern(), gp, kickFraction);
        kick.trigger (kickPattern().getSemitoneOffset (kickStep), stepLevelGain (kickPattern().getLevel (kickStep)), kickOverrides);
    }

    int snareLen = snarePattern().getActiveLength();
    int snareStep = globalStep % snareLen;
    if (snarePattern().isOn (snareStep))
    {
        float snareFraction = snareLen > 1 ? (float) snareStep / (float) (snareLen - 1) : 0.0f;
        auto snareOverrides = resolveSnareVoicingOverrides (snarePattern(), gp, snareFraction);
        snare.trigger (stepLevelGain (snarePattern().getLevel (snareStep)), snareOverrides);
    }

    int hihatLen = hihatPattern().getActiveLength();
    int hihatStep = globalStep % hihatLen;
    if (hihatPattern().isOn (hihatStep))
    {
        float hihatFraction = hihatLen > 1 ? (float) hihatStep / (float) (hihatLen - 1) : 0.0f;
        auto hihatOverrides = resolveHiHatVoicingOverrides (hihatPattern(), gp, hihatFraction);
        hihat.trigger (stepLevelGain (hihatPattern().getLevel (hihatStep)), hihatOverrides);
    }

    int bassLen = bassPattern().getActiveLength();
    int bassStep = globalStep % bassLen;
    if (bassPattern().isOn (bassStep))
    {
        float bassFraction = bassLen > 1 ? (float) bassStep / (float) (bassLen - 1) : 0.0f;
        auto overrides = resolveBassVoicingOverrides (bassPattern(), gp, bassFraction);
        bass.trigger (bassPattern().getSemitoneOffset (bassStep), stepLevelGain (bassPattern().getLevel (bassStep)), overrides);
    }

    int skankLen = skankPattern().getActiveLength();
    int skankStep = globalStep % skankLen;
    if (skankPattern().isOn (skankStep))
    {
        float fraction = skankLen > 1 ? (float) skankStep / (float) (skankLen - 1) : 0.0f;
        auto skankOverrides = resolveSkankVoicingOverrides (skankPattern(), gp, fraction);
        // per-step chord quality - always explicit for sequenced notes,
        // never falls back to the global knob (see SkankPatternSlot::chordIsMinor)
        int minorOverride = skankPatternSlot().getChordIsMinor (skankStep) ? 1 : 0;
        float sawMix = resolve (skankPattern(), SkankParamID::SawMix, fraction, 0.0f, 1.0f,
                                 AudioEngine::findOverride (gp.skankOverrides, (int) SkankParamID::SawMix));
        skank.triggerChord (skankPattern().getSemitoneOffset (skankStep), stepLevelGain (skankPattern().getLevel (skankStep)),
                             sawMix, skankOverrides, minorOverride);
    }
}

// Delay Send - resolved fresh every render block regardless of play
// state (not just on-steps, not snapshotted per-note - a continuous
// mix-routing scalar, same granularity Volume already has), so it's
// always current even before Play has ever been pressed (manual Trigger
// preview must respect a just-moved Delay Send slider immediately, not
// only after the first sequencer step) - see ParamID.h's comment on why
// this is still curve/override-capable despite that.
void AudioEngine::resolveDelaySends()
{
    const auto& gp = globalPatterns[(size_t) currentGlobalPatternSlot.load()];
    int step = globalStepAtomic.load();

    auto resolveSend = [step] (const StepPattern& pattern, int paramId, int len, const ParamOverride* ov, float baseValue) -> float
    {
        float fraction = len > 1 ? (float) (step % len) / (float) (len - 1) : 0.0f;
        float send = resolveParam (pattern, paramId, fraction, 0.0f, 1.0f, ov);
        return send >= 0.0f ? send : baseValue;
    };

    resolvedKickSend.store (resolveSend (kickPattern(), (int) KickParamID::DelaySend, kickPattern().getActiveLength(),
                                          findOverride (gp.kickOverrides, (int) KickParamID::DelaySend), kick.delaySend.load()));
    resolvedBassSend.store (resolveSend (bassPattern(), (int) BassParamID::DelaySend, bassPattern().getActiveLength(),
                                          findOverride (gp.bassOverrides, (int) BassParamID::DelaySend), bass.delaySend.load()));
    resolvedSkankSend.store (resolveSend (skankPattern(), (int) SkankParamID::DelaySend, skankPattern().getActiveLength(),
                                           findOverride (gp.skankOverrides, (int) SkankParamID::DelaySend), skank.delaySend.load()));
    resolvedSnareSend.store (resolveSend (snarePattern(), (int) SnareParamID::DelaySend, snarePattern().getActiveLength(),
                                           findOverride (gp.snareOverrides, (int) SnareParamID::DelaySend), snare.delaySend.load()));
    resolvedHihatSend.store (resolveSend (hihatPattern(), (int) HiHatParamID::DelaySend, hihatPattern().getActiveLength(),
                                           findOverride (gp.hihatOverrides, (int) HiHatParamID::DelaySend), hihat.delaySend.load()));
}

// Feedback/Tone/Drive/Wet, resolved fresh every render block against
// delayPattern's own Length, sampled by the GLOBAL step counter (Delay
// has no step position of its own - see delayPattern's comment in
// AudioEngine.h). Time is deliberately absent - see ParamID.h's
// DelayParamID comment.
void AudioEngine::resolveDelayParams (float& feedbackOut, float& toneOut, float& driveOut, float& wetOut)
{
    const auto& gp = globalPatterns[(size_t) currentGlobalPatternSlot.load()];
    int len = delayPattern.getActiveLength();
    int step = globalStepAtomic.load();
    float fraction = len > 1 ? (float) (step % len) / (float) (len - 1) : 0.0f;

    auto resolve1 = [&] (DelayParamID id, float lo, float hi, const ParamOverride* ov, float baseValue) -> float
    {
        float v = resolveParam (delayPattern, (int) id, fraction, lo, hi, ov);
        return v >= 0.0f ? v : baseValue;
    };

    feedbackOut = resolve1 (DelayParamID::Feedback, 0.0f, DubDelay::maxFeedback, findOverride (gp.delayOverrides, (int) DelayParamID::Feedback), delay.feedback.load());
    toneOut     = resolve1 (DelayParamID::Tone,     200.0f, 8000.0f,             findOverride (gp.delayOverrides, (int) DelayParamID::Tone),     delay.toneHz.load());
    driveOut    = resolve1 (DelayParamID::Drive,    0.0f,   1.0f,                findOverride (gp.delayOverrides, (int) DelayParamID::Drive),    delay.drive.load());
    wetOut      = resolve1 (DelayParamID::Wet,      0.0f,   1.0f,                findOverride (gp.delayOverrides, (int) DelayParamID::Wet),      delay.wet.load());
}

void AudioEngine::renderInstrumentsAndDelay (float* out, int numSamples)
{
    // lazily sized to whatever numSamples turns out to be - JUCE doesn't
    // guarantee every callback matches the size announced in prepare()
    if ((int) kickScratch.size() < numSamples)
    {
        kickScratch.assign ((size_t) numSamples, 0.0f);
        bassScratch.assign ((size_t) numSamples, 0.0f);
        skankScratch.assign ((size_t) numSamples, 0.0f);
        snareScratch.assign ((size_t) numSamples, 0.0f);
        hihatScratch.assign ((size_t) numSamples, 0.0f);
        sendScratch.assign ((size_t) numSamples, 0.0f);
    }
    else
    {
        std::fill (kickScratch.begin(), kickScratch.begin() + numSamples, 0.0f);
        std::fill (bassScratch.begin(), bassScratch.begin() + numSamples, 0.0f);
        std::fill (skankScratch.begin(), skankScratch.begin() + numSamples, 0.0f);
        std::fill (snareScratch.begin(), snareScratch.begin() + numSamples, 0.0f);
        std::fill (hihatScratch.begin(), hihatScratch.begin() + numSamples, 0.0f);
    }

    // each instrument renders into its OWN buffer first, rather than
    // straight into the shared mix, so the DelaySend weighting below
    // never leaks into the dry mix itself
    kick.renderAdd (kickScratch.data(), numSamples);
    bass.renderAdd (bassScratch.data(), numSamples);
    skank.renderAdd (skankScratch.data(), numSamples);
    snare.renderAdd (snareScratch.data(), numSamples);
    hihat.renderAdd (hihatScratch.data(), numSamples);

    // instrument-level sends - "which instruments feed the delay," see
    // DELAY_FEEDBACK_LOOP_ANALYSIS.txt and each synth's delaySend field.
    // Curve/override-capable now (see ParamID.h's comment on Delay Send)
    // - resolvedXSend is kept fresh every STEP by advanceStep(), read
    // once per block here, not per sample, matching every other
    // instrument's snapshot convention.
    float kickSend = resolvedKickSend.load();
    float bassSend = resolvedBassSend.load();
    float skankSend = resolvedSkankSend.load();
    float snareSend = resolvedSnareSend.load();
    float hihatSend = resolvedHihatSend.load();

    for (int i = 0; i < numSamples; ++i)
    {
        out[i] += kickScratch[(size_t) i] + bassScratch[(size_t) i] + skankScratch[(size_t) i]
                + snareScratch[(size_t) i] + hihatScratch[(size_t) i];
        sendScratch[(size_t) i] = kickScratch[(size_t) i] * kickSend
                                 + bassScratch[(size_t) i] * bassSend
                                 + skankScratch[(size_t) i] * skankSend
                                 + snareScratch[(size_t) i] * snareSend
                                 + hihatScratch[(size_t) i] * hihatSend;
    }

    float delayFeedback, delayTone, delayDrive, delayWet;
    resolveDelayParams (delayFeedback, delayTone, delayDrive, delayWet);
    delay.process (out, sendScratch.data(), numSamples, delayFeedback, delayTone, delayDrive, delayWet);
}

void AudioEngine::renderNextBlock (juce::AudioBuffer<float>& buffer, int numSamples)
{
    buffer.clear();

    // always current, whether playing, stopped, or freshly launched -
    // see resolveDelaySends' own comment.
    resolveDelaySends();

    // Transport start/stop is only ever applied here, on the audio thread,
    // so globalStep / samplePositionInStep never need cross-thread locking.
    auto cmd = pendingCommand.exchange (TransportCmd::None);
    if (cmd == TransportCmd::Play)
    {
        globalStep = -1;
        samplePositionInStep = 0.0;
        playing.store (true);
        advanceStep();
    }
    else if (cmd == TransportCmd::Stop)
    {
        playing.store (false);
        globalStep = 0;
        globalStepAtomic.store (0);
        samplePositionInStep = 0.0;
    }

    // manual Trigger buttons should sound like the sequenced note would -
    // same curve/override resolution as advanceStep, sampled at fraction
    // 0.0 (start of the curve) since a manual trigger has no real step
    // position.
    const auto& manualGp = globalPatterns[(size_t) currentGlobalPatternSlot.load()];

    if (manualKickTriggerRequested.exchange (false))
    {
        auto kickOverrides = resolveKickVoicingOverrides (kickPattern(), manualGp, 0.0f);
        kick.trigger (0, stepLevelGain (StepLevel::Normal), kickOverrides);
    }

    if (manualBassTriggerRequested.exchange (false))
    {
        auto overrides = resolveBassVoicingOverrides (bassPattern(), manualGp, 0.0f);
        bass.trigger (0, stepLevelGain (StepLevel::Normal), overrides);
    }

    if (manualSkankTriggerRequested.exchange (false))
    {
        auto skankOverrides = resolveSkankVoicingOverrides (skankPattern(), manualGp, 0.0f);
        float sawMix = resolve (skankPattern(), SkankParamID::SawMix, 0.0f, 0.0f, 1.0f,
                                 AudioEngine::findOverride (manualGp.skankOverrides, (int) SkankParamID::SawMix));
        skank.triggerChord (0, stepLevelGain (StepLevel::Normal), sawMix, skankOverrides);
    }

    if (manualSnareTriggerRequested.exchange (false))
    {
        auto snareOverrides = resolveSnareVoicingOverrides (snarePattern(), manualGp, 0.0f);
        snare.trigger (stepLevelGain (StepLevel::Normal), snareOverrides);
    }

    if (manualHiHatTriggerRequested.exchange (false))
    {
        auto hihatOverrides = resolveHiHatVoicingOverrides (hihatPattern(), manualGp, 0.0f);
        hihat.trigger (stepLevelGain (StepLevel::Normal), hihatOverrides);
    }

    auto* out = buffer.getWritePointer (0);

    if (playing.load())
    {
        int offset = 0;
        int remaining = numSamples;
        const double spStep = samplesPerStep();

        while (remaining > 0)
        {
            int samplesUntilNextStep = juce::jmax (1, (int) std::ceil (spStep - samplePositionInStep));
            int chunk = juce::jmin (remaining, samplesUntilNextStep);

            renderInstrumentsAndDelay (out + offset, chunk);

            samplePositionInStep += chunk;
            offset += chunk;
            remaining -= chunk;

            if (samplePositionInStep >= spStep - 0.0001)
            {
                samplePositionInStep -= spStep;
                if (samplePositionInStep < 0.0)
                    samplePositionInStep = 0.0;
                advanceStep();
            }
        }
    }
    else
    {
        // let a manually-triggered hit ring out even while the transport is stopped
        renderInstrumentsAndDelay (out, numSamples);
    }

    for (int i = 0; i < numSamples; ++i)
        out[i] = (float) std::tanh (out[i] * 0.9);

    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}
}
