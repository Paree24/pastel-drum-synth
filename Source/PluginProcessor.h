#pragma once
#include <JuceHeader.h>
#include "dsp/DrumEngine.h"
#include "dsp/Effects.h"
#include "dsp/Limiter.h"
#include "dsp/TempoSync.h"
#include "Presets.h"
#include "Visuals.h"

namespace mds {

// parameter ID helpers: "p{part}_{slot}" cooked values with per-part ranges
inline juce::String synthParamID (int part, int slot) {
    return "p" + juce::String (part) + "_" + juce::String (slot);
}

class DrumProcessor : public juce::AudioProcessor, private juce::Timer {
public:
    DrumProcessor();
    ~DrumProcessor() override;

    void prepareToPlay (double sr, int block) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Pastel Drum Synth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return juce::jmax (1, (int) getFactoryPresets().size()); }
    int getCurrentProgram() override { return currentKit; }
    void setCurrentProgram (int i) override { loadKit (i); }
    const juce::String getProgramName (int i) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    bool isBusesLayoutSupported (const BusesLayout& l) const override;
    static constexpr int NumAuxOuts = 12;   // stereo aux buses "Out 2".."Out 13" (one per part if wanted)

    // ---- public engine API for UI ----
    static constexpr int MaxVoicesPerPart = 4;
    static constexpr int NumChokeGroups = 8;
    static constexpr int NumMacros = 4, MaxMacroTargets = 8;
    // macro targets: synth slots 0..19, model knobs, then the mixer values
    enum { MT_Model0 = 20, MT_Level = MT_Model0 + NumModelParams, MT_Pan, MT_SendRev, MT_SendDly, MT_Tune, NumMacroTargets };
    struct MacroTarget { std::atomic<int> param { -1 }; std::atomic<float> depth { 0.f }; };
    struct Macro { juce::String name; MacroTarget t[MaxMacroTargets]; };
    struct PartEngine {
        DrumVoice voices[MaxVoicesPerPart];
        PartParams params;
        float level = 0.8f, pan = 0.5f, tune = 0.f;   // tune: audio thread copy (macro-modulated)
        std::atomic<bool> mute { false }, solo { false };
        int midiNote = 36;
        std::atomic<float> meter { 0.f }; // written on audio thread, read on UI
        int tail = 0; // remaining tail samples: keeps FX ringing after voices end
        ScopeTap scope; // last-hit waveform for scope/analyzer (frozen)
        FxChainState fx;
        SlotDsp fxDsp[FxChainState::NumSlots];
        // post-chain, post-level sends to the shared returns: mirrors of the
        // p{n}_srev / p{n}_sdly parameters (audio thread, macro-modulated)
        std::atomic<float> sendRev { 0.f }, sendDly { 0.f };
        std::atomic<int> output { 0 };   // 0 = Main, 1..12 = aux bus (disabled bus -> Main)
        std::atomic<int> choke { 0 };    // 0 = none, 1..8 (hats share group 1 by default)
        Macro macros[NumMacros];
        juce::SpinLock revLock;          // guards revCur (audio thread copies the Ptr at trigger)
        RevBuf::Ptr revCur;              // REVERSE: the part's sound, rendered + reversed
        juce::uint64 revSig = 0;         // message thread: settings the cache was rendered from
        void prepare (float sr, const juce::dsp::ProcessSpec& spec) {
            for (auto& v : voices) v.prepare (sr);
            FxProcessor::prepareAll (fxDsp, FxChainState::NumSlots, spec);
        }
    };

    PartEngine parts[NumParts];
    FxChainState globalFx;
    SlotDsp globalDsp[FxChainState::NumSlots], globalDspR[FxChainState::NumSlots]; // L / R state
    // shared send returns (FX SEND tab): reverb + delay, mixed in before the global chain
    std::atomic<float> revDecay { 0.5f }, revDamp { 0.5f }, dlyTime { 0.35f }, dlyFb { 0.35f };
    std::atomic<int> dlySync { -1 };          // delay return: -1 = free (DLY TIME), else a TempoSync division
    std::atomic<float> hostBpm { 120.f };     // last tempo read from the playhead (UI readout)
    SlotDsp sendRevDsp, sendDlyDsp; // stereo FDN reverb / ping-pong delay
    std::atomic<float> masterGain { 0.8f };
    std::atomic<bool> limiterOn { true };
    std::atomic<float> outLevel { 0.f };
    std::atomic<int> selectedPart { 0 };
    std::atomic<int> auditionReq { -1 };   // UI TRIGGER: part to hit on the next block (same path as MIDI)

    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    int tailLenSamples = 44100 * 4; // FX tail budget after the last voice ends
    double currentSampleRate() const { return sampleRate; }
    std::atomic<int> stateVersion { 0 }; // bumped on every state mutation; UI refreshes on change

    int currentKit = -1; // factory index, or -1 for user/init sound
    juce::String kitName = "Init Kit";
    void loadKit (int idx);
    void initKit(); // blank slate: defaults, empty FX, undoable
    void initKitNoUndo();
    void applyKitToApvts();   // parts[].params / level / pan -> apvts (sends + tune have their own setters)
    // sends + tune live in the apvts (automatable): message-thread setters
    static juce::String sendParamID (int part, bool dly) { return "p" + juce::String (part) + (dly ? "_sdly" : "_srev"); }
    static juce::String tuneParamID (int part) { return "tune" + juce::String (part); }
    void setSend (int part, bool dly, float v);
    float getSend (int part, bool dly) const;
    void setTune (int part, float semis);
    float getTune (int part) const;
    static int defaultChoke (int part) { return part == ClosedHat || part == OpenHat ? 1 : 0; }
    void pullApvtsToParts(); // message thread: read apvts -> parts (also called at block start cheaply)

    // randomize locks: per part, synth slots 0..19 + WAVE, NOISE, the two
    // sends and the whole FX chain. Locked items are never touched by
    // RANDOMIZE / MUTATE / KIT RND. Session state: saved with the DAW
    // project, kept across kit changes (not stored in kit files).
    // LockModel = the engine choice only; each model knob has its own lock (LockModelKnob0 + k)
    enum { LockWave = 20, LockNoise, LockSendRev, LockSendDly, LockFx, LockModel, LockModelKnob0,
           NumLocks = LockModelKnob0 + NumModelParams };
    static int modelKnobLock (int k) { return LockModelKnob0 + juce::jlimit (0, NumModelParams - 1, k); }
    bool anyModelKnobLocked (int part) const {
        for (int k = 0; k < NumModelParams; ++k) if (isLocked (part, modelKnobLock (k))) return true;
        return false;
    }
    bool isLocked (int part, int key) const { return locks[juce::jlimit (0, NumParts - 1, part)][juce::jlimit (0, NumLocks - 1, key)]; }
    void setLocked (int part, int key, bool on) { locks[juce::jlimit (0, NumParts - 1, part)][juce::jlimit (0, NumLocks - 1, key)] = on; }
    void toggleLock (int part, int key) { setLocked (part, key, ! isLocked (part, key)); }

    // randomize: everything EXCEPT envelope slots 4..7 and locked items. mutate: mild perturbation.
    void randomizePart (int part, float intensity = 1.f); // intensity 0..1
    void randomizeKit (float intensity = 1.f);
    void randomizePartFx (int part, float intensity); // fresh musical FX chain from a per-part pool
    void mutatePart (int part, float amount = 0.25f);
    void mutateKit (float amount = 0.18f);

    // engine MODEL per part (0 = CLASSIC): sets the choice + loads that model's default knobs (undoable)
    void setPartModel (int part, int model);
    static juce::String modelParamID (int part, int k) { return "p" + juce::String (part) + "_mp" + juce::String (k); }
    // REVERSE: (re)render reversed copies for parts that need one (message thread; timer + tests).
    // force = false (timer): a part whose knobs are still moving waits until they settle
    void updateReverseCaches (bool force = true);
    // UI thumbnail of a part's hit (message thread): the part's current sound
    // (any engine, REVERSE applied) rendered offline at 16 kHz and reduced to
    // `buckets` normalised peak values. `sig` is the caller's last signature:
    // returns empty (and leaves sig) when the sound has not changed since.
    std::vector<float> renderThumbnail (int part, int buckets, juce::uint64& sig);

    // one history for everything: full-state snapshots (kit edits) and single
    // parameter steps ("PARAM", recorded per knob gesture: begin..end = one step)
    void pushUndo();
    void undo();
    void redo();
    std::vector<juce::ValueTree> undoStack, redoStack;
    static constexpr size_t MaxUndo = 30;

    juce::ValueTree fullStateVT();
    void loadFullStateVT (const juce::ValueTree&, bool undoable = true);

    // ---- part clipboard ----
    juce::ValueTree partToVT (int part);
    void partFromVT (int part, const juce::ValueTree&);   // no undo, no apvts push (callers do)
    juce::ValueTree partClipboard;
    void copyPart (int part) { partClipboard = partToVT (part); }
    void pastePart (int part);
    void clearPart (int part);

    // ---- macros (4 per part, centre = neutral) ----
    static juce::String macroParamID (int part, int k) { return "p" + juce::String (part) + "_mac" + juce::String (k); }
    void setMacroTarget (int part, int k, int target, float depth); // depth 0 = remove
    float macroDepth (int part, int k, int target) const;
    void defaultMacros (int part);
    static juce::String macroTargetKey (int target);
    static int macroTargetFromKey (const juce::String&);
    juce::String macroTargetLabel (int part, int target) const;
    bool macroTargetValid (int part, int target) const;

    bool locks[NumParts][NumLocks] = {};

private:
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec spec {};
    static constexpr float LimitCeiling = 0.9661f;   // -0.3 dBFS
    PeakLimiter limiter;                              // Main only; its lookahead is the plugin latency
    std::vector<float> auxDelay;                      // aux buses delayed by the same lookahead (stay in sync with Main)
    int auxDelayLen = 1, auxDelayPos = 0;
    void prepareLimiter();                            // limiter + aux delay lines; sets the plugin latency
    int globalTail = 0, sendTail = 0;
    juce::Random liveRng { (juce::uint32) juce::Time::currentTimeMillis() };

    void triggerPart (int part, float vel);
    void readPartFromApvts (int p, PartParams& out) const;   // model / knobs / reverse (allocation-free)
    // macro offsets (normalised, per target) for one part; false = all macros neutral
    bool macroOffsets (int p, float* off) const;
    void randomizeModel (int part, float intensity);
    void mutateModel (int part, float amount);
    void timerCallback() override { updateReverseCaches (false); }
    static BusesProperties makeBuses();
    juce::ReferenceCountedArray<RevBuf> revPool;   // keeps every reversed buffer alive until no voice holds it
    // REVERSE throttle: a changed sound renders once its knobs settle (or every MaxWait while dragged)
    juce::uint64 revPendSig[NumParts] = {};
    double revPendSince[NumParts] = {}, revPendFirst[NumParts] = {};

    // cached parameter pointers (filled once: the audio thread never builds ID strings)
    void cacheParamPointers();
    std::atomic<float>* slotRaw[NumParts][20] = {};
    juce::NormalisableRange<float> slotRange[NumParts][20];
    std::atomic<float>* waveRaw[NumParts] = {};
    std::atomic<float>* noiseRaw[NumParts] = {};
    std::atomic<float>* lvlRaw[NumParts] = {};
    std::atomic<float>* panRaw[NumParts] = {};
    std::atomic<float>* tuneRaw[NumParts] = {};
    std::atomic<float>* modelRaw[NumParts] = {};
    std::atomic<float>* mpRaw[NumParts][NumModelParams] = {};
    std::atomic<float>* revRaw[NumParts] = {};
    std::atomic<float>* sendRaw[NumParts][2] = {};
    std::atomic<float>* macroRaw[NumParts][NumMacros] = {};
    std::atomic<float>* masterRaw = nullptr;

    // knob-turn undo: every change gesture on an automatable parameter (editor
    // knobs, an embedding host's UI) becomes one PARAM step on the undo stack
    struct GestureUndo : juce::AudioProcessorListener {
        explicit GestureUndo (DrumProcessor& p) : proc (p) {}
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}
        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails&) override {}
        void audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) override;
        void audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) override;
        DrumProcessor& proc;
        std::vector<float> startValue;   // normalised value at gesture start, -1 = no gesture (message thread)
        bool applying = false;           // undo/redo is moving a parameter: don't record it
    };
    GestureUndo gestureUndo { *this };
    void pushUndoEntry (juce::ValueTree entry);
    juce::ValueTree paramEntry (int index) const;   // PARAM step holding the parameter's current value
    void applyParamEntry (const juce::ValueTree&);
    void applyStateVT (const juce::ValueTree&);     // kit state -> engine + apvts (no undo)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumProcessor)
};

} // namespace mds
