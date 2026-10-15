#pragma once
#include <JuceHeader.h>
#include "DrumDsp.h"
#include <array>
#include <vector>
#include <algorithm>

// ============================================================
// 7 effect types (EQ4, Comp, Transient, Distortion, Reverb, Delay, Filter)
// + reorderable chain. Params as atomics (message-thread writes).
// ============================================================

namespace mds {

// FX_RETIRED (4) was OTT: the id stays reserved so saved files keep their
// numbering; a retired slot loads as empty.
enum FxType { FX_EQ = 0, FX_COMP, FX_TRANSIENT, FX_DIST, FX_RETIRED, FX_REVERB, FX_DELAY, FX_FILTER, FX_NONE = 99 };
inline bool isUsableFx (int t) { return t >= FX_EQ && t <= FX_FILTER && t != FX_RETIRED; }

inline const char* fxTypeName (int t) {
    switch (t) {
        case FX_EQ: return "EQ"; case FX_COMP: return "COMPRESSOR"; case FX_TRANSIENT: return "TRANSIENT SHAPER";
        case FX_DIST: return "DISTORTION"; case FX_REVERB: return "REVERB";
        case FX_DELAY: return "DELAY"; case FX_FILTER: return "FILTER"; default: return "OFF";
    }
}

// 6 knob params per slot (meaning depends on type)
struct FxSlot {
    std::atomic<int> type { FX_EQ };
    std::atomic<float> p0 { 0.5f }, p1 { 0.5f }, p2 { 0.5f }, p3 { 0.5f }, p4 { 0.5f }, p5 { 0.7f }; // p5 = MIX
    std::atomic<bool> bypass { false };

    FxSlot() = default;
    FxSlot (const FxSlot& o) { copyFrom (o); }
    FxSlot (FxSlot&& o) noexcept { copyFrom (o); }
    FxSlot& operator= (const FxSlot& o) { copyFrom (o); return *this; }
    FxSlot& operator= (FxSlot&& o) noexcept { copyFrom (o); return *this; }
    void copyFrom (const FxSlot& o) {
        type.store (o.type.load()); p0.store (o.p0.load()); p1.store (o.p1.load());
        p2.store (o.p2.load()); p3.store (o.p3.load()); p4.store (o.p4.load());
        p5.store (o.p5.load());
        bypass.store (o.bypass.load());
    }

    void setParams (float a,float b,float c,float d,float e,float f) { p0=a;p1=b;p2=c;p3=d;p4=e;p5=f; }
    juce::ValueTree toVT() const {
        juce::ValueTree v ("FX");
        v.setProperty ("type", (int) type.load(), nullptr);
        v.setProperty ("p0", p0.load(), nullptr); v.setProperty ("p1", p1.load(), nullptr);
        v.setProperty ("p2", p2.load(), nullptr); v.setProperty ("p3", p3.load(), nullptr);
        v.setProperty ("p4", p4.load(), nullptr); v.setProperty ("p5", p5.load(), nullptr);
        v.setProperty ("bypass", bypass.load(), nullptr);
        return v;
    }
    void fromVT (const juce::ValueTree& v) {
        type = (int) v.getProperty ("type", 0);
        if (type != FX_NONE && ! isUsableFx (type)) type = FX_NONE; // retired (OTT) or unknown
        p0 = (float) v.getProperty ("p0", 0.5f); p1 = (float) v.getProperty ("p1", 0.5f);
        p2 = (float) v.getProperty ("p2", 0.5f); p3 = (float) v.getProperty ("p3", 0.5f);
        p4 = (float) v.getProperty ("p4", 0.5f); p5 = (float) v.getProperty ("p5", 0.7f);
        bypass = (bool) v.getProperty ("bypass", false);
    }
};

struct FxChainState {
    static constexpr int NumSlots = 10;
    FxSlot slots[NumSlots];
    std::array<int, NumSlots> order { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
    std::atomic<bool> enabled { true }; // master toggle for the whole chain

    FxChainState() = default;
    FxChainState (const FxChainState& o) { copyFrom (o); }
    FxChainState (FxChainState&& o) noexcept { copyFrom (o); }
    FxChainState& operator= (const FxChainState& o) { copyFrom (o); return *this; }
    FxChainState& operator= (FxChainState&& o) noexcept { copyFrom (o); return *this; }
    void copyFrom (const FxChainState& o) {
        for (int i = 0; i < NumSlots; ++i) slots[i].copyFrom (o.slots[i]);
        order = o.order;
        enabled.store (o.enabled.load());
    }

    juce::ValueTree toVT() const {
        juce::ValueTree v ("CHAIN");
        for (int i = 0; i < NumSlots; ++i) v.addChild (slots[i].toVT(), -1, nullptr);
        juce::String o; for (int i = 0; i < NumSlots; ++i) o += juce::String (order[(size_t) i]) + (i < NumSlots - 1 ? "," : "");
        v.setProperty ("order", o, nullptr);
        v.setProperty ("enabled", enabled.load(), nullptr);
        return v;
    }
    void fromVT (const juce::ValueTree& v) {
        for (int i = 0; i < NumSlots; ++i) {
            // init EVERYTHING first: files may predate the slot count (else
            // untouched slots keep default-constructed ACTIVE state = phantom FX)
            slots[i].type.store (FX_NONE);
            slots[i].bypass.store (false);
            slots[i].p0.store (0.5f); slots[i].p1.store (0.5f); slots[i].p2.store (0.5f);
            slots[i].p3.store (0.5f); slots[i].p4.store (0.5f); slots[i].p5.store (1.f);
        }
        for (int i = 0; i < NumSlots && i < v.getNumChildren(); ++i) slots[i].fromVT (v.getChild (i));
        for (int i = 0; i < NumSlots; ++i) order[(size_t) i] = i; // reset before applying stored order
        auto o = v.getProperty ("order", "0,1,2,3,4,5,6,7,8,9").toString();
        auto toks = juce::StringArray::fromTokens (o, ",", "");
        for (int i = 0; i < NumSlots && i < toks.size(); ++i) order[(size_t) i] = juce::jlimit (0, NumSlots - 1, toks[i].getIntValue());
        enabled.store ((bool) v.getProperty ("enabled", true));
    }
};

// ---- algorithmic reverb: 16-line feedback delay network ----
// input diffusion (5 allpasses) -> 16 delay lines (staggered, mutually
// non-harmonic lengths) mixed by a 16x16 Hadamard matrix. Each line has a
// Jot absorption filter: DC gain from the RT60, Nyquist gain from the HF
// RT60 (DAMP), so every line decays consistently per frequency (a plain LP
// per line colours the tail). Per-line cubic-interpolated modulation
// (2.5 ms at unrelated 0.13..1.06 Hz rates) smears the modal peaks that
// read as "metallic"; 2 output diffusers per side. L/R/mono taps use all 16 lines (a mono sum never
// cancels half the network). SIZE scales every line: small room .. hall;
// DECAY is a real RT60 (0.15 s .. 20 s). prepare() allocates; process() never.
struct FdnReverb {
    static constexpr int N = 16, NAP = 5, NOUT = 2;
    static constexpr float kMaxScale = 1.4f;
    float sr = 44100.f;
    std::vector<float> line[N], ap[NAP], outAp[2][NOUT];
    int pos[N] = {}, apPos[NAP] = {}, outPos[2][NOUT] = {};
    float damp[N] = {}, lfo[N] = {}, lfoInc[N] = {};

    FdnReverb() { prepare (44100.f); }
    static const float* lineMs() {
        static const float m[N] = { 21.7f, 24.3f, 27.1f, 29.9f, 33.1f, 36.7f, 40.3f, 44.3f,
                                    48.7f, 53.3f, 58.1f, 63.7f, 69.1f, 75.7f, 82.3f, 89.9f };
        return m;
    }
    static const float* apMs()  { static const float m[NAP] = { 1.7f, 2.9f, 4.3f, 6.7f, 9.1f }; return m; }
    static const float* outMs (int side) {
        static const float m[2][NOUT] = { { 5.3f, 7.9f }, { 6.1f, 8.7f } };
        return m[side];
    }
    void prepare (float s) {
        sr = s;
        for (int i = 0; i < N; ++i) line[i].assign ((size_t) (lineMs()[i] * 0.001f * kMaxScale * sr) + 128, 0.f);
        for (int i = 0; i < NAP; ++i) ap[i].assign ((size_t) (apMs()[i] * 0.001f * kMaxScale * sr) + 8, 0.f);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < NOUT; ++i) outAp[c][i].assign ((size_t) (outMs (c)[i] * 0.001f * sr) + 8, 0.f);
        reset();
    }
    void reset() {
        for (auto& b : line) std::fill (b.begin(), b.end(), 0.f);
        for (auto& b : ap) std::fill (b.begin(), b.end(), 0.f);
        for (auto& side : outAp) for (auto& b : side) std::fill (b.begin(), b.end(), 0.f);
        for (int i = 0; i < N; ++i) {
            pos[i] = 0; damp[i] = 0.f;
            lfo[i] = (float) i * 2.399f;                                         // golden-angle phases
            lfoInc[i] = juce::MathConstants<float>::twoPi * 0.75f * (0.17f + 0.083f * (float) ((i * 7) % N)) / sr; // 0.13..1.06 Hz, unrelated
        }
        for (int i = 0; i < NAP; ++i) apPos[i] = 0;
        for (auto& side : outPos) for (auto& p : side) p = 0;
    }
    static float allpass (std::vector<float>& b, int& p, int d, float g, float x) {
        int n = (int) b.size();
        d = juce::jlimit (1, n - 1, d);
        float z = b[(size_t) ((p - d + n) % n)];
        float y = -g * x + z;
        b[(size_t) p] = x + g * y;
        p = (p + 1) % n;
        return y;
    }
    static float monoOut (float l, float r) { return (l + r) * 0.5f; } // what a mono insert slot hears
    // rt60 seconds, size 0..1, dampAmt 0..1 (0 = bright, 1 = dark)
    void process (float x, float rt60, float size, float dampAmt, float& outL, float& outR) {
        const float scale = 0.2f + (kMaxScale - 0.2f) * juce::jlimit (0.f, 1.f, size);
        float v = x;
        for (int k = 0; k < NAP; ++k)
            v = allpass (ap[k], apPos[k], (int) (apMs()[k] * 0.001f * scale * sr), 0.7f, v);
        const float t60 = juce::jmax (0.05f, rt60) * sr;
        const float t60hf = t60 * std::pow (2.f, -juce::jlimit (0.f, 1.f, dampAmt) * 4.f); // HF RT60: equal .. 1/16
        const float modDepth = 0.0025f * sr * (0.5f + 0.5f * scale);                     // ~1.5..2.9 ms wander
        float sgl[N];
        for (int i = 0; i < N; ++i) {
            auto& b = line[i];
            int n = (int) b.size();
            float dly = lineMs()[i] * 0.001f * scale * sr;
            lfo[i] += lfoInc[i];
            if (lfo[i] > juce::MathConstants<float>::twoPi) lfo[i] -= juce::MathConstants<float>::twoPi;
            dly = juce::jlimit (2.f, (float) (n - 4), dly + modDepth * std::sin (lfo[i]));
            float rp = (float) pos[i] - dly;
            if (rp < 0.f) rp += (float) n;
            int i0 = (int) rp; float fr = rp - (float) i0;
            // cubic (Hermite) read: linear interpolation dulls + colours the tail
            float y0 = b[(size_t) ((i0 - 1 + n) % n)], y1 = b[(size_t) (i0 % n)], y2 = b[(size_t) ((i0 + 1) % n)], y3 = b[(size_t) ((i0 + 2) % n)];
            float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            float rd = ((c3 * fr + c2) * fr + c1) * fr + y1;
            // Jot absorption: one-pole with DC gain gl (RT60) and Nyquist gain gh (HF RT60)
            float gl = std::pow (10.f, -3.f * dly / t60), gh = std::pow (10.f, -3.f * dly / t60hf);
            float a1 = (gl - gh) / (gl + gh);
            damp[i] = gl * (1.f - a1) * rd + a1 * damp[i];
            sgl[i] = damp[i];
        }
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j) { float a = sgl[j], c = sgl[j + h]; sgl[j] = a + c; sgl[j + h] = a - c; }
        const float norm = 0.25f; // 1/sqrt(16)
        float l = 0.f, r = 0.f;
        for (int i = 0; i < N; ++i) {
            float fbk = sgl[i] * norm;
            float inj = v * ((i * 5 + 3) % 7 < 3 ? -0.25f : 0.25f);
            float w = fbk + inj;
            if (! std::isfinite (w)) w = 0.f;
            line[i][(size_t) pos[i]] = w;
            pos[i] = (pos[i] + 1) % (int) line[i].size();
            // all 16 lines on both sides, orthogonal sign patterns
            l += fbk * (((i * 3) % 5 < 2) ? -1.f : 1.f);
            r += fbk * (((i * 7 + 1) % 4 < 2) ? -1.f : 1.f);
        }
        for (int k = 0; k < NOUT; ++k) {
            l = allpass (outAp[0][k], outPos[0][k], (int) (outMs (0)[k] * 0.001f * sr), 0.6f, l);
            r = allpass (outAp[1][k], outPos[1][k], (int) (outMs (1)[k] * 0.001f * sr), 0.6f, r);
        }
        outL = l * 0.45f; outR = r * 0.45f;
    }
};

// ---- tiny biquad (allocation-free, for smoothed per-sample EQ updates) ----
struct Biquad {
    float b0=1,b1=0,b2=0,a1=0,a2=0,x1=0,x2=0,y1=0,y2=0;
    void reset() { x1=x2=y1=y2=0; }
    void setPeak (float freq, float Q, float gdb, float sr) {
        float A = std::pow (10.f, gdb / 40.f);
        float w = 2.f * juce::MathConstants<float>::pi * freq / sr;
        float c = std::cos (w), s = std::sin (w), al = s / (2.f * Q);
        float a0 = 1.f + al / A;
        b0 = (1.f + al * A) / a0; b1 = (-2.f * c) / a0; b2 = (1.f - al * A) / a0;
        a1 = (-2.f * c) / a0;     a2 = (1.f - al / A) / a0;
    }
    float process (float x) {
        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

// ---- per-slot DSP state (preallocated) ----
struct SlotDsp {
    // EQ: 4 smoothed peaking bands (no audio-thread allocation, no zipper)
    Biquad eq[4];
    float eqSm[4] = { 0.f, 0.f, 0.f, 0.f }; // smoothed band gains (dB)
    // comp
    float compEnv = 0.f;
    // transient: attack pair (fast/slow rise), sustain pair (fast/slow fall), gain smoother
    float trAF = 0.f, trAS = 0.f, trRF = 0.f, trRS = 0.f, trG = 0.f;
    // reverb + delay
    FdnReverb reverb;
    // filter stages (FILTER type reuses its own state, not the EQ's)
    ResFilter fltA, fltB;
    juce::AudioBuffer<float> delayBuf { 2, 192000 * 2 };
    int delayPos = 0;
    float fbLp = 0.f, dlyHp = 0.f, modPh = 0.f; // delay damping / HP / MOD phase
    float tiltLp = 0.f;                          // EQ tilt split
    float revTone = 0.f;                         // reverb wet tone LP
    juce::dsp::ProcessSpec spec {};

    void prepare (const juce::dsp::ProcessSpec& s);
    void reset();
};

struct FxProcessor {
    static void prepareAll (SlotDsp* dsps, int n, const juce::dsp::ProcessSpec& spec);
    // process mono sample through one slot; returns processed
    static float processSlot (FxSlot& slot, SlotDsp& dsp, float x, float sr);
    static void processChain (FxChainState& chain, SlotDsp* dsps, float& L, float& R, float sr);
    // shared send returns (wet only): REV DECAY/DAMP 0..1, DLY TIME/FDBK 0..1
    static void sendReverbStereo (SlotDsp& dsp, float x, float decay, float damp, float& outL, float& outR);
    static float rt60FromKnob (float v01) { return 0.15f * std::pow (133.f, juce::jlimit (0.f, 1.f, v01)); } // 0.15 .. 20 s
    // stereo ping-pong send delay: in -> L, repeats alternate L/R
    static void sendDelayStereo (SlotDsp& dsp, float x, float time, float fb, float sr, float& outL, float& outR);
    static void defaultSlot (int type, FxSlot& s);
    static void clearChain (FxChainState& ch); // empty: all slots OFF
    static int numActive (const FxChainState& ch);
    static constexpr int MaxKnobs = 6;
    static int numKnobs (int) { return 6; }
    // rebuild order as a clean permutation (stored order first). Kills any
    // duplicated/missing indices so one slot can never shadow another.
    static void normaliseChainOrder (FxChainState& ch);
    static const char* knobLabel (int type, int knob); // 0..numKnobs-1
};

} // namespace mds
