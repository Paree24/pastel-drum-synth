#pragma once
#include "DrumDsp.h"
#include "Models.h"

// ============================================================
// 12-part drum engine: per-type params, ranges, voices
// ============================================================

namespace mds {

enum PartType {
    Kick = 0, Snare, ClosedHat, OpenHat, Ride, Crash,
    Tom1, Tom2, LowTom, Membrane, Metallic, Sfx, NumParts
};

inline const char* partName (int p) {
    static const char* n[] = { "KICK","SNARE","CHH","OHH","RIDE","CRASH","TOM 1","TOM 2","LOW TOM","MEM PERC","MET PERC","SFX" };
    return n[juce::jlimit (0, 11, p)];
}
inline int partMidiNote (int p) {
    // 12 contiguous white keys C2..B2 + C3..G3: GM-style low register (kick on C2 / 36)
    static const int m[] = { 36, 38, 40, 41, 43, 45, 47, 48, 50, 52, 53, 55 };
    return m[juce::jlimit (0, 11, p)];
}
inline juce::String noteName (int m) {
    static const char* n[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String (n[m % 12]) + juce::String (m / 12 - 1);
}
inline juce::Colour partColour (int p) {
    static const juce::uint32 c[] = {
        // v3 pastels (one hue per part, matching the Pastel EFX Grid family)
        0xffe9a3b8, 0xffe88a8a, 0xffe6dc9a, 0xffe8b86a, 0xffa9d58e, 0xff8fd0c2,
        0xff86c5c5, 0xff8fc6e8, 0xff9eb9f3, 0xffb9a7f0, 0xffc3b2e8, 0xfff2b48f
    };
    return juce::Colour (c[juce::jlimit (0, 11, p)]);
}

// 20 generic float params per part + 2 discrete. Meaning is per-type (see paramMeta).
struct PartParams {
    float p[20] = {};   // normalised-domain values stored COOKED (real units)
    int wave = 0;       // osc wave 0..4
    int noiseType = 0;  // 0..7
    int satMode = 0;    // 0..4
    // engine MODEL (0 = CLASSIC: the voice above, untouched) + its 0..1 knobs, and REVERSE
    int model = 0;
    float m[NumModelParams] = {};
    bool reverse = false;

    juce::ValueTree toVT() const {
        juce::ValueTree v ("PART");
        for (int i = 0; i < 20; ++i) v.setProperty ("p" + juce::String (i), p[i], nullptr);
        v.setProperty ("wave", wave, nullptr);
        v.setProperty ("noiseType", noiseType, nullptr);
        v.setProperty ("satMode", satMode, nullptr);
        if (model != 0 || reverse) {   // classic parts keep the original file format
            v.setProperty ("model", model, nullptr);
            for (int i = 0; i < NumModelParams; ++i) v.setProperty ("m" + juce::String (i), m[i], nullptr);
            v.setProperty ("rev", reverse, nullptr);
        }
        return v;
    }
    void fromVT (const juce::ValueTree& v) {
        for (int i = 0; i < 20; ++i) p[i] = (float) v.getProperty ("p" + juce::String (i), p[i]);
        wave = v.getProperty ("wave", wave);
        noiseType = v.getProperty ("noiseType", noiseType);
        satMode = v.getProperty ("satMode", satMode);
        model = v.getProperty ("model", 0);
        for (int i = 0; i < NumModelParams; ++i) m[i] = (float) v.getProperty ("m" + juce::String (i), 0.5f);
        reverse = (bool) v.getProperty ("rev", false);
    }
};

// Metadata for UI + randomize/mutate. Index = slot 0..15 shown (16..19 reserved/advanced).
struct ParamMeta { const char* label; float min, max, def; bool isEnv; const char* fmt; };
// fmt: "st","ms","s","%","dB","Hz","x","q"

const ParamMeta* metaForPart (int part); // 16 entries
const ParamMeta* advMetaForPart (int part, int slot); // slots 16..19, nullptr if unused
const ParamMeta* slotMeta (int part, int slot); // unified access (meta or adv)
bool isMembranePart (int part); // kick/snare/toms/membrane: punch-capable
void defaultParamsForPart (int part, PartParams& out);
// envelope slot indices per part (excluded from randomize per user request)
bool isEnvelopeSlot (int part, int slot);

// slot semantic per part (documented):
// KICK: 0 Tune(st) 1 PitchAmt(st) 2 PitchDecay(ms) 3 Curve 4 Attack(ms) 5 Decay(ms) 6 Sustain(dB) 7 Release(ms)
//       8 SubLvl 9 SubTune(oct) 10 ClickLvl 11 ClickDecay(ms) 12 Drive 13 NoiseLvl 14 NoiseDecay(ms) 15 Reso(Q)
// SNARE: 0 Tune 1 PitchAmt(st) 2 PitchDecay(ms) 3 Curve 4 Attack 5 Decay 6 Sustain 7 Release
//       8 Body 9 Noise 10 NoiseDecay(ms) 11 Tone 12 Drive 13 Comp 14 NoiseHP(Hz) 15 VelAmt
//       16 Punch 17 BoostFreq 18 BoostAmt 19 NoiseToFx (adv)
// HATS (CHH/OHH): 0 Tune 1 PitchAmt 2 PitchDecay 3 Curve 4 Attack 5 Decay 6 Sustain 7 Release
//       8 MetalMix 9 NoiseLvl 10 NoiseTone 11 Bright 12 Drive 13 Strike 14 Choke 15 VelAmt
// RIDE/CRASH: 0 Tune 1 PitchAmt 2 PitchDecay 3 Curve 4 Attack 5 Decay 6 Sustain 7 Release
//       8 PartialMix 9 NoiseLvl 10 NoiseTone 11 Damp 12 Drive 13 Strike 14 Reso 15 VelAmt
// TOMS: 0 Tune 1 PitchAmt 2 PitchDecay 3 Curve 4 Attack 5 Decay 6 Sustain 7 Release
//       8 SubLvl 9 ClickLvl 10 NoiseLvl 11 NoiseTone 12 NoiseDecay 13 Reso 14 Drive 15 VelAmt
// MEMBRANE: 0 Tune 1 PitchAmt 2 PitchDecay 3 Curve 4 Attack 5 Decay 6 Sustain 7 Release
//       8 MembMix 9 Damp 10 NoiseLvl 11 NoiseTone 12 Drive 13 Reso 14 FM 15 VelAmt
// METALLIC: 0 Tune 1 Bend(+-st) 2 BendTime 3 Curve(bipolar) 4 Attack 5 Decay 6 Sustain 7 Release
//       8 Material 9 Inharm 10 Excite 11 Hardness 12 Damp 13 Drive 14 OscLayer 15 VelAmt
// SFX: 0 Tune 1 Sweep(+-st) 2 SweepTime 3 Curve(bipolar) 4 Attack 5 Decay 6 Sustain 7 Release
//       8 FM 9 Noise 10 Cutoff 11 FilterEnv 12 Drive 13 Crush 14 Reso 15 VelAmt
//       16 LfoRate 17 LfoPitch 18 LfoFilt 19 FmRatio (adv)

// a part's sound rendered once and reversed (REVERSE button); shared, immutable
struct RevBuf : juce::ReferenceCountedObject {
    using Ptr = juce::ReferenceCountedObjectPtr<RevBuf>;
    std::vector<float> data;
};

// ---- one mono voice ----
struct DrumVoice {
    bool active = false;
    int part = 0;
    float velocity = 1.f;
    float sampleRate = 44100.f;

    AnalogOsc osc, osc2, sub;
    NoiseGen noise;
    Env amp;
    PitchEnv pitch;
    ResFilter bodyFilt, noiseFilt;
    PeakFilter boost;   // BOOST knob bell (kick/snare/toms/membrane)
    Saturator sat;
    // modal resonators for membrane / metallic (tuned-ish percussion keeps them)
    struct Partial { float ph=0.f, amp=0.f, ratio=1.f, dec=0.3f, t=0.f; };
    Partial partials[8];
    // cymbal voice (hats/ride/crash): classic analog-style 6-square metal bank,
    // ride bell partials, filter chain (2x HP, clank BP, noise HP, tail LP)
    float cymPh[6] = {}, bellPh[3] = {};
    ResFilter cymA, cymB, cymC, cymD, cymE;
    // snare state: fund/ping/FM phases, tail filter pair, comp follower
    float snBodyPh = 0.f, snPingPh = 0.f, snFmPh = 0.f;
    float snCompEnv = 0.f;
    ResFilter snTailHp, snTailLp, snSnapBp, snBridge;
    float hitT = 0.f; // per-hit clock (s): never stops, unlike pitch.t (which freezes at P DECAY)
    // portion of the last rendered sample that must bypass the part FX chain
    // (snare noise layer: stays clean, never hits DIST/MULTIBAND/COMP)
    float postFx = 0.f;
    // metallic click ping (kick/toms): inharmonic partials, one per part tone
    float pingPh[3] = { 0.f, 0.3f, 0.6f };
    float pingFreq[3] = { 0.f, 0.f, 0.f };
    float fbState = 0.f;
    float crushHold = 0.f; int crushCnt = 0; // SFX CRUSH sample-hold
    float mtY[8][2] = {}, mtExLp = 0.f, mtFollow = 0.f, mtOscPh[2] = {}; // metal resonators / exciter / ring-out / osc layer
    float sfxLfoPh = 0.f, sfxB = 0.f;                                  // SFX LFO + FM modulator phases
    float panL = 0.7f, panR = 0.7f;
    float choke = 0.f;
    float velLpK = 1.f, velLpA = 0.f, velLpB = 0.f; // velocity tone (1 = open)
    void setVelTone (float vel, float amt) {
        const float fc = velToneHz (vel, amt);
        velLpK = fc >= 18000.f ? 1.f : 1.f - std::exp (-6.2831853f * fc / sampleRate);
        velLpA = velLpB = 0.f;
    }
    // live-tweakable trigger state (wave/tune keep working on sustained notes)
    float baseMidiF = 60.f, tuneOff = 0.f, lastTuneF = 9999.f;

    void prepare (float sr) {
        sampleRate = sr;
        osc.setSampleRate (sr); osc2.setSampleRate (sr); sub.setSampleRate (sr);
        noise.setSampleRate (sr); amp.setSampleRate (sr); pitch.setSampleRate (sr);
        bodyFilt.setSampleRate (sr); noiseFilt.setSampleRate (sr);
        cymA.setSampleRate (sr); cymB.setSampleRate (sr); cymC.setSampleRate (sr);
        cymD.setSampleRate (sr); cymE.setSampleRate (sr);
        snTailHp.setSampleRate (sr); snTailLp.setSampleRate (sr); snSnapBp.setSampleRate (sr); snBridge.setSampleRate (sr);
        mv.prepare (sr);
    }
    void trigger (int partType, const PartParams& pr, float vel, float tuneOffsetSt);
    // REVERSE: play a pre-rendered, reversed copy of the part's sound
    void startReverse (int partType, RevBuf::Ptr buf, float gainIn, float vel = 1.f, float velAmt = 0.f) {
        part = partType; rev = buf; revPos = 0; revGain = gainIn; revFade = 1.f; revFadeStep = 0.f;
        setVelTone (vel, velAmt);
        modelOn = false; active = buf != nullptr; postFx = 0.f; choke = 0.f;
    }
    // one-shot unless SUSTAIN is dialled in: sequencer note lengths must not
    // truncate DECAY (sustain > -60 dB = gated, note-off starts RELEASE)
    void noteOff() { if (! modelOn && rev == nullptr && amp.sustain > 0.f) amp.noteOff(); }
    void chokeNow (float ms = 25.f) {
        choke = ms / 1000.f * sampleRate;
        if (modelOn) mv.choke (ms);
        if (rev != nullptr) revFadeStep = std::max (revFadeStep, revFade / std::max (1.f, choke));
    }
    ModelVoice mv; bool modelOn = false;
    RevBuf::Ptr rev; int revPos = 0; float revGain = 1.f, revFade = 1.f, revFadeStep = 0.f;
    bool isDone() const { return ! active; }
    float render();
    float renderWithParams (const PartParams& pr);
    float renderRaw (const PartParams& pr); // renderWithParams before the velocity tone
    float renderSnare (const PartParams& pr);
    float renderCymbal (const PartParams& pr);
    float renderMetal (const PartParams& pr);
    float renderSfx (const PartParams& pr);
};

} // namespace mds
