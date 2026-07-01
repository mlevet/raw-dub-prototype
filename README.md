# Raw Dub Prototype

A standalone Mac instrument for composing dub tracks quickly. Not a DAW: no
plugin formats, no project/file system beyond a future single "Track Idea"
JSON save, no menu diving. Every feature is judged against one question:
*does this help finish a dub track?*

Full vision, references, and constraints philosophy are described in
project memory and prior design conversation; this README tracks what
actually exists in code right now.

## Status

Early prototype. Working through the project's own priority order:

1. **Low end (Kick + Bass) — in progress.** Both voices exist and are
   playable; currently in a deliberate listening-and-tuning phase before
   moving on (see "Bass synthesis" below).
2. Delay — not started.
3. Reverb — not started.
4. Snare, Hat, other percussion — not started.
5. Pattern variations / arrangement — not started.

## What's built

- **Standalone JUCE app** (`AudioAppComponent`, not a plugin) with a single
  window: transport, a 16-step sequencer, and per-voice controls. Black and
  white throughout, on purpose.
- **Transport & clock**: sample-accurate 16th-note stepping. All transport
  state changes (play/stop) are posted as atomic commands from the GUI
  thread and applied only on the audio thread - no locks.
- **Uniform step model** (`StepPattern`): every step has on/off, a musical
  **level** (Ghost / Normal / Accent - not raw MIDI velocity), and a pitch
  offset. The same shape is used for every voice; what differs per voice is
  how much of it the UI exposes.
- **Shared step widget** (`StepButton`): click to activate, horizontal drag
  to cycle Ghost/Normal/Accent, vertical drag to bend pitch (only on voices
  where pitch means something - see below). Accent affects level *and*
  drives the synthesis harder pre-saturation, so it's real harmonic weight,
  not just volume.
- **KickSynth**: sine oscillator + exponential pitch envelope + exponential
  amp decay + saturation (Drive). No pitch editing exposed (rhythm role,
  not melodic).
- **BassSynth**: sawtooth oscillator -> saturation (Drive) -> resonant
  lowpass filter -> pluck-style envelope. Pitch is exposed and **scale-
  locked** to natural minor degrees relative to the voice's own Tune (see
  `Source/Scale.h`) rather than free chromatic - continuous chromatic
  pitch made basslines unreadable as a shape at a glance.
- **Bass synthesis is tuned toward early UK digidub character**, not
  generic electronic bass: filter defaults keep the fundamental intact
  (higher cutoff, low resonance - the filter is tone colour, not the main
  expressive gesture), and output gain is deliberately kept under the
  synth's own internal headroom so the master limiter doesn't flatten
  dynamics before they're heard.

## Deliberately not built yet

Preset browser, project/file management, plugin formats, MIDI learn,
automation lanes, modulation matrix, scripting, AI generation, complex
routing, portamento/glide, octave-range handling on the pitch UI, a
song-wide Key concept (Bass's Tune currently acts as its own root). All of
these are real ideas, just not needed yet - see `MELODIC_EDITOR_DESIGN.txt`
for the reasoning behind several of these deferrals.

## Building

Requires the JUCE checkout at `../DDD1BassTranslator/JUCE` (shared with
sibling projects, not vendored here).

```
cmake -S . -B build
cmake --build build --target RawDubPrototype
```

Output app is at
`build/RawDubPrototype_artefacts/Raw Dub Prototype.app`.

## Further reading

- `IMPLEMENTATION_NOTES.txt` - detailed walkthrough of the original
  kick-only harness (transport/concurrency model, DSP formulas).
- `MELODIC_EDITOR_DESIGN.txt` - the design reasoning behind the melodic
  step editor (scale-locked bar grid, why chromatic pitch and a discrete
  piano-roll were rejected, open sub-problems left for later).
