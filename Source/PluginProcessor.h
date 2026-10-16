#pragma once
#include <JuceHeader.h>
#include "dsp/DrumEngine.h"
#include "dsp/Effects.h"
#include "Presets.h"

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
    struct PartEngine {
        DrumVoice voices[MaxVoicesPerPart];
        PartParams params;
        float level = 0.8f, pan = 0.5f;
        bool mute = false, solo = false;
        int midiNote = 36;
        float meter = 0.f; // written on audio thread, read on UI
        int tail = 0; // remaining tail samples: keeps FX ringing after voices end
        float scopeBuf[2048] = {}; // last-hit waveform for scope/analyzer (frozen)
        std::atomic<int> scopePos { 0 };
        FxChainState fx;
        SlotDsp fxDsp[FxChainState::NumSlots];
        std::atomic<float> sendRev { 0.f }, sendDly { 0.f }; // post-chain, post-level sends to the shared returns
        std::atomic<int> output { 0 };   // 0 = Main, 1..12 = aux bus (disabled bus -> Main)
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
    void applyKitToApvts();
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
    // REVERSE: (re)render reversed copies for parts that need one (message thread; timer + tests)
    void updateReverseCaches();

    // undo support: simple stack of kit states
    void pushUndo();
    void undo();
    std::vector<juce::ValueTree> undoStack;

    juce::ValueTree fullStateVT();
    void loadFullStateVT (const juce::ValueTree&);

    // choke groups: hats share group 1
    int chokeGroup[NumParts] = { 0,0,1,1,0,0,0,0,0,0,0,0 };
    bool locks[NumParts][NumLocks] = {};

private:
    double sampleRate = 44100.0;
    juce::dsp::ProcessSpec spec {};
    float limEnv = 0.f;
    int globalTail = 0, sendTail = 0;
    juce::Random liveRng { (juce::uint32) juce::Time::currentTimeMillis() };

    void triggerPart (int part, float vel);
    void readPartFromApvts (int p, PartParams& out) const;   // model / knobs / reverse (allocation-free)
    void randomizeModel (int part, float intensity);
    void mutateModel (int part, float amount);
    void timerCallback() override { updateReverseCaches(); }
    static BusesProperties makeBuses();
    juce::ReferenceCountedArray<RevBuf> revPool;   // keeps every reversed buffer alive until no voice holds it
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumProcessor)
};

} // namespace mds
