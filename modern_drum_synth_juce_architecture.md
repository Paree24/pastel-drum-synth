# Modern Analog Drum Synthesizer — JUCE Architecture and Implementation Plan

## 1. Project overview

Build a modern analog-modelled, multi-part drum synthesizer as a JUCE audio plug-in for DnB, dubstep, house, techno, and hybrid electronic percussion. Draw from classic analog drum circuits while extending them with modern control, tuning, modulation, nonlinear processing, randomization, and mutation. This is not a strict vintage emulation: model useful behavior of analog-style oscillators, noise sources, resonant filters, envelopes, feedback, and saturation, while allowing extended pitch ranges, fast envelopes, hard transients, deep sub, aggressive drive, and experimental modulation.

### Parts

The initial specification contains 12 instrument types:

1. Kick
2. Snare
3. Closed hi-hat (CHH)
4. Open hi-hat (OHH)
5. Ride
6. Crash
7. Tom 1
8. Tom 2
9. Low Tom
10. Membrane Percussion
11. Metallic Percussion
12. SFX

**Terminology note:** The original request said “102part” but listed 12 types. This plan assumes 12 independently addressable parts. Keep the architecture data-driven so the number of parts can be increased later if 102 parts was intended.

### Core requirements

- Independent synthesis parameters and effects for every part.
- One global/master effects chain.
- Six effect types: four-band EQ, compressor, transient shaper, distortion/saturation, OTT-style multiband dynamics, and reverb/delay.
- Effects can be reordered arbitrarily in each chain.
- Part and global chains use the same processor framework.
- Save/restore complete plug-in state, including effect order.
- Host automation for meaningful parameters.
- Real-time-safe audio processing.
- Presets and a practical, modern UI.
- Per-part and whole-kit randomization with controllable scope and intensity.
- Mutation of an existing sound or kit while preserving its identity and useful musical structure.

## 2. High-level signal flow

```text
Host MIDI / Internal Sequencer (optional)
                 |
          MIDI Event Routing
                 |
       +---------+---------+------------------+
       |         |         |                  |
   Kick Part  Snare Part  ...             SFX Part
       |         |                            |
   Synth Voice Synth Voice                 Synth Voice
       |         |                            |
   Part FX     Part FX                    Part FX
       |         |                            |
       +---------+----------------------------+
                         |
                   Part Mixer
            (gain, pan, mute, solo)
                         |
                Global Effects Chain
                         |
                Master Gain / Meter
                         |
                    Audio Output
```

MIDI routing should map incoming notes to parts through configurable note assignments. A built-in step sequencer is optional and should not block the initial synthesis/effects implementation.

## 3. Architecture principles

1. **Separate DSP from UI.** The audio engine must not depend on editor components.
2. **Use shared DSP building blocks.** Envelopes, oscillators, noise generators, filters, resonators, and saturation primitives should be reused where appropriate.
3. **Use specialized analog-modelled voice engines.** A kick should model an analog-style oscillator, pitch envelope, noise click, and nonlinear body path; cymbals and membrane percussion need suitable resonator and noise structures.
4. **Design for analog character, not analog limitations.** Add controlled drift, component variation, oscillator instability, and nonlinear behavior as optional parameters. Keep tuning stable and repeatable when desired.
5. **Use one effect interface.** Every effect should fit into the same chain API and be usable in both part and global chains.
6. **Make chains data-driven.** Each chain is an ordered list of effect instances, not a fixed set of six slots.
7. **Keep the audio thread real-time safe.** Avoid memory allocation, blocking locks, file I/O, and unpredictable work in `processBlock()`.
8. **Prepare before processing.** Allocate buffers and initialize DSP state in `prepareToPlay()` or in an off-audio-thread preparation step.
9. **Make state serializable.** Presets must reproduce the sound, chain order, parameter values, routing, and part configuration.
10. **Make randomization reproducible.** Use a seed that can be stored with undo history or preset metadata when repeatability matters.

## 4. Suggested source tree

```text
Source/
  PluginProcessor.h
  PluginProcessor.cpp
  PluginEditor.h
  PluginEditor.cpp

  Parameters/
    ParameterLayout.h
    ParameterLayout.cpp
    ParameterIDs.h
    StateSerializer.h
    StateSerializer.cpp

  Engine/
    DrumPart.h
    DrumPart.cpp
    DrumVoice.h
    DrumVoice.cpp
    VoiceAllocator.h
    VoiceAllocator.cpp
    DrumMixer.h
    DrumMixer.cpp
    MidiRouter.h
    MidiRouter.cpp

  Synthesis/
    KickSynth.h
    KickSynth.cpp
    SnareSynth.h
    SnareSynth.cpp
    CymbalSynth.h
    CymbalSynth.cpp
    TomSynth.h
    TomSynth.cpp
    MembraneSynth.h
    MembraneSynth.cpp
    MetallicSynth.h
    MetallicSynth.cpp
    SfxSynth.h
    SfxSynth.cpp

  DSP/
    Envelope.h/.cpp
    AnalogOscillator.h/.cpp
    AnalogNoise.h/.cpp
    AnalogFilter.h/.cpp
    ResonatorBank.h/.cpp
    NonlinearCircuit.h/.cpp
    DriftModel.h/.cpp
    Saturator.h/.cpp
    OversamplingManager.h
    OversamplingManager.cpp

  Effects/
    EffectProcessor.h
    EffectChain.h
    EffectChain.cpp
    EffectFactory.h
    EffectFactory.cpp
    FourBandEQ.h
    FourBandEQ.cpp
    Compressor.h
    Compressor.cpp
    TransientShaper.h
    TransientShaper.cpp
    Distortion.h
    Distortion.cpp
    OTTProcessor.h
    OTTProcessor.cpp
    ReverbDelay.h
    ReverbDelay.cpp

  Presets/
    PresetManager.h
    PresetManager.cpp

  Randomization/
    RandomizationEngine.h/.cpp
    RandomizationProfile.h
    MutationStrategy.h/.cpp
    ParameterMetadata.h
    VariationSet.h/.cpp

  UI/
    MainView.h
    MainView.cpp
    PartSelector.h
    PartSelector.cpp
    PartSynthEditor.h
    PartSynthEditor.cpp
    EffectsRack.h
    EffectsRack.cpp
    EffectEditor.h
    EffectEditor.cpp
    MixerView.h
    MixerView.cpp
    GlobalEffectsView.h
    GlobalEffectsView.cpp
```

This is a logical starting point, not a requirement to create every file immediately. Keep the first working implementation small and split modules as they become substantial.

## 5. Core classes and responsibilities

### `DrumSynthAudioProcessor`

Responsibilities:

- JUCE plug-in lifecycle and bus layout.
- Receive MIDI and route events to parts.
- Own the part bank, mixer, global effect chain, and master output stage.
- Prepare DSP for the current sample rate and block size.
- Process audio without blocking or allocating.
- Expose host parameters and plug-in state.
- Serialize/restore state and presets.

Use `AudioProcessorValueTreeState` (APVTS) for host-automatable parameters. A custom state tree may be needed for dynamic effect-chain structure and per-instance effect state; keep it integrated with the plug-in's state serialization.

### `DrumPart`

Responsibilities:

- Part identity and instrument type.
- Part-specific synthesis engine and parameter state.
- Voice allocation and note handling.
- Per-part effect chain.
- Gain, pan, mute, solo, velocity response, MIDI note mapping, and choke group.
- Render the part to a supplied buffer.

The part should not own UI objects.

### `DrumVoice` and voice allocation

Responsibilities:

- Note-on/note-off lifecycle.
- Render one active voice.
- Track voice age and active state.
- Apply velocity and modulation.
- Handle voice stealing deterministically when polyphony is exceeded.

Use part-appropriate polyphony. For example, a kick may often use monophonic or low-polyphony behavior with optional tail/choke control, while SFX may need more voices.

### Analog-modelled synthesis engines

Use specialized engines built from shared analog-style circuit primitives rather than forcing every sound into one generic model. Prefer stable, controllable digital models of familiar circuit behavior over blindly adding noise or random detuning.

| Engine | Suggested building blocks |
|---|---|
| Kick | Analog-style VCO or sine/triangle core, fast pitch-decay envelope, trigger click/noise, resonant body, optional sub path and nonlinear output stage |
| Snare | Tuned oscillators, analog noise source, resonant snare/noise filter, optional clap/noise burst and saturation |
| CHH/OHH | Inharmonic oscillator cores or metallic resonators, noise source, contour filter, short/long decay envelopes |
| Ride/Crash | Dense metallic oscillator/resonator network, noise excitation, multiple decay paths, optional nonlinear feedback |
| Toms | Oscillator core with pitch envelope, resonant filter/body, noise attack, decay shaping |
| Membrane percussion | Coupled resonators or modal model with membrane-like pitch and damping behavior |
| Metallic percussion | Inharmonic resonator bank, cross-modulation/FM, noise excitation, feedback and drive controls |
| SFX | Analog-style oscillator/noise/filter/modulation building blocks, feedback, safe self-oscillation, optional sample layer |

For imported samples or user sample layers, keep file loading and decoding outside the audio callback. Publish prepared sample data to the audio thread using a safe ownership strategy.

## 6. Modern analog drum sound design requirements

### Kick

- Pitch envelope with adjustable start pitch, end pitch, curve, and time.
- Separate sub, body, and click layers.
- Control of oscillator phase/reset and layer phase relationships.
- Independent envelopes and level for each layer.
- Optional saturation/clipping in the voice.
- Tail/decay shaping and tuning controls.

### Snare

- Tuned body with one or more oscillators.
- Adjustable noise spectrum and envelope.
- Snare-wire/resonance control.
- Optional clap or transient layer.
- Separate body, crack, and noise levels.
- Optional internal drive.

### Cymbals

- Inharmonic partials rather than only harmonic oscillator stacks.
- Noise excitation and filtering.
- Multiple decay groups for bright attack and darker tail.
- Brightness, metallic character, stereo width, and damping controls.
- CHH/OHH choke relationship support.

### Analog character controls

Expose optional controls where they suit each voice:

- **Drift:** slow oscillator/filter variation, subtle and bounded.
- **Component tolerance:** small deviations in tuning, cutoff, or decay.
- **Noise character:** amount, color, and trigger consistency.
- **Drive/saturation:** nonlinear circuit behavior at useful gain levels.
- **Per-trigger variation:** repeatable or random variation on each hit.
- **Stability/amount:** one macro that scales analog imperfections from stable to characterful.

Keep these deterministic when repeatability is requested, and do not compromise kick tuning, transient clarity, or preset recall.

### All parts

- Velocity can affect more than volume: timbre, pitch, noise amount, attack, or drive.
- Fast envelopes and modulation.
- Tuning and decay controls.
- Deterministic triggering when requested.
- Optional randomization with controllable amount and repeatability.
- Output headroom and sensible internal gain staging.

Add optional analog-character controls per part: oscillator drift, component tolerance, noise amount, saturation, and per-trigger variation. These should default to subtle or off, and be independently seedable for consistent presets. Preserve tuning, punch, repeatability, and modern low-end control.

The goal is a wide dynamic range of sound design, not a permanently distorted signal. Make aggressive settings available while preserving clean operating ranges.

## 7. Effects framework

### Common interface

Define a common processor interface. A simplified conceptual API:

```cpp
class EffectProcessor
{
public:
    virtual ~EffectProcessor() = default;

    virtual void prepare (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void reset() = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) = 0;

    virtual void setBypassed (bool shouldBypass) = 0;
    virtual bool isBypassed() const = 0;

    virtual juce::ValueTree saveState() const = 0;
    virtual void loadState (const juce::ValueTree& state) = 0;

    virtual juce::String getTypeID() const = 0;
};
```

This is a design sketch. Production code should define how parameters are updated safely, how channel layouts are handled, and whether processors support sidechains or latency reporting.

### `EffectChain`

Responsibilities:

- Store an ordered sequence of effect instances.
- Prepare each effect for the current processing specification.
- Process audio through effects in order.
- Support bypass, add, remove, duplicate, and reorder operations.
- Serialize effect type, unique instance ID, order, and parameter state.
- Handle changes without unsafe mutation of the active audio-thread chain.

**Real-time safety warning:** Do not let the GUI directly mutate a `std::vector` of active processors while `processBlock()` iterates over it. Build and prepare replacement chain structures away from the audio thread, then publish/swap them using a robust real-time-safe ownership strategy. Retire old structures only when the audio thread can no longer access them. Avoid ordinary mutexes in the audio callback.

### Effect factory

Use an `EffectFactory` to construct an effect from a stable type ID, such as:

- `eq4`
- `compressor`
- `transient`
- `distortion`
- `ott`
- `reverb_delay`

The factory allows the same effect types to be used in per-part and global chains.

## 8. Effects specifications

### 8.1 Four-band parametric EQ

Each band should provide frequency, gain, and Q, with a selectable filter shape where practical.

Recommended filter shapes:

- Bell/peaking
- Low shelf
- High shelf
- Low-pass
- High-pass

The four-band limit applies to the EQ instance, not to the total number of EQ instances in a chain. Use stable filter designs and smooth coefficient changes to avoid clicks or instability. Validate frequency limits against sample rate and prevent invalid Q values.

### 8.2 Compressor

Parameters:

- Threshold
- Ratio
- Attack
- Release
- Knee
- Makeup gain
- Wet/dry mix
- Optional sidechain source and detector mode

Provide gain-reduction metering. Ensure the detector, envelope follower, and gain computer behave well at different sample rates and block sizes.

### 8.3 Transient shaper

Parameters:

- Attack
- Sustain
- Sensitivity
- Timing/window controls as appropriate
- Output gain
- Wet/dry

A transient shaper is not simply a compressor with different labels. Implement an envelope/difference-based design or a suitable dual-envelope detector. Test for pumping, clicks, and exaggerated noise on cymbal tails.

### 8.4 Distortion / saturation

Possible modes:

- Soft saturation
- Hard clip
- Waveshaping
- Foldback
- Asymmetric saturation

Parameters:

- Drive/input gain
- Tone or pre/post filtering
- Output gain
- Wet/dry
- Oversampling quality

Use oversampling where nonlinear processing creates significant aliasing. Make oversampling quality configurable and profile the CPU cost. Do not oversample every effect by default.

### 8.5 OTT-style multiband dynamics

Implement three bands with configurable crossovers and upward/downward dynamics.

Parameters:

- Depth/mix
- Per-band upward and downward compression amount or equivalent controls
- Attack/release scaling
- Crossover frequencies
- Output gain

Include a depth or wet/dry control so extreme OTT can be blended with the unprocessed signal. Ensure crossover bands reconstruct cleanly and do not create excessive gain or audible discontinuities.

### 8.6 Reverb / Delay

These can share a selectable slot with two modes, or be implemented as two separate effect types if independent instances are desirable. For maximum routing flexibility, separate effect types are preferable:

- `reverb`
- `delay`

If strictly preserving six user-facing effect categories is important, display them under one “Reverb / Delay” category while allowing the slot to choose a mode.

Reverb controls:

- Decay/time
- Size or diffusion
- Pre-delay
- Damping
- Wet/dry
- Stereo width

Delay controls:

- Free or tempo-synced time
- Feedback
- Low-/high-pass filtering
- Ping-pong/stereo mode
- Wet/dry

For delay and reverb, use bounded, preallocated buffers. Avoid reallocating buffers during audio processing.

## 9. Per-part and global routing

Each part has its own effect chain:

```text
Part Synth -> Part FX[0] -> Part FX[1] -> ... -> Part FX[N]
           -> Part Gain/Pan -> Mixer
```

The global chain is applied after summing the parts:

```text
Mixer Sum -> Global FX[0] -> Global FX[1] -> ... -> Global FX[N]
          -> Master Gain/Meter -> Output
```

Use the same effect factory and processor interface for both scopes. Each effect instance must have its own independent state.

Part and global effect order should be independent. For example:

- Kick: EQ -> saturation -> transient shaper
- Snare: transient shaper -> distortion -> EQ -> reverb
- Global: EQ -> compressor -> OTT -> limiter/clipper (if a separate final safety stage is added)

Do not silently add a limiter to the specified six-effect list. A final output safety stage can be an optional separate utility, clearly represented in the UI.

## 10. Parameter and state model

### Stable parameter IDs

Use stable, unique IDs for host-automatable parameters. A naming pattern could be:

```text
part.kick.level
part.kick.pitchStart
part.kick.pitchEnd
part.kick.pitchTime
part.kick.subLevel
part.kick.clickLevel

part.snare.bodyLevel
part.snare.noiseLevel

part.kick.fx.<instance-id>.drive
part.kick.fx.<instance-id>.mix
global.fx.<instance-id>.threshold
```

These are illustrative names. JUCE parameter IDs have versioning and compatibility implications; establish an ID convention early and preserve it after release.

Dynamic effects create a design challenge: the number and order of effect instances can change. Keep the persistent chain structure in a serializable state model, and expose automatable parameters in a way compatible with the chosen JUCE parameter strategy. Decide early whether effect slots are fixed-capacity with selectable types or truly dynamic. Fixed-capacity slots are easier for host automation; dynamic instances offer more flexibility but require careful parameter registration and project recall design.

### State should include

- All synthesis parameters.
- Part types and MIDI note mappings.
- Part gain, pan, mute/solo, velocity behavior, and choke groups.
- Voice/polyphony settings.
- Effect type, instance ID, order, bypass, and parameter values.
- Global effect chain.
- Optional sequencer and modulation state.
- Preset metadata and state schema version.

Use versioned serialization and migration code when the state model changes.

## 11. UI layout

Recommended main view:

1. **Part selector:** 12 pads/buttons with name, mute/solo, and level meter.
2. **Synthesis panel:** controls specific to the selected part.
3. **Effects rack:** ordered effects with drag-and-drop reordering, bypass, remove, duplicate, and add-effect actions.
4. **Effect editor:** detailed controls for the selected effect.
5. **Mixer/global area:** part level/pan and the global effects chain.
6. **Preset browser:** load/save, categories, favorites, and factory presets.

Keep synthesis and effects available without losing the selected part. Use meters for part level, master level, and compressor/OTT gain reduction. Avoid a UI that requires opening many modal windows to perform common sound-design tasks.

### Drag-and-drop effect ordering

The UI should issue a reorder command to the model. The audio engine should not be edited directly from a mouse callback. Chain updates should be prepared safely off the audio thread and committed through the chain-publication mechanism.

## 12. Randomize and mutate system

Randomization is a first-class sound-design feature, not a button that indiscriminately scrambles every parameter. Give each operation clear scope, constraints, and undo support.

### Randomize operations

- **Parameter/group:** randomize one control or a selected group.
- **Part:** generate a new sound for the selected drum part.
- **Kit:** randomize multiple parts, optionally preserving levels, routing, and selected parts.
- **Effects:** randomize effect parameters or create a new chain. Preserve chain order unless explicitly asked to randomize it.
- **Analog character:** vary drift, component tolerances, noise, or per-trigger variation.

Provide a continuous intensity control or Subtle / Medium / Wild presets. Scope toggles should include Synthesis, Envelopes, Modulation, Analog Character, Effects, and Mixer. By default, exclude output gain, master level, MIDI mapping, choke groups, effect-chain structure, and safety-critical settings.

### Mutate operations

Mutation starts from the current sound and makes bounded changes, preserving its identity more often than full randomization.

- **Mutate sound:** perturb selected synthesis parameters around current values.
- **Mutate kit:** mutate parts independently or with a shared intensity.
- **Generate variations:** create several nearby alternatives from one source sound.
- **Lock parameters:** protect pitch, decay, sub level, an effect, or any other chosen parameter.
- **Morph amount:** control how far the result moves from the original.
- **Mutation target:** synthesis, analog character, effects, or a combination.

### Parameter-aware variation

Do not use a uniform random distribution for every parameter. Frequency and time may need logarithmic/multiplicative movement; gains should be adjusted in dB; enums must select valid modes; envelope times must remain within sensible ranges; and related controls should be kept coherent. Clamp and validate all values through the same metadata and constraints used by the UI and host. Optionally compensate output level after a mutation to avoid surprising loudness jumps.

### Suggested module

```text
Randomization/
  RandomizationEngine.h/.cpp
  RandomizationProfile.h
  MutationStrategy.h/.cpp
  ParameterMetadata.h
  VariationSet.h/.cpp
```

Parameter metadata should describe its group, instrument applicability, range, unit, distribution type (linear, logarithmic, dB, discrete, enum), randomization/mutation eligibility, default mutation radius, related parameters, and lock state. Use a deterministic seeded random generator. Apply a validated change-set as one user-visible transaction and support undo/redo. Never run randomization work inside `processBlock()`.

### Simple UI workflow

1. **Randomize** opens a small menu: Part / Kit / Effects, plus intensity.
2. **Mutate** creates a nearby variation of the current sound.
3. A few scope toggles choose Synthesis, Analog Character, Effects, and Mixer.
4. Locked parameters show a small lock icon; keep the main UI uncluttered.
5. Undo, Redo, and A/B comparison make experimentation safe.

Advanced distributions and algorithms belong in a secondary panel, not the main workflow.

## 13. MIDI and performance behavior

- Configurable MIDI note assignment for each part.
- Velocity response curves per part.
- Optional note-off behavior for sustained voices or gated SFX.
- Choke groups, especially for CHH/OHH and related cymbal articulations.
- Predictable voice stealing.
- Optional per-part MIDI channel filtering.
- MIDI learn can be added later.
- Internal sequencer is optional and should be a separate module.

## 14. Performance and audio-thread safety

The audio callback must not:

- Allocate or free memory.
- Load samples or presets from disk.
- Block on a mutex.
- Perform file I/O or network I/O.
- Rebuild effect chains.
- Wait for UI operations.
- Log excessively.

Prepare in advance:

- Voice pools and their buffers.
- Effect processors and scratch buffers.
- Delay/reverb memory.
- Oversampling processors.
- Filter and envelope state.
- Any sample data.

Additional considerations:

- Handle mono/stereo channel layouts deliberately.
- Support host block sizes that vary, including small blocks.
- Test 44.1, 48, 88.2, and 96 kHz (and higher rates if supported).
- Use denormal protection where necessary.
- Smooth continuous parameters.
- Avoid data races when transferring parameter values or chain structures.
- Profile worst-case active voices and maximum effect counts.

## 15. Testing strategy

### Unit tests

- Envelope timing and retrigger behavior.
- Oscillator pitch and phase behavior.
- Filter stability and coefficient updates.
- Distortion output and oversampling behavior.
- Compressor gain reduction.
- OTT crossover/reconstruction behavior.
- Effect-chain order and bypass behavior.
- State save/restore, including chain order and unique instance IDs.
- MIDI note mapping and choke groups.

### Integration tests

- Repeatedly add, remove, duplicate, and reorder effects while audio is running.
- Change sample rate and block size.
- Load old presets after state schema changes.
- Test mono/stereo routing and host automation.
- Test sustained notes, dense MIDI bursts, and voice stealing.
- Check that the plug-in remains stable during rapid UI interaction.

### Listening tests

Build a focused test preset set:

- Deep sub kick
- Punchy house kick
- DnB kick with click and controlled sub
- Aggressive dubstep snare
- Tight clap/snare hybrid
- Short and long hi-hats
- Bright ride and wide crash
- Tuned tom fills
- Metallic and membrane percussion
- Experimental SFX
- Clean, saturated, transient-shaped, and OTT-heavy variants

Loudness-match comparisons when judging processing. Do not use peak level alone as a proxy for punch or quality.

## 16. Recommended implementation phases

### Phase 1 — Plugin foundation

- Create a JUCE audio plug-in project (VST3/AU as needed for target platforms).
- Set up MIDI input and stereo output.
- Establish APVTS parameter conventions.
- Implement state save/restore and basic UI.
- Add a simple test oscillator and audio meter.

**Exit criterion:** the plug-in loads in a host, receives MIDI, generates sound, and restores parameters correctly.

### Phase 2 — First voices

- Implement kick and snare engines.
- Implement voice allocation and note mapping.
- Add velocity and envelopes.
- Add initial factory presets.

**Exit criterion:** kick and snare can produce useful sounds without relying on master processing.

### Phase 3 — Effect infrastructure

- Define `EffectProcessor`.
- Implement `EffectFactory` and `EffectChain`.
- Add safe chain preparation and publication.
- Implement bypass, wet/dry, ordering, and state serialization.
- Start with EQ and distortion as proof of architecture.

**Exit criterion:** arbitrary effect order survives save/reload and remains safe during playback.

### Phase 4 — Remaining effects

- Compressor
- Transient shaper
- OTT-style multiband dynamics
- Reverb
- Delay
- Oversampling support for nonlinear stages

**Exit criterion:** all effects work in both part and global chains and can be reordered independently.

### Phase 5 — Remaining voices

- CHH/OHH
- Ride/Crash
- Tom 1/Tom 2/Low Tom
- Membrane percussion
- Metallic percussion
- SFX

**Exit criterion:** every listed part has a purpose-built or appropriate specialized engine and useful presets.

### Phase 6 — Randomize and mutate

- Add parameter metadata and deterministic seeded randomization.
- Implement part, kit, and effect randomization with scopes and intensity.
- Implement identity-preserving mutation, parameter locks, variation generation, undo/redo, and A/B.
- Test value bounds, parameter relationships, and the usefulness of generated sounds.

**Exit criterion:** randomize and mutate create useful, repeatable variations without changing locked parameters or corrupting state.

### Phase 7 — Full mixer and UI

- Part selector and synthesis panels.
- Effects rack with drag-and-drop.
- Part mixer and global chain.
- Preset browser and meters.

**Exit criterion:** common sound-design tasks are efficient and state recall is reliable.

### Phase 8 — Optimization and release preparation

- Profile worst-case CPU.
- Validate thread safety.
- Test across supported hosts and sample rates.
- Add factory presets and documentation.
- Test plug-in validation and installer/package workflow.

## 17. Key decisions to make early

1. **12 versus 102 parts:** current plan assumes 12 parts corresponding to the listed instrument types.
2. **Dynamic versus fixed effect slots:** dynamic chains are more flexible; fixed-capacity slots simplify stable host automation.
3. **Reverb/delay as one category or two processors:** two processors give better ordering and multiple-instance flexibility.
4. **Sample layering:** recommended as optional, even though synthesis is the core.
5. **Internal sequencer:** optional for the first version; MIDI-triggered operation should work independently.
6. **Supported formats/platforms:** decide target operating systems and plug-in formats before choosing packaging and CI.
7. **Preset compatibility:** design versioned state serialization before shipping presets.

## 18. Definition of done for the first usable version

- 12 independently controllable drum parts.
- Distinct synthesis engines appropriate to each instrument family.
- Per-part and global effect chains.
- Arbitrary effect order, with safe add/remove/reorder behavior.
- Four-band EQ, compressor, transient shaper, distortion/saturation, OTT, reverb, and delay.
- Stable host automation for exposed parameters.
- Reliable preset and project-state recall.
- MIDI mapping, velocity response, and choke groups.
- Part/kit/effect randomization and identity-preserving mutation with locks, intensity, undo/redo, and A/B comparison.
- No audio-thread allocations or blocking locks in normal processing.
- Useful factory presets demonstrating DnB, dubstep, house, techno, and experimental sounds.

The highest-risk engineering areas are dynamic effect-chain changes, host automation for dynamic effect instances, CPU use under oversampling/multiband processing, and parameter-aware mutation that produces useful variations rather than broken sounds. Prove those foundations early, before investing heavily in UI polish.
