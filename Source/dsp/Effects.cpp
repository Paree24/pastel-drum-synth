#include "Effects.h"

namespace mds {

void SlotDsp::prepare (const juce::dsp::ProcessSpec& s) {
    spec = s;
    for (auto& f : eq) f.reset();
    for (int i = 0; i < 4; ++i) eqSm[i] = 0.f;
    reverb.prepare ((float) s.sampleRate);
    fltA.setSampleRate ((float) s.sampleRate);
    fltB.setSampleRate ((float) s.sampleRate);
    fltA.reset(); fltB.reset();
    delayBuf.setSize (2, (int) (s.sampleRate * 2.0) + 64, false, true, true);
    reset();
}
void SlotDsp::reset() {
    for (auto& f : eq) f.reset();
    fltA.reset(); fltB.reset();
    eqSm[0]=eqSm[1]=eqSm[2]=eqSm[3]=0.f;
    compEnv = 0.f; trAF = trAS = trRF = trRS = trG = 0.f;
    reverb.reset(); delayBuf.clear(); delayPos = 0; fbLp = dlyHp = modPh = tiltLp = revTone = 0.f;
}

void FxProcessor::prepareAll (SlotDsp* dsps, int n, const juce::dsp::ProcessSpec& spec) {
    for (int i = 0; i < n; ++i) dsps[i].prepare (spec);
}

void FxProcessor::defaultSlot (int type, FxSlot& s) {
    s.type = type; s.bypass = false;
    switch (type) {
        case FX_EQ: s.setParams (0.5f,0.5f,0.5f,0.5f,0.5f,1.f); break;
        case FX_COMP: s.setParams (0.6f,0.4f,0.3f,0.4f,0.5f,1.f); break;
        case FX_TRANSIENT: s.setParams (0.6f,0.5f,0.5f,0.5f,0.5f,1.f); break;
        case FX_DIST: s.setParams (0.35f,0.5f,0.1f,0.5f,0.5f,0.6f); break;
        case FX_REVERB: s.setParams (0.4f,0.5f,0.5f,0.f,0.5f,0.3f); break; // PRE 0 ms by default
        case FX_DELAY: s.setParams (0.35f,0.35f,0.5f,0.5f,0.f,0.3f); break;
        case FX_FILTER: s.setParams (0.6f,0.25f,0.f,0.f,0.2f,1.f); break;
        default: s.setParams (0.5f,0.5f,0.5f,0.5f,0.5f,1.f); break;
    }
}

const char* FxProcessor::knobLabel (int type, int knob) {
    static const char* eq[] = {"LOW","LOW MID","HIGH MID","HIGH","TILT","MIX"};
    static const char* cp[] = {"THRESH","RATIO","ATTACK","RELEASE","MAKEUP","MIX"};
    static const char* tr[] = {"ATTACK","SUSTAIN","SHAPE","SENS","OUT","MIX"};
    static const char* di[] = {"DRIVE","TONE","ALGO","OUT","BIAS","MIX"};
    static const char* rv[] = {"DECAY","SIZE","DAMP","PRE","TONE","MIX"};
    static const char* dl[] = {"TIME","FDBK","LP","HP","MOD","MIX"};
    static const char* fl[] = {"CUTOFF","RESO","MODE","SLOPE","DRIVE","MIX"};
    static const char* nn[] = {"A","B","C","D","E","MIX"};
    const char** t = nn;
    switch (type) {
        case FX_EQ: t = eq; break; case FX_COMP: t = cp; break; case FX_TRANSIENT: t = tr; break;
        case FX_DIST: t = di; break; case FX_REVERB: t = rv; break;
        case FX_DELAY: t = dl; break; case FX_FILTER: t = fl; break; default: break;
    }
    return t[juce::jlimit (0, numKnobs (type) - 1, knob)];
}

static float softClip (float x) { return std::tanh (x); }

float FxProcessor::processSlot (FxSlot& slot, SlotDsp& dsp, float x, float sr) {
    if (slot.bypass) return x;
    int type = slot.type.load();
    if (! isUsableFx (type)) return x; // empty / retired slot: hard bypass
    float p0 = slot.p0.load(), p1 = slot.p1.load(), p2 = slot.p2.load();
    float p3 = slot.p3.load(), p4 = slot.p4.load(), p5 = slot.p5.load();
    float wet = juce::jlimit (0.f, 1.f, p5);
    float y = x;

    switch (type) {
        case FX_EQ: {
            // p0..p3 gains -15..+15dB on fixed bands. Gains glide toward targets
            // (zipper-free drags) and coefficients are computed inline: no
            // audio-thread allocation, ever.
            const float freqs[4] = { 120.f, 800.f, 3000.f, 9000.f };
            const float gs[4] = { p0, p1, p2, p3 };
            for (int b = 0; b < 4; ++b) {
                float target = gs[b] * 30.f - 15.f;
                dsp.eqSm[b] += 0.005f * (target - dsp.eqSm[b]);
                if (std::abs (dsp.eqSm[b] - target) > 0.0005f)
                    dsp.eq[b].setPeak (freqs[b], 0.9f, dsp.eqSm[b], sr);
                y = dsp.eq[b].process (y);
            }
            // TILT: +-6 dB see-saw around ~1 kHz (centre = flat)
            {
                float tg = juce::Decibels::decibelsToGain ((p4 * 2.f - 1.f) * 6.f);
                dsp.tiltLp += (1.f - std::exp (-juce::MathConstants<float>::twoPi * 1000.f / sr)) * (y - dsp.tiltLp);
                y = dsp.tiltLp / tg + (y - dsp.tiltLp) * tg;
            }
            break;
        }
        case FX_COMP: {
            float thresh = -48.f + p0 * 48.f;
            float ratio = 1.f + p1 * 19.f;
            float att = 0.0005f + p2 * p2 * 0.15f;
            float rel = 0.01f + p3 * p3 * 0.6f;
            float aC = std::exp (-1.f / (att * sr + 1.f)), rC = std::exp (-1.f / (rel * sr + 1.f));
            float inDb = 20.f * std::log10 (std::abs (y) + 1e-6f);
            float over = juce::jmax (0.f, inDb - thresh);
            float target = over * (1.f - 1.f / ratio);
            dsp.compEnv = target > dsp.compEnv ? aC * dsp.compEnv + (1.f - aC) * target : rC * dsp.compEnv + (1.f - rC) * target;
            float makeup = p4 * 24.f;
            y = y * juce::Decibels::decibelsToGain (-dsp.compEnv + makeup * 0.25f);
            break;
        }
        case FX_TRANSIENT: {
            // Level-independent (SPL-style) design: gain is a dB difference
            // between envelope pairs, applied as a clean VCA (no rectified terms).
            //   attack  = fast-rise vs slow-rise follower (> 0 dB only on onsets)
            //   sustain = slow-fall vs fast-fall follower (> 0 dB only in tails)
            // ATTACK/SUSTAIN: -1..+1 (centre = off). SHAPE: attack window 1..60 ms.
            // SENS: detection depth. OUT: -12..+12 dB (centre = unity).
            auto co = [sr] (float sec) { return std::exp (-1.f / (sec * sr)); };
            float ax = std::abs (y);
            float win = 0.001f * std::pow (60.f, p2);
            const float rel = 0.08f;
            auto follow = [] (float& e, float in, float cA, float cR) {
                float c = in > e ? cA : cR; e = c * e + (1.f - c) * in;
            };
            follow (dsp.trAF, ax, co (0.0002f), co (rel));
            follow (dsp.trAS, ax, co (win),     co (rel));
            follow (dsp.trRF, ax, co (0.0002f), co (0.025f));
            follow (dsp.trRS, ax, co (0.0002f), co (0.35f));
            auto db = [] (float e) { return 20.f * std::log10 (juce::jmax (e, 1.0e-5f)); };
            float atkDiff = juce::jmax (0.f, db (dsp.trAF) - db (dsp.trAS));
            float susDiff = juce::jmax (0.f, db (dsp.trRS) - db (dsp.trRF));
            if (dsp.trRS < 1.0e-4f) susDiff = 0.f; // never pump the noise floor
            float k = 0.25f + p3 * 1.0f;
            float att = p0 * 2.f - 1.f, sus = p1 * 2.f - 1.f;
            float gDb = juce::jlimit (-36.f, 18.f, att * atkDiff * k + sus * susDiff * k);
            dsp.trG += (gDb - dsp.trG) * (1.f - co (0.0003f)); // de-click
            y *= juce::Decibels::decibelsToGain (dsp.trG + (p4 * 2.f - 1.f) * 12.f);
            break;
        }
        case FX_DIST: {
            float drive = 1.f + p0 * 30.f;
            // 8 distinct algorithms on ALGO knob
            int mode = juce::jlimit (0, 7, (int) (p2 * 7.999f));
            // BIAS: DC offset into the shaper (asymmetric clipping = even
            // harmonics); the shaped offset is subtracted so no DC leaks out
            float bias = p4 * 0.6f;
            auto shape = [mode] (float d) {
                float s = d;
                switch (mode) {
                    case 0: s = softClip (d); break;                                            // soft tube
                    case 1: s = juce::jlimit (-1.f, 1.f, d); break;                              // hard clip
                    case 2: s = std::tanh (juce::jlimit (-1.5f, 1.5f, d) * 3.f) * 0.85f; break;  // fuzz
                    case 3: { float f = d * 0.6f; if (f > 1.f) f = 2.f - f; if (f < -1.f) f = -2.f - f; s = f; break; } // foldback
                    case 4: s = d >= 0.f ? std::tanh (d * 1.6f) : std::tanh (d * 0.55f); break;  // asymmetric
                    case 5: { float steps = 16.f; s = std::round (juce::jlimit (-1.f, 1.f, d) * steps) / steps; break; } // bitcrush
                    case 6: s = std::sin (juce::jlimit (-1.5f, 1.5f, d) * juce::MathConstants<float>::halfPi) * 0.9f; break; // sine shape
                    default: s = std::tanh ((d > 0.f ? d : d * 0.15f) * 1.4f); break;          // rectify
                }
                return s;
            };
            float s = shape (y * drive + bias) - shape (bias);
            float tone = p1;
            dsp.fbLp += (0.02f + tone * 0.9f) * (s - dsp.fbLp);
            s = dsp.fbLp * 0.35f + s * 0.65f;
            y = s * 0.7f * juce::Decibels::decibelsToGain ((p3 * 2.f - 1.f) * 12.f); // OUT: -12..+12 dB
            break;
        }
        case FX_REVERB: {
            // DECAY = RT60 0.15..20 s, SIZE = room length, DAMP = in-tail HF
            // loss, PRE = pre-delay 0..200 ms, TONE = wet brightness, MIX
            int n = dsp.delayBuf.getNumSamples();
            int pre = juce::jlimit (0, n - 2, (int) (p3 * 0.2f * sr));
            dsp.delayBuf.setSample (0, dsp.delayPos, y);
            float xin = dsp.delayBuf.getSample (0, (dsp.delayPos - pre + n) % n);
            dsp.delayPos = (dsp.delayPos + 1) % n;
            float l = 0.f, r = 0.f;
            dsp.reverb.process (xin, rt60FromKnob (p0), p1, p2, l, r);
            float wetSig = FdnReverb::monoOut (l, r) * 1.4f; // mono slot: both sides (all 16 lines)
            float tc = 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (2000.f * std::pow (2.f, p4 * 3.3f), sr * 0.45f) / sr);
            dsp.revTone += tc * (wetSig - dsp.revTone);
            y = dsp.revTone; // fully wet: MIX blends below
            break;
        }
        case FX_DELAY: {
            // TIME, FDBK, LP (repeat darkening), HP (repeat thinning 20 Hz..2 kHz),
            // MOD (tape wobble on the read head), MIX
            float tSec = 0.02f + p0 * p0 * 1.2f;
            dsp.modPh += juce::MathConstants<float>::twoPi * 0.6f / sr;
            if (dsp.modPh > juce::MathConstants<float>::twoPi) dsp.modPh -= juce::MathConstants<float>::twoPi;
            float dSamp = tSec * sr * (1.f + p4 * 0.012f * std::sin (dsp.modPh));
            int n = dsp.delayBuf.getNumSamples();
            dSamp = juce::jlimit (1.f, (float) (n - 3), dSamp);
            float rp = (float) dsp.delayPos - dSamp;
            if (rp < 0.f) rp += (float) n;
            int i0 = (int) rp; float fr = rp - (float) i0;
            float dl = dsp.delayBuf.getSample (0, i0 % n) * (1.f - fr) + dsp.delayBuf.getSample (0, (i0 + 1) % n) * fr;
            dsp.fbLp += (0.05f + (1.f - p2) * 0.5f) * (dl - dsp.fbLp);
            float hpC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * (20.f * std::pow (100.f, p3)) / sr);
            dsp.dlyHp += hpC * (dsp.fbLp - dsp.dlyHp);
            float rep = dsp.fbLp - dsp.dlyHp;
            float fb = juce::jlimit (0.f, 0.92f, p1 * 0.95f);
            dsp.delayBuf.setSample (0, dsp.delayPos, juce::jlimit (-1.2f, 1.2f, y + rep * fb));
            dsp.delayPos = (dsp.delayPos + 1) % n;
            y = rep; // fully wet: MIX blends below
            break;
        }
        case FX_FILTER: {
            // resonant multimode: LP / HP / BP / Notch, 12 or 24 dB/oct
            float cutoff = 20.f * std::pow (900.f, p0); // 20 Hz .. 18 kHz
            float q = 0.5f + p1 * 14.5f;
            int mode = juce::jlimit (0, 3, (int) (p2 * 3.999f)); // 0 LP 1 HP 2 BP 3 Notch
            bool steep = p3 >= 0.5f; // 24 dB cascade
            float s = juce::jlimit (-2.f, 2.f, y * (1.f + p4 * 8.f));
            // NOTE: ResFilter modes are 0 LP / 1 BP / 2 HP
            auto stage = [&] (ResFilter& f, float in) -> float {
                if (mode == 3) return in - f.process (in, cutoff, q, 1); // notch via BP
                int rm = (mode == 0) ? 0 : (mode == 1 ? 2 : 1);
                return f.process (in, cutoff, q, rm);
            };
            float o = stage (dsp.fltA, s);
            if (steep) o = stage (dsp.fltB, o);
            y = std::tanh (o) * 0.9f;
            break;
        }
        default: break;
    }
    float dry = x;
    return dry * (1.f - wet) + y * wet;
}

void FxProcessor::processChain (FxChainState& chain, SlotDsp* dsps, float& L, float& R, float sr) {
    float m = (L + R) * 0.5f;
    if (chain.enabled.load())
        for (int k = 0; k < FxChainState::NumSlots; ++k) {
            int idx = chain.order[(size_t) k];
            m = processSlot (chain.slots[(size_t) idx], dsps[idx], m, sr);
        }
    // mono-ize back with slight stereo from chain (width handled in voice); keep stereo equal
    L = m; R = m;
}

void FxProcessor::sendReverbStereo (SlotDsp& dsp, float x, float decay, float damp, float& outL, float& outR) {
    // longer decays get a bigger space: one knob covers room .. hall
    dsp.reverb.process (x, rt60FromKnob (decay), 0.3f + 0.7f * decay, damp, outL, outR);
}

void FxProcessor::sendDelayStereo (SlotDsp& dsp, float x, float tSec, float fb, float sr, float& outL, float& outR) {
    int n = dsp.delayBuf.getNumSamples();
    int dLen = juce::jlimit (1, n - 2, (int) (tSec * sr));
    int rp = (dsp.delayPos - dLen + n) % n;
    float dL = dsp.delayBuf.getSample (0, rp), dR = dsp.delayBuf.getSample (1, rp);
    float g = juce::jlimit (0.f, 0.92f, fb * 0.95f);
    dsp.fbLp += 0.3f * (dR - dsp.fbLp); // darken repeats a little
    dsp.delayBuf.setSample (0, dsp.delayPos, juce::jlimit (-1.2f, 1.2f, x + dsp.fbLp * g)); // R repeats feed L
    dsp.delayBuf.setSample (1, dsp.delayPos, juce::jlimit (-1.2f, 1.2f, dL * g));           // L repeats feed R
    dsp.delayPos = (dsp.delayPos + 1) % n;
    outL = dL; outR = dR;
}

void FxProcessor::clearChain (FxChainState& ch) {
    for (int i = 0; i < FxChainState::NumSlots; ++i) defaultSlot (FX_NONE, ch.slots[i]);
    for (int i = 0; i < FxChainState::NumSlots; ++i) ch.order[(size_t) i] = i; // ALL slots: no duplicates, none skipped
    ch.enabled.store (true);
}

int FxProcessor::numActive (const FxChainState& ch) {
    int n = 0;
    for (int i = 0; i < FxChainState::NumSlots; ++i)
        if (ch.slots[(size_t) i].type.load() != FX_NONE) ++n;
    return n;
}

void FxProcessor::normaliseChainOrder (FxChainState& ch) {
    bool seen[FxChainState::NumSlots] = {};
    std::array<int, FxChainState::NumSlots> out {};
    int n = 0;
    for (int k = 0; k < FxChainState::NumSlots; ++k) {
        int s = ch.order[(size_t) k];
        if (s >= 0 && s < FxChainState::NumSlots && ! seen[(size_t) s]) {
            seen[(size_t) s] = true;
            out[(size_t) n++] = s;
        }
    }
    for (int s = 0; s < FxChainState::NumSlots; ++s)
        if (! seen[(size_t) s]) out[(size_t) n++] = s;
    ch.order = out;
}

} // namespace mds
