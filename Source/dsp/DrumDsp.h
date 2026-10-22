#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <random>

// ============================================================
// Shared analog-modelled DSP primitives (all realtime-safe)
// ============================================================

namespace mds {

// ---- utils ----
inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
inline float midiToFreq (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

// ---- seeded rng (deterministic) ----
struct SeededRng {
    std::mt19937 gen { 0x1234abcd };
    void seed (juce::uint32 s) { gen.seed (s); }
    float uni (float a = 0.f, float b = 1.f) {
        std::uniform_real_distribution<float> d (a, b); return d (gen);
    }
};

// velocity response (VEL>AMP = amt, 0..1): a power curve in amplitude, so
// half velocity is ~-10 dB and ghost notes reach -35..-40 dB at the default 80%
// (the old linear 0.2 + 0.8*vel spanned only 13 dB: hits all sounded alike).
// amt 0 = no velocity response.
inline float velGain (float vel, float amt) {
    return std::pow (juce::jlimit (0.05f, 1.f, vel), 2.f * juce::jlimit (0.f, 1.f, amt));
}
// soft hits are darker: one-pole low-pass cutoff, ~20 kHz (open) at full
// velocity down to ~2.5 kHz for ghost notes at the default 80%
inline float velToneHz (float vel, float amt) {
    return 20000.f * std::exp2 (-4.f * (1.f - juce::jlimit (0.f, 1.f, vel)) * juce::jlimit (0.f, 1.f, amt));
}

// ---- fast ADSR/AR envelope with exponential segments ----
struct Env {
    enum Stage { Idle, Attack, Decay, Sustain, Release };
    Stage stage = Idle;
    float value = 0.f, attack = 0.001f, decay = 0.12f, sustain = 0.f, release = 0.06f;
    float sr = 44100.f;

    void setSampleRate (float s) { sr = s; }
    void set (float a, float d, float s_, float r) {
        attack = juce::jmax (0.0001f, a); decay = juce::jmax (0.001f, d);
        sustain = juce::jlimit (0.f, 1.f, s_); release = juce::jmax (0.001f, r);
    }
    void noteOn() { stage = Attack; }
    void noteOff() { if (stage != Idle) stage = Release; }
    void reset() { stage = Idle; value = 0.f; }
    bool isActive() const { return stage != Idle; }
    float next() {
        const float aC = attack * sr, dC = decay * sr, rC = release * sr;
        switch (stage) {
            case Attack:  value += (1.f - value) * (1.f - std::exp (-1.f / juce::jmax (1.f, aC * 0.25f))); if (value > 0.999f) { value = 1.f; stage = Decay; } break;
            case Decay:   value += (sustain - value) * (1.f - std::exp (-1.f / juce::jmax (1.f, dC * 0.25f))); if (std::abs (value - sustain) < 0.0008f) { value = sustain; if (sustain <= 0.0005f) stage = Release; } break;
            case Sustain: value = sustain; if (sustain <= 0.0005f) stage = Release; break;
            case Release: value += (0.f - value) * (1.f - std::exp (-1.f / juce::jmax (1.f, rC * 0.25f))); if (value < 0.0006f) { value = 0.f; stage = Idle; } break;
            default: break;
        }
        return value;
    }
};

// ---- bipolar sweep curve: remaining fraction 1 -> 0 as x goes 0 -> 1 ----
// curve 0.5 = linear; < 0.5 = fast start then settle (drops/zaps, the
// membrane-like move); > 0.5 = slow start then accelerate (risers/builds)
inline float sweepRemaining (float x, float curve) {
    x = juce::jlimit (0.f, 1.f, x);
    float c = juce::jlimit (0.f, 1.f, curve);
    float k = 1.f + 6.f * std::abs (c - 0.5f) * 2.f; // 1 .. 7
    float progress = c < 0.5f ? 1.f - std::pow (1.f - x, k) : std::pow (x, k);
    return 1.f - progress;
}

// ---- one-pole pitch envelope (exp decay, strong range) ----
struct PitchEnv {
    float value = 0.f; // 0..1
    float decay = 0.2f, curve = 0.35f, amountSt = 24.f;
    float sr = 44100.f; float t = 0.f; bool on = false;
    void setSampleRate (float s) { sr = s; }
    void trigger (float amtSt, float dec, float crv) { amountSt = amtSt; decay = juce::jmax (0.002f, dec); curve = crv; t = 0.f; on = true; }
    // returns semitone offset
    float next() {
        if (! on) return 0.f;
        t += 1.f / sr;
        float x = t / decay;
        if (x >= 1.f) { on = false; return 0.f; }
        float c = juce::jlimit (0.01f, 1.f, curve);
        // curve: low = snappy concave, high = linear-ish
        float shape = std::pow (1.f - x, 1.f + 3.f * (1.f - c));
        value = shape;
        return amountSt * shape;
    }
};

// ---- analog-style oscillator with drift + phase reset ----
struct AnalogOsc {
    float phase = 0.f, freq = 55.f, sr = 44100.f;
    float driftAmt = 0.f, driftPhase = 0.f;
    int wave = 0; // 0 sine 1 tri 2 saw 3 square 4 fm-ish
    float fmAmt = 0.f, fmRatio = 1.5f, fmPhase = 0.f;
    juce::Random rng;

    void setSampleRate (float s) { sr = s; }
    void reset (float p = 0.f) { phase = p; fmPhase = 0.f; }
    // 2-sample polynomial band-limited step residual (t = phase 0..1, dt = phase increment)
    static float polyBlep (float t, float dt) {
        if (dt <= 0.f) return 0.f;
        if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    float next() {
        driftPhase += 2.f * juce::MathConstants<float>::pi * 0.7f / sr;
        float drift = driftAmt * 0.004f * std::sin (driftPhase) ;
        float f = juce::jmax (1.f, freq * (1.f + drift));
        float inc = f / sr;
        phase += inc; if (phase >= 1.f) phase -= 1.f;
        float s = 0.f;
        switch (wave) {
            case 0: s = std::sin (2.f * juce::MathConstants<float>::pi * phase); break;
            case 1: s = 2.f * std::abs (2.f * (phase - 0.5f)) - 1.f; s = -s; break;
            // saw / square: PolyBLEP-corrected edges (no aliasing fold-back on high tunings)
            case 2: s = 2.f * phase - 1.f - polyBlep (phase, inc); s = juce::jlimit (-1.f, 1.f, s * 0.8f + 0.2f * std::sin (2.f * juce::MathConstants<float>::pi * phase)); break;
            case 3: {
                float ph2 = phase + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
                s = 0.8f * ((phase < 0.5f ? 1.f : -1.f) + polyBlep (phase, inc) - polyBlep (ph2, inc));
                break;
            }
            case 4: {
                fmPhase += fmRatio * inc; if (fmPhase >= 1.f) fmPhase -= 1.f;
                float m = std::sin (2.f * juce::MathConstants<float>::pi * fmPhase);
                s = std::sin (2.f * juce::MathConstants<float>::pi * phase + fmAmt * m);
                break;
            }
            default: s = std::sin (2.f * juce::MathConstants<float>::pi * phase); break;
        }
        return s;
    }
};

// ---- multi-type noise generator (NOT just white noise) ----
// types: 0 white 1 pink 2 violet/blue 3 metallic 4 digital 5 crackle 6 tape 7 resonant
struct NoiseGen {
    juce::Random rng;
    float sr = 44100.f;
    // pink filter state (Paul Kellet)
    float b0=0,b1=0,b2=0,b3=0,b4=0,b5=0,b6=0;
    float lp1 = 0.f, lp2 = 0.f;      // tape / blue helpers
    float metalPhase[4] = {0.13f,0.31f,0.57f,0.83f};
    float resLp = 0.f, resBp = 0.f;
    float crackleTimer = 0.f;
    int bitDepth = 8;

    void setSampleRate (float s) { sr = s; }
    void reset() { b0=b1=b2=b3=b4=b5=b6=0; lp1=lp2=resLp=resBp=0; crackleTimer=0; }

    float white() { return rng.nextFloat() * 2.f - 1.f; }

    float next (int type, float tone /*0..1 brightness*/, float rateMod = 1.f) {
        float w = white();
        switch (type) {
            case 0: return w;
            case 1: { // pink (Kellet)
                b0 = 0.99886f*b0 + w*0.0555179f; b1 = 0.99332f*b1 + w*0.0750759f;
                b2 = 0.96900f*b2 + w*0.1538520f; b3 = 0.86650f*b3 + w*0.3104856f;
                b4 = 0.55000f*b4 + w*0.5329522f; b5 = -0.7616f*b5 - w*0.0168980f;
                float p = b0+b1+b2+b3+b4+b5+b6+w*0.5362f; b6 = w*0.115926f;
                return juce::jlimit (-1.f, 1.f, p * 0.35f);
            }
            case 2: { // violet/blue (HP-ish: differentiate white)
                float hp = w - lp1; lp1 += 0.02f * (w - lp1);
                float bright = juce::jlimit (0.f, 1.f, tone);
                return juce::jlimit (-1.f, 1.f, hp * (0.6f + 1.6f * bright) + w * 0.15f);
            }
            case 3: { // metallic: sum of square-ish inharmonic partials + noise
                float m = 0.f;
                const float ratios[4] = { 1.483f, 2.932f, 4.117f, 5.771f };
                for (int i = 0; i < 4; ++i) {
                    metalPhase[i] += ratios[i] * (300.f + 3000.f * tone) * rateMod / sr;
                    if (metalPhase[i] >= 1.f) metalPhase[i] -= 1.f;
                    m += (metalPhase[i] < 0.5f ? 1.f : -1.f) * (0.25f / (i + 1));
                }
                return juce::jlimit (-1.f, 1.f, m * 0.8f + w * 0.35f);
            }
            case 4: { // digital: bitcrushed + rate crushed noise
                float steps = std::pow (2.f, 3.f + 5.f * (1.f - tone));
                float q = std::round (w * steps) / steps;
                lp2 += (0.05f + 0.5f * tone) * (q - lp2);
                return juce::jlimit (-1.f, 1.f, lp2 * 1.2f);
            }
            case 5: { // crackle: sparse impulses + bed
                crackleTimer -= 1.f;
                float bed = w * 0.08f;
                if (crackleTimer <= 0.f) {
                    crackleTimer = 20.f + rng.nextFloat() * (2000.f - 1900.f * tone);
                    return juce::jlimit (-1.f, 1.f, (rng.nextFloat()*2.f-1.f) * 0.9f);
                }
                return bed;
            }
            case 6: { // tape: dark LP noise + slight AM wow
                static thread_local float wowPh = 0.f;
                wowPh += 2.f * juce::MathConstants<float>::pi * 4.f / sr;
                float cutoff = 0.02f + 0.25f * tone;
                lp1 += cutoff * (w - lp1);
                return juce::jlimit (-1.f, 1.f, lp1 * (1.f + 0.15f * std::sin (wowPh)) * 1.6f);
            }
            case 7: { // resonant: bandpassed noise, Q follows tone
                float cutoff = 400.f + 9000.f * tone * tone;
                float q = 0.5f + 8.f * tone;
                float g = juce::jlimit (0.001f, 0.99f, std::tan (juce::MathConstants<float>::pi * cutoff / sr) / (1.f + std::tan (juce::MathConstants<float>::pi * cutoff / sr)));
                // simple resonant lowpass approx
                resLp += g * (w - resLp);
                float hp2 = w - resLp;
                resBp += g * q * 0.1f * (hp2 - resBp);
                return juce::jlimit (-1.2f, 1.2f, resBp * (1.f + q * 0.4f));
            }
            default: return w;
        }
    }
};

inline const char* noiseTypeName (int t) {
    switch (t) {
        case 0: return "White"; case 1: return "Pink"; case 2: return "Violet";
        case 3: return "Metallic"; case 4: return "Digital"; case 5: return "Crackle";
        case 6: return "Tape"; case 7: return "Resonant"; default: return "White";
    }
}

// ---- RBJ peaking bell (for the BOOST knob) ----
struct PeakFilter {
    float b0=1,b1=0,b2=0,a1=0,a2=0,x1=0,x2=0,y1=0,y2=0;
    float lF=-1,lQ=-1,lG=999;
    void reset() { x1=x2=y1=y2=0; }
    void set (float freq, float Q, float gdb, float sr) {
        if (freq == lF && Q == lQ && gdb == lG) return;
        lF = freq; lQ = Q; lG = gdb;
        float A = std::pow (10.f, gdb / 40.f);
        float w = 2.f * juce::MathConstants<float>::pi * freq / sr;
        float c = std::cos (w), s = std::sin (w), al = s / (2.f * Q);
        float a0 = 1.f + al / A;
        b0 = (1.f + al * A) / a0; b1 = (-2.f * c) / a0; b2 = (1.f - al * A) / a0;
        a1 = (-2.f * c) / a0; a2 = (1.f - al / A) / a0;
    }
    float process (float x) {
        float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};
struct ResFilter {
    float sr = 44100.f;
    float ic1 = 0.f, ic2 = 0.f;
    void setSampleRate (float s) { sr = s; }
    void reset() { ic1 = ic2 = 0.f; }
    // mode 0 LP 1 BP 2 HP
    float process (float x, float cutoff, float q, int mode) {
        cutoff = juce::jlimit (20.f, juce::jmin (19000.f, sr * 0.45f), cutoff);
        q = juce::jlimit (0.05f, 20.f, q);
        float g = std::tan (juce::MathConstants<float>::pi * cutoff / sr);
        float k = 1.f / q;
        float a1 = 1.f / (1.f + g * (g + k));
        float a2 = g * a1, a3 = g * a2;
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
        // denormal guard
        if (! std::isfinite (ic1)) ic1 = 0.f;
        if (! std::isfinite (ic2)) ic2 = 0.f;
        if (mode == 0) return v2;
        if (mode == 1) return v1;
        return x - k * v1 - v2;
    }
};

// ---- saturator with clearly separated characters per mode ----
struct Saturator {
    int mode = 0; // 0 soft 1 hard 2 fuzz 3 fold 4 asym
    float drive = 0.3f, tone = 0.5f, lp = 0.f;
    float process (float x) {
        float d = 1.f + drive * drive * 16.f; // gentle taper: modes live in the knee zone
        float y = x * d, o = y;
        switch (mode) {
            case 0: o = std::tanh (y); break;                                             // soft tube
            case 1: o = juce::jlimit (-0.75f, 0.75f, y) * 1.1f + 0.1f * std::tanh (y * 2.f); break; // hard: clips early
            case 2: { float b = std::tanh (juce::jlimit (-1.5f, 1.5f, y) * 2.5f); o = b * 0.85f + 0.2f * (b - lp); break; } // fuzz + edge
            case 3: {                                                                     // foldback x2
                float f = y * 1.6f; int folds = 0;
                while (f > 1.f && folds < 3) { f = 2.f - f; ++folds; }
                while (f < -1.f && folds < 3) { f = -2.f - f; ++folds; }
                o = f; break;
            }
            default: o = y >= 0.f ? std::tanh (y * 2.f) : std::tanh (y * 0.5f) * 0.9f; break; // asymmetric
        }
        // light tone smoothing only: heavy smoothing dulled the modes together
        lp += (0.05f + 0.9f * tone) * (o - lp);
        return juce::jlimit (-1.2f, 1.2f, lp * 0.25f + o * 0.75f);
    }
    void reset() { lp = 0.f; }
};

} // namespace mds
