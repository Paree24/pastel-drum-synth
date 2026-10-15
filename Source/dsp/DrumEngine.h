#pragma once
#include "DrumDsp.h"

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
        0xfff49ac1, 0xfff87171, 0xfffbbf24, 0xfffb923c, 0xffa3e635, 0xff34d399,
        0xff22d3ee, 0xff38bdf8, 0xff60a5fa, 0xffa78bfa, 0xfff0abfc, 0xfff9a8d4
    };
    return juce::Colour (c[juce::jlimit (0, 11, p)]);
}

// 20 generic float params per part + 2 discrete. Meaning is per-type (see paramMeta).
struct PartParams {
    float p[20] = {};   // normalised-domain values stored COOKED (real units)
    int wave = 0;       // osc wave 0..4
    int noiseType = 0;  // 0..7
    int satMode = 0;    // 0..4

    juce::ValueTree toVT() const {
        juce::ValueTree v ("PART");
        for (int i = 0; i < 20; ++i) v.setProperty ("p" + juce::String (i), p[i], nullptr);
        v.setProperty ("wave", wave, nullptr);
        v.setProperty ("noiseType", noiseType, nullptr);
        v.setProperty ("satMode", satMode, nullptr);
        return v;
    }
    void fromVT (const juce::ValueTree& v) {
        for (int i = 0; i < 20; ++i) p[i] = (float) v.getProperty ("p" + juce::String (i), p[i]);
        wave = v.getProperty ("wave", wave);
        noiseType = v.getProperty ("noiseType", noiseType);
        satMode = v.getProperty ("satMode", satMode);
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
    // cymbal voice (hats/ride/crash): 808-style 6-square metal bank,
    // ride bell partials, filter chain (2x HP, clank BP, noise HP, tail LP)
    float cymPh[6] = {}, bellPh[3] = {};
    ResFilter cymA, cymB, cymC, cymD, cymE;
    // snare state: fund/ping/FM phases, tail filter pair, comp follower
    float snBodyPh = 0.f, snPingPh = 0.f, snFmPh = 0.f;
    float snCompEnv = 0.f;
    ResFilter snTailHp, snTailLp, snSnapBp, snBridge;
    float hitT = 0.f; // per-hit clock (s): never stops, unlike pitch.t (which freezes at P DECAY)
    // portion of the last rendered sample that must bypass the part FX chain
    // (snare noise layer: stays clean, never hits DIST/OTT/COMP)
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
    }
    void trigger (int partType, const PartParams& pr, float vel, float tuneOffsetSt);
    // one-shot unless SUSTAIN is dialled in: sequencer note lengths must not
    // truncate DECAY (sustain > -60 dB = gated, note-off starts RELEASE)
    void noteOff() { if (amp.sustain > 0.f) amp.noteOff(); }
    void chokeNow (float ms = 25.f) { choke = ms / 1000.f * sampleRate; }
    bool isDone() const { return ! active; }
    float render();
    float renderWithParams (const PartParams& pr);
    float renderSnare (const PartParams& pr);
    float renderCymbal (const PartParams& pr);
    float renderMetal (const PartParams& pr);
    float renderSfx (const PartParams& pr);
};

} // namespace mds
