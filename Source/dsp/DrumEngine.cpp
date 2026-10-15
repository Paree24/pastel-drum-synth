#include "DrumEngine.h"

namespace mds {

// ---------------- metadata (ranges depend on part type) ----------------
// Envelope ranges per type: hats short, kick medium, crash long, etc.
static const ParamMeta kickM[16] = {
    {"TUNE",-24,12,-5,false,"st"},{"PITCH AMT",0,60,36,false,"st"},{"P DECAY",2,900,320,false,"ms"},{"CURVE",1,100,35,false,"%"},
    {"ATTACK",0.05,20,1.0,true,"ms"},{"DECAY",40,2500,420,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,1200,120,true,"ms"},
    {"SUB LVL",0,100,70,false,"%"},{"SUB OCT",-2,0,-1,false,"x"},{"CLICK LVL",0,100,40,false,"%"},{"CLICK DEC",1,120,15,false,"ms"},
    {"DRIVE",0,100,0,false,"%"},{"NOISE LVL",0,100,25,false,"%"},{"N DECAY",1,300,40,false,"ms"},{"RESO",0,100,35,false,"%"}
};
static const ParamMeta snareM[16] = {
    {"TUNE",-12,12,0,false,"st"},{"PITCH AMT",0,48,18,false,"st"},{"P DECAY",3,120,18,false,"ms"},{"CURVE",1,100,30,false,"%"},
    {"ATTACK",0.05,20,0.5,true,"ms"},{"DECAY",35,800,120,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,800,80,true,"ms"},
    {"BODY",0,100,65,false,"%"},{"NOISE",0,100,70,false,"%"},{"N DECAY",20,400,120,false,"ms"},{"TONE",0,100,55,false,"%"},
    {"DRIVE",0,100,20,false,"%"},{"COMP",0,100,50,false,"%"},{"NOISE HP",20,8000,400,false,"Hz"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta chhM[16] = {
    {"TUNE",-12,12,2,false,"st"},{"SPREAD",0,36,10,false,"%"},{"P DECAY",1,200,40,false,"ms"},{"CURVE",1,100,50,false,"%"},
    {"ATTACK",0.02,10,0.3,true,"ms"},{"DECAY",10,500,90,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",2,300,30,true,"ms"},
    {"METAL MIX",0,100,70,false,"%"},{"NOISE LVL",0,100,60,false,"%"},{"NOISE TONE",0,100,75,false,"%"},
    {"BRIGHT",0,100,65,false,"%"},{"DRIVE",0,100,0,false,"%"},{"STRIKE",0,100,50,false,"%"},{"CHOKE",0,100,80,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta ohhM[16] = {
    {"TUNE",-12,12,0,false,"st"},{"SPREAD",0,36,10,false,"%"},{"P DECAY",1,300,60,false,"ms"},{"CURVE",1,100,50,false,"%"},
    {"ATTACK",0.02,10,0.5,true,"ms"},{"DECAY",50,2500,600,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,1200,150,true,"ms"},
    {"METAL MIX",0,100,70,false,"%"},{"NOISE LVL",0,100,60,false,"%"},{"NOISE TONE",0,100,70,false,"%"},
    {"BRIGHT",0,100,60,false,"%"},{"DRIVE",0,100,0,false,"%"},{"STRIKE",0,100,50,false,"%"},{"CHOKE",0,100,80,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta rideM[16] = {
    {"TUNE",-12,12,0,false,"st"},{"SPREAD",0,24,6,false,"%"},{"P DECAY",1,400,80,false,"ms"},{"CURVE",1,100,50,false,"%"},
    {"ATTACK",0.05,20,0.8,true,"ms"},{"DECAY",200,6000,2200,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",10,3000,400,true,"ms"},
    {"PARTIALS",0,100,75,false,"%"},{"NOISE LVL",0,100,45,false,"%"},{"NOISE TONE",0,100,70,false,"%"},
    {"DAMP",0,100,40,false,"%"},{"DRIVE",0,100,0,false,"%"},{"STRIKE",0,100,50,false,"%"},{"RESO",0,100,45,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta crashM[16] = {
    {"TUNE",-12,12,0,false,"st"},{"SPREAD",0,36,12,false,"%"},{"P DECAY",1,500,100,false,"ms"},{"CURVE",1,100,45,false,"%"},
    {"ATTACK",0.05,20,0.6,true,"ms"},{"DECAY",200,8000,3200,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",10,3000,500,true,"ms"},
    {"PARTIALS",0,100,80,false,"%"},{"NOISE LVL",0,100,55,false,"%"},{"NOISE TONE",0,100,75,false,"%"},
    {"DAMP",0,100,35,false,"%"},{"DRIVE",0,100,0,false,"%"},{"STRIKE",0,100,50,false,"%"},{"RESO",0,100,50,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta tomM[16] = {
    {"TUNE",-24,12,0,false,"st"},{"PITCH AMT",0,48,22,false,"st"},{"P DECAY",2,700,220,false,"ms"},{"CURVE",1,100,38,false,"%"},
    {"ATTACK",0.05,20,0.8,true,"ms"},{"DECAY",50,2500,500,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,1500,150,true,"ms"},
    {"SUB LVL",0,100,45,false,"%"},{"CLICK LVL",0,100,30,false,"%"},{"NOISE LVL",0,100,30,false,"%"},{"NOISE TONE",0,100,50,false,"%"},
    {"N DECAY",2,500,60,false,"ms"},{"RESO",0,100,55,false,"%"},{"DRIVE",0,100,0,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta membM[16] = {
    {"TUNE",-24,12,0,false,"st"},{"PITCH AMT",0,48,20,false,"st"},{"P DECAY",2,700,200,false,"ms"},{"CURVE",1,100,42,false,"%"},
    {"ATTACK",0.05,20,0.7,true,"ms"},{"DECAY",50,3000,600,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,1500,150,true,"ms"},
    {"MEMB MIX",0,100,65,false,"%"},{"DAMP",0,100,45,false,"%"},{"NOISE LVL",0,100,35,false,"%"},{"NOISE TONE",0,100,55,false,"%"},
    {"DRIVE",0,100,0,false,"%"},{"RESO",0,100,60,false,"%"},{"FM",0,100,25,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta metM[16] = {
    {"TUNE",-24,24,0,false,"st"},{"BEND",-12,12,0,false,"st"},{"BEND TIME",1,2000,200,false,"ms"},{"CURVE",0,100,50,false,"%"},
    {"ATTACK",0.05,20,0.1,true,"ms"},{"DECAY",50,8000,900,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,2500,250,true,"ms"},
    {"MATERIAL",0,100,30,false,"%"},{"INHARM",0,100,35,false,"%"},{"EXCITE",0,100,40,false,"%"},{"HARDNESS",0,100,60,false,"%"},
    {"DAMP",0,100,40,false,"%"},{"DRIVE",0,100,0,false,"%"},{"OSC LAYER",0,100,0,false,"%"},{"VEL>AMP",0,100,80,false,"%"}
};
static const ParamMeta sfxM[16] = {
    {"TUNE",-36,36,0,false,"st"},{"SWEEP",-72,72,30,false,"st"},{"SWEEP TIME",1,4000,400,false,"ms"},{"CURVE",0,100,50,false,"%"},
    {"ATTACK",0.05,2000,1,true,"ms"},{"DECAY",20,8000,700,true,"ms"},{"SUSTAIN",-60,0,-60,true,"dB"},{"RELEASE",5,5000,300,true,"ms"},
    {"FM",0,100,20,false,"%"},{"NOISE",0,100,20,false,"%"},{"CUTOFF",0,100,75,false,"%"},
    {"F.ENV",-100,100,30,false,"%"},{"DRIVE",0,100,0,false,"%"},{"CRUSH",0,100,0,false,"%"},{"RESO",0,100,30,false,"%"},{"VEL>AMP",0,100,70,false,"%"}
};

const ParamMeta* metaForPart (int part) {
    switch (part) {
        case Kick: return kickM; case Snare: return snareM;
        case ClosedHat: return chhM; case OpenHat: return ohhM;
        case Ride: return rideM; case Crash: return crashM;
        case Tom1: case Tom2: case LowTom: return tomM;
        case Membrane: return membM; case Metallic: return metM;
        case Sfx: return sfxM; default: return snareM;
    }
}
bool isEnvelopeSlot (int, int slot) { return slot >= 4 && slot <= 7; }

bool isMembranePart (int part) {
    return part == Kick || part == Snare || part == Tom1 || part == Tom2
        || part == LowTom || part == Membrane;
}

static const ParamMeta noiseFxMeta = { "NZ>FX", 0, 100, 0, false, "%" }; // snare: noise share sent through the part FX
static const ParamMeta punchMeta = { "PUNCH", 0, 100, 55, false, "%" };
// SFX extras: LFO (wobble / trill / vibrato) + FM modulator ratio
static const ParamMeta sfxLfoRate = { "LFO RATE", 0.1f, 30, 6, false, "Hz" };
static const ParamMeta sfxLfoPitch = { "LFO>PITCH", 0, 24, 0, false, "st" };
static const ParamMeta sfxLfoFilt = { "LFO>FILT", 0, 100, 0, false, "%" };
static const ParamMeta sfxRatio = { "FM RATIO", 0.25f, 8, 2, false, "ratio" };
static const ParamMeta boostAmtMeta = { "BOOST AMT", 0, 24, 6, false, "dB" };
static const ParamMeta bFk = { "BOOST FREQ", 30, 6000, 55, false, "Hz" };
static const ParamMeta bFs = { "BOOST FREQ", 30, 6000, 190, false, "Hz" };
static const ParamMeta bFt = { "BOOST FREQ", 30, 6000, 100, false, "Hz" };
static const ParamMeta bFm = { "BOOST FREQ", 30, 6000, 150, false, "Hz" };

const ParamMeta* advMetaForPart (int part, int slot) {
    if (part == Sfx) {
        if (slot == 16) return &sfxLfoRate;
        if (slot == 17) return &sfxLfoPitch;
        if (slot == 18) return &sfxLfoFilt;
        if (slot == 19) return &sfxRatio;
        return nullptr;
    }
    if (! isMembranePart (part)) return nullptr;
    if (slot == 16) return &punchMeta;
    if (slot == 17) {
        if (part == Kick) return &bFk;
        if (part == Snare) return &bFs;
        if (part == Membrane) return &bFm;
        return &bFt; // toms
    }
    if (slot == 18) return &boostAmtMeta;
    if (slot == 19 && part == Snare) return &noiseFxMeta;
    return nullptr;
}
const ParamMeta* slotMeta (int part, int slot) {
    if (slot < 16) return &metaForPart (part)[slot];
    return advMetaForPart (part, slot);
}

void defaultParamsForPart (int part, PartParams& out) {
    const ParamMeta* m = metaForPart (part);
    for (int i = 0; i < 20; ++i) out.p[i] = 0.f;
    for (int i = 0; i < 16; ++i) out.p[i] = m[i].def;
    for (int s = 16; s < 20; ++s)
        if (auto* am = advMetaForPart (part, s)) out.p[s] = am->def;
    if (isMembranePart (part)) out.p[18] = 0.f; // boost off by default; dial to taste
    out.wave = 0; out.satMode = 0;
    // sensible noise types per part
    switch (part) {
        case Kick: out.noiseType = 0; break;
        case Snare: out.noiseType = 0; break;
        case ClosedHat: case OpenHat: case Ride: case Crash: out.noiseType = 2; break;
        case Tom1: case Tom2: case LowTom: out.noiseType = 0; break;
        case Membrane: out.noiseType = 6; break;
        case Metallic: out.noiseType = 0; break;
        case Sfx: out.noiseType = 0; break;
        default: out.noiseType = 0; break;
    }
    if (part == Membrane) out.wave = 4;
    if (part == ClosedHat || part == OpenHat || part == Ride || part == Crash) out.wave = 3;
}

// ---------------- voice ----------------
void DrumVoice::trigger (int partType, const PartParams& pr, float vel, float tuneOffsetSt) {
    part = partType; velocity = juce::jlimit (0.05f, 1.f, vel);
    active = true; fbState = 0.f; choke = 0.f;
    tuneOff = tuneOffsetSt; lastTuneF = 9999.f;
    noise.reset(); bodyFilt.reset(); noiseFilt.reset(); sat.reset();
    boost.reset(); snBridge.reset(); cymA.reset(); cymB.reset(); cymC.reset(); cymD.reset(); cymE.reset(); snTailHp.reset(); snTailLp.reset(); snSnapBp.reset();
    hitT = 0.f; postFx = 0.f; crushCnt = 0; crushHold = 0.f;
    for (auto& m : mtY) m[0] = m[1] = 0.f;
    mtExLp = mtFollow = 0.f; sfxLfoPh = 0.f; sfxB = 0.f; mtOscPh[0] = mtOscPh[1] = 0.f;
    // snare: fresh fund/tick phases per hit (slight jitter kills machine-gun)
    snBodyPh = noise.rng.nextFloat();
    snPingPh = 0.f; snFmPh = noise.rng.nextFloat(); snCompEnv = 0.f;
    float tune = pr.p[0] + tuneOffsetSt;
    float pAmt = pr.p[1], pDec = pr.p[2] / 1000.f, curve = pr.p[3] / 100.f;
    float a = pr.p[4] / 1000.f, d = pr.p[5] / 1000.f;
    // snare: amp env shapes the BODY only; the noise layer runs its own
    // envelope (N DECAY) and keeps the voice alive on its own.
    float susDb = pr.p[6]; float sus = susDb <= -59.f ? 0.f : dbToGain (susDb);
    float r = pr.p[7] / 1000.f;
    if (partType == Metallic) d *= 4.f; // modes carry the decay; the amp env only gates
    amp.set (a, d, sus, r); amp.reset(); amp.noteOn();
    pitch.trigger (pAmt, pDec, curve);
    float baseMidi = 0.f;
    switch (partType) {
        case Kick: baseMidi = 43; break;       // G2 98 Hz: kick TUNE -10..-15 = 45..55 Hz
        case Snare: baseMidi = 57; break;       // ~A3 220 Hz + tune: 150-350 Hz window
        case ClosedHat: baseMidi = 84; break;
        case OpenHat: baseMidi = 82; break;
        case Ride: baseMidi = 80; break;
        case Crash: baseMidi = 78; break;
        case Tom1: baseMidi = 57; break;
        case Tom2: baseMidi = 52; break;
        case LowTom: baseMidi = 45; break;
        case Membrane: baseMidi = 55; break;
        case Metallic: baseMidi = 76; break;
        case Sfx: baseMidi = 64; break;
        default: baseMidi = 60; break;
    }
    float baseF = midiToFreq (baseMidi + tune);
    baseMidiF = baseMidi; // stored for live tune tracking in render
    osc.freq = baseF; osc2.freq = baseF * 1.01f; sub.freq = baseF * 0.5f;
    osc.wave = pr.wave; osc2.wave = pr.wave;
    sub.wave = (pr.wave == 4) ? 0 : pr.wave; // WAVE chooses body AND sub character
    float fmHeadroom = (pr.wave == 4) ? 4.0f : 0.f; // FM wave must FM on every part, not just metallic
    osc.reset (0.f); osc2.reset (0.13f); sub.reset (0.f);
    osc.fmAmt = pr.p[14] / 100.f * 6.f * (partType == Membrane ? 1.f : 0.f);
    osc.driftAmt = 0.3f;
    // metallic click ping: kick and snare share the bright inharmonic tone
    if (partType == Kick || partType == Snare) {
        float b = 900.f * std::pow (2.f, tune / 12.f);
        pingFreq[0] = b * 3.98f; pingFreq[1] = b * 7.62f; pingFreq[2] = b * 12.4f;
    } else if (partType == Tom1 || partType == Tom2 || partType == LowTom) {
        float b = 700.f * std::pow (2.f, tune / 12.f);
        pingFreq[0] = b * 2.99f; pingFreq[1] = b * 6.1f; pingFreq[2] = b * 10.2f;
    } else {
        pingFreq[0] = pingFreq[1] = pingFreq[2] = 0.f;
    }
    pingPh[0] = 0.f; pingPh[1] = 0.3f; pingPh[2] = 0.6f;
    sat.mode = pr.satMode; sat.drive = pr.p[12] / 100.f * ((partType == Kick || partType == Snare) ? 1.f : 0.8f);
    // modal partials for metallic/membrane/cymbals.
    // Hats use a fundamental-free dense high set (wash, not bells).
    {
        static const float metRatios[8] = { 1.f, 1.483f, 2.117f, 2.932f, 4.17f, 5.43f, 6.79f, 8.93f };
        static const float membRatios[8] = { 1.f, 1.59f, 2.14f, 2.65f, 3.16f, 4.06f, 4.70f, 5.50f };
        static const float hatRatios[8] = { 2.71f, 3.38f, 4.16f, 5.02f, 6.10f, 7.44f, 9.02f, 11.1f };
        const float* ratios = metRatios;
        float fundW = 1.f, decMul = 1.f;
        if (partType == Membrane) { ratios = membRatios; }
        else if (partType == ClosedHat || partType == OpenHat) { ratios = hatRatios; decMul = 1.6f; }
        else if (partType == Ride) { fundW = 0.6f; }
        else if (partType == Crash) { fundW = 0.45f; }
        for (int i = 0; i < 8; ++i) {
            // ±0.3% per-hit detune: breaks machine regularity, keeps tuning
            partials[i].ratio = ratios[i] * (1.f + (noise.rng.nextFloat() - 0.5f) * 0.006f);
            partials[i].ph = 0.02f * i;
            partials[i].t = 0.f;
            partials[i].dec = juce::jmax (0.03f, d * (1.2f - 0.10f * i) * decMul);
            partials[i].amp = (i == 0 ? fundW : 1.f) / (1.f + i * 0.7f);
        }
    }
    float velAmp = pr.p[15] / 100.f;
    float vGain = (1.f - velAmp) + velAmp * velocity;
    panL = panR = vGain;
    // cymbals: free-running metal phases per hit (no identical machine-gun hits)
    for (auto& ph : cymPh) ph = noise.rng.nextFloat();
    bellPh[0] = bellPh[1] = bellPh[2] = 0.f;
}

// helper
static int noiseTypeCache (const PartParams& pr) { return juce::jlimit (0, 7, pr.noiseType); }

float DrumVoice::render() { return 0.f; }

// ---------------- snare ----------------
// Three layers, each with its own clock-driven envelope:
//   BODY  : pitched shell (WAVE osc + 1.59x overtone) + SUB, fast pitch drop,
//           amp ADSR, PUNCH overshoot, BOOST bell, DRIVE, punchy COMP
//           (slow attack lets the transient through).
//   SNAP  : 2-3 ms band-passed noise crack, level = PUNCH. Rides with the body.
//   NOISE : selected noise type, HP/LP band set by TONE, own N DECAY env.
//           Clean: no drive/comp, and it bypasses the part FX chain (postFx).
float DrumVoice::renderSnare (const PartParams& pr) {
    postFx = 0.f;
    const float sr = sampleRate, t = hitT;
    hitT += 1.f / sr;
    if (choke > 0.f) { choke -= 1.f; if (choke <= 0.f) amp.noteOff(); }

    const float bodyMix = pr.p[8] / 100.f, noiseLvl = pr.p[9] / 100.f;
    const float nDec = juce::jmax (0.01f, pr.p[10] / 1000.f);
    const float tone = juce::jlimit (0.f, 1.f, pr.p[11] / 100.f);
    const float drive = juce::jlimit (0.f, 1.f, pr.p[12] / 100.f);
    const float ca = juce::jlimit (0.f, 1.f, pr.p[13] / 100.f);
    const float nzHp = juce::jlimit (20.f, 8000.f, pr.p[14]);
    const float punch = juce::jlimit (0.f, 1.f, pr.p[16] / 100.f);
    const float bodyDec = juce::jmax (0.02f, pr.p[5] / 1000.f);

    // noise envelope: ~0.4 ms rise, exp fall reaching -30 dB at N DECAY
    float nEnv = (1.f - std::exp (-t / 0.0004f)) * std::exp (-t * 3.45f / nDec);
    bool bodyOn = amp.isActive();
    float env = bodyOn ? amp.next() : 0.f;
    if (! amp.isActive() && nEnv * noiseLvl < 1.0e-4f && t > 0.005f) { active = false; return 0.f; }

    // ---- BODY ----
    float f0 = midiToFreq (baseMidiF + pr.p[0] + tuneOff) * std::pow (2.f, pitch.next() / 12.f);
    osc.wave = pr.wave;
    osc.fmAmt = (pr.wave == 4) ? 4.f : 0.f;
    osc.freq = f0;
    float o1 = osc.next();
    // tame the edges of saw/square/FM without losing the character
    if (pr.wave != 0) o1 = bodyFilt.process (o1, juce::jmin (16000.f, f0 * 9.f), 0.7f, 0);
    snBodyPh += f0 * 1.59f / sr; if (snBodyPh >= 1.f) snBodyPh -= 1.f;   // shell overtone
    const float twoPi = juce::MathConstants<float>::twoPi;
    float over = std::sin (twoPi * snBodyPh) * std::exp (-t / (bodyDec * 0.3f));
    // shell noise: a band of noise around the body's upper modes rides the
    // body envelope and goes through the same saturator. Bridges the gap
    // between the body tone and the wire band so the two read as one drum.
    float shellNz = snBridge.process (noise.white(), juce::jmin (f0 * 2.5f, sr * 0.4f), 1.2f, 1)
                  * noiseLvl * std::exp (-t / (bodyDec * 0.35f));
    float shell = (o1 + over * 0.35f) * bodyMix + shellNz * 0.6f;
    // kick-style punch: early overshoot driven into soft saturation. The
    // first ~15 ms sit dense and loud ("umph"); DRIVE raises the push.
    float pre = shell * (0.9f + drive * 6.f) * (1.f + punch * 1.5f * std::exp (-t / 0.015f));
    float sat = std::tanh (pre);
    // ...plus a post-saturation overshoot so the hit stands out from the body
    float body = sat * (1.f + punch * 0.9f * std::exp (-t / 0.02f)) * env;
    float bDb = pr.p[18];
    if (bDb > 0.05f) {
        boost.set (juce::jlimit (30.f, 6000.f, pr.p[17]), 1.1f, bDb, sr);
        body = boost.process (body);
    }

    // ---- SNAP ---- (white burst, band set by TONE)
    float snap = snSnapBp.process (noise.white(), 1800.f + 3500.f * tone, 1.2f, 1)
               * std::exp (-t / 0.0025f) * punch * 1.5f;
    float x = body * 0.8f + snap * 0.6f;

    // ---- NOISE ---- (wires)
    float n = noise.next (noiseTypeCache (pr), tone);
    float lpF = 3000.f * std::pow (2.f, tone * 2.5f); // TONE: 3 kHz .. 17 kHz top
    n = snTailLp.process (snTailHp.process (n, nzHp, 0.707f, 2), lpF, 0.707f, 0); // NOISE HP sets the bottom
    // wire buzz: the head drives the wires, so the noise is amplitude-
    // modulated by the (saturated) body while it rings
    n *= 1.f + 0.6f * sat * env;
    float nz = n * nEnv * noiseLvl * 1.4f;

    // COMP = glue: ONE detector on body + wires, same gain on both, so
    // they swell and duck together. 6 ms attack keeps the crack.
    if (ca > 0.001f) {
        float a = std::abs (x + nz);
        float coef = (a > snCompEnv) ? (1.f - std::exp (-1.f / (sr * 0.006f)))
                                     : (1.f - std::exp (-1.f / (sr * 0.070f)));
        snCompEnv += (a - snCompEnv) * coef;
        float thrDb = -6.f - 18.f * ca, ratio = 1.f + 5.f * ca;
        float eDb = 20.f * std::log10 (snCompEnv + 1.0e-6f);
        float grDb = juce::jmax (0.f, eDb - thrDb) * (1.f - 1.f / ratio);
        float g = dbToGain (-grDb + ca * 7.f);
        x *= g; nz *= g;
    }

    float velAmp = pr.p[15] / 100.f;
    float vGain = (1.f - velAmp) + velAmp * velocity;
    // NZ>FX: share of the noise that goes through the part FX; rest stays clean
    float send = juce::jlimit (0.f, 1.f, pr.p[19] / 100.f);
    postFx = nz * (1.f - send) * vGain;
    return (x + nz) * vGain;
}

// ---------------- cymbals (CHH / OHH / RIDE / CRASH) ----------------
// 808-style metal: 6 band-limited squares at inharmonic ratios (SPREAD
// fans them), ring-modulated pairs for ride/crash density, 24 dB HP +
// a clank band-pass, a noise wash that lasts the whole decay, a short
// strike transient, and a tail LP that darkens over the decay.
//   METAL/PARTIALS = metal level, NOISE LVL/TONE = wash level/brightness,
//   BRIGHT (hats) raises the HP, DAMP (ride/crash) darkens + shortens the
//   tail, RESO (ride/crash) = bell (ride) / ring-mod shimmer (crash).
float DrumVoice::renderCymbal (const PartParams& pr) {
    postFx = 0.f;
    if (choke > 0.f) { choke -= 1.f; if (choke <= 0.f) amp.noteOff(); }
    float env = amp.next();
    if (! amp.isActive()) { active = false; return 0.f; }
    const float sr = sampleRate, t = hitT;
    hitT += 1.f / sr;

    const bool hat = (part == ClosedHat || part == OpenHat);
    const float tuneMult = std::pow (2.f, (pr.p[0] + tuneOff) / 12.f);
    const float spread = juce::jlimit (0.f, 1.f, pr.p[1] / 36.f);
    const float metalMix = pr.p[8] / 100.f, noiseLvl = pr.p[9] / 100.f;
    const float tone = juce::jlimit (0.f, 1.f, pr.p[10] / 100.f);
    const float tb = juce::jlimit (0.f, 1.f, pr.p[11] / 100.f);   // hats: BRIGHT, ride/crash: DAMP
    const float drive = juce::jlimit (0.f, 1.f, pr.p[12] / 100.f);
    const float reso = hat ? 0.f : juce::jlimit (0.f, 1.f, pr.p[14] / 100.f);
    const float userD = juce::jmax (0.01f, pr.p[5] / 1000.f);

    // ---- metal bank ----
    static const float f808[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f };
    const float scale = hat ? 1.f : (part == Ride ? 1.12f : 0.92f);
    auto blep = [] (float ph, float dt) {
        if (ph < dt) { ph /= dt; return ph + ph - ph * ph - 1.f; }
        if (ph > 1.f - dt) { ph = (ph - 1.f) / dt; return ph * ph + ph + ph + 1.f; }
        return 0.f;
    };
    float sq[6], metal = 0.f;
    for (int i = 0; i < 6; ++i) {
        float f = f808[i] * scale * tuneMult * (1.f + spread * 0.12f * ((float) i - 2.5f));
        float dt = juce::jmin (0.49f, f / sr);
        cymPh[i] += dt; if (cymPh[i] >= 1.f) cymPh[i] -= 1.f;
        float ph2 = cymPh[i] + 0.5f; if (ph2 >= 1.f) ph2 -= 1.f;
        sq[i] = (cymPh[i] < 0.5f ? 1.f : -1.f) + blep (cymPh[i], dt) - blep (ph2, dt);
        metal += sq[i];
    }
    metal *= 1.f / 6.f;
    if (! hat) {
        float ring = (sq[0] * sq[3] + sq[1] * sq[4] + sq[2] * sq[5]) * (1.f / 3.f);
        float rm = (part == Crash) ? 0.45f + 0.4f * reso : 0.35f;
        metal = metal * (1.f - rm) + ring * rm;
    }
    float hpF = (part == ClosedHat) ? 5500.f + 4000.f * tb
              : (part == OpenHat)   ? 4500.f + 3500.f * tb
              : (part == Ride)      ? 3200.f : 2400.f;
    hpF = juce::jmin (hpF * juce::jmin (tuneMult, 1.6f), sr * 0.42f);
    float m = cymB.process (cymA.process (metal, hpF, 0.75f, 2), hpF, 0.75f, 2);
    // clank: narrow band-pass (wide skirts let the 205-800 Hz square fundamentals through)
    float clank = cymC.process (metal, juce::jmin (3400.f * tuneMult, sr * 0.4f), 3.f + 5.f * reso, 1);
    m += clank * (hat ? 0.15f : 0.3f);

    // ---- noise wash ----
    float n = noise.next (noiseTypeCache (pr), tone);
    float nHP = juce::jmin (hpF * (0.55f + 0.9f * tone), sr * 0.42f);
    n = cymD.process (n, nHP, 0.7f, 2);

    // ---- ride bell ----
    float bell = 0.f;
    if (part == Ride && reso > 0.001f) {
        static const float br[3] = { 1.f, 1.49f, 2.17f };
        for (int i = 0; i < 3; ++i) {
            bellPh[i] += 820.f * tuneMult * br[i] / sr; if (bellPh[i] >= 1.f) bellPh[i] -= 1.f;
            bell += std::sin (juce::MathConstants<float>::twoPi * bellPh[i]) / (1.f + i);
        }
        bell *= reso * 0.12f * std::exp (-t / (userD * 0.6f)); // accent, not the main tone
    }

    // ---- strike + crash bloom ----
    float strike = std::exp (-t / (hat ? 0.0035f : 0.006f)) * pr.p[13] / 50.f; // STRIKE (50 = stock)
    float bloom = (part == Crash) ? 0.55f + 0.45f * (1.f - std::exp (-t / 0.012f)) : 1.f;
    float sig = m * metalMix * bloom * (0.7f + 1.3f * strike)
              + n * noiseLvl * (0.45f + 1.6f * strike) * (hat ? 1.f : 0.8f)
              + bell;

    // ---- tail darkening: LP glides down over the decay, DAMP pulls it lower ----
    float lpEnd = hat ? 9000.f + 6000.f * tb : 9000.f - 5500.f * tb;
    float lpCut = 18000.f - (18000.f - lpEnd) * juce::jmin (1.f, t / userD);
    lpCut *= 0.65f + 0.35f * velocity; // soft hits are darker
    sig = cymE.process (sig, lpCut, 0.6f, 0);
    if (! hat) sig *= std::exp (-t * tb * 2.f / userD); // DAMP shortens the ring

    if (drive > 0.0005f) {
        float g = 1.f + drive * 9.f;
        sig += juce::jmin (1.f, drive * 4.f) * (std::tanh (sig * g) / std::tanh (g) - sig);
    }
    float velAmp = pr.p[15] / 100.f;
    float vGain = (1.f - velAmp) + velAmp * velocity;
    return sig * env * vGain * 1.6f;
}

float DrumVoice::renderWithParams (const PartParams& pr) {
    if (! active) return 0.f;
    if (part == Snare) return renderSnare (pr);
    if (part == ClosedHat || part == OpenHat || part == Ride || part == Crash) return renderCymbal (pr);
    if (part == Metallic) return renderMetal (pr);
    if (part == Sfx) return renderSfx (pr);
    postFx = 0.f;
    if (choke > 0.f) { choke -= 1.f; if (choke <= 0.f) amp.noteOff(); }
    float env = amp.next();
    if (! amp.isActive()) { active = false; return 0.f; }
    const float tHit = hitT; // envelopes below run on this, never on pitch.t
    hitT += 1.f / sampleRate;

    float pOffSt = pitch.next(); // strong pitch envelope
    float semis = pOffSt;
    float fMul = std::pow (2.f, semis / 12.f);
    // wave + tune are LIVE (not frozen at trigger): sustained notes retune/retimbre
    osc.wave = pr.wave; osc2.wave = pr.wave;
    sub.wave = (pr.wave == 4) ? 0 : pr.wave;
    float fmHead = (pr.wave == 4) ? 4.0f : 0.f;
    float fmExtra = pr.p[14] / 100.f * 6.f * (part == Membrane ? 1.f : 0.f);
    osc.fmAmt = juce::jmax (fmHead, fmExtra); osc2.fmAmt = osc.fmAmt;
    float liveBaseF = midiToFreq (baseMidiF + pr.p[0] + tuneOff);
    float f0 = liveBaseF * fMul;
    osc.freq = f0;         float o1 = osc.next();
    osc2.freq = f0 * 1.008f; float o2 = osc2.next();
    // kick SUB OCT: 0 = unison, -1/-2 = octaves down; other parts sit an octave down
    float subMul = (part == Kick) ? std::pow (2.f, std::round (juce::jlimit (-2.f, 0.f, pr.p[9]))) : 0.5f;
    sub.freq = f0 * subMul; float sb = sub.next();

    // WAVE must be unmistakable: per-wave filter brightness + raw osc bleed
    // straight to the mix (the body lowpass alone would swallow the difference)
    float waveMult = 1.f, waveBleed = 0.f;
    if (pr.wave == 1)      { waveMult = 0.9f;  waveBleed = o1 * 0.05f; }
    else if (pr.wave == 2) { waveMult = 1.25f; waveBleed = o1 * 0.12f; }
    else if (pr.wave == 3) { waveMult = 1.35f; waveBleed = o1 * 0.14f; }
    else if (pr.wave == 4) { waveMult = 1.15f; waveBleed = o1 * 0.15f; }

    float resoQ = 0.5f + (pr.p[15] >= 0 ? 0.f : 0.f);
    // per-part reso slot differs; use helper: slot 15 for kick, 13/14 etc. Use max of candidate slots mapped:
    float reso = 0.4f, drive = pr.p[12] / 100.f, noiseLvl = 0.3f, noiseTone = 0.5f, nDec = 0.1f;
    float bodyMix = 0.7f, subMix = 0.f, clickMix = 0.f, metalMix = 0.f, fmMix = 0.f, damp = 0.4f, fb = 0.f, width = 0.f;
    float punchAmt = 0.f;
    switch (part) {
        case Kick:     reso = pr.p[15]/100.f; subMix = pr.p[8]/100.f; clickMix = pr.p[10]/100.f; noiseLvl = pr.p[13]/100.f; nDec = pr.p[14]/1000.f; drive = pr.p[12]/100.f; bodyMix = 0.9f; punchAmt = pr.p[16]/100.f; break;
        case ClosedHat: case OpenHat: reso = 0.5f; metalMix = pr.p[8]/100.f; noiseLvl = pr.p[9]/100.f; noiseTone = pr.p[10]/100.f; damp = pr.p[11]/100.f; drive = pr.p[12]/100.f; break;
        case Ride: case Crash: reso = pr.p[14]/100.f; metalMix = pr.p[8]/100.f; noiseLvl = pr.p[9]/100.f; noiseTone = pr.p[10]/100.f; damp = pr.p[11]/100.f; drive = pr.p[12]/100.f; break;
        case Tom1: case Tom2: case LowTom: reso = pr.p[13]/100.f; subMix = pr.p[8]/100.f; clickMix = pr.p[9]/100.f; noiseLvl = pr.p[10]/100.f; noiseTone = pr.p[11]/100.f; nDec = pr.p[12]/1000.f; drive = pr.p[14]/100.f; bodyMix = 0.9f; punchAmt = pr.p[16]/100.f; break;
        case Membrane: reso = pr.p[13]/100.f; metalMix = pr.p[8]/100.f; damp = pr.p[9]/100.f; noiseLvl = pr.p[10]/100.f; noiseTone = pr.p[11]/100.f; drive = pr.p[12]/100.f; fmMix = pr.p[14]/100.f; punchAmt = pr.p[16]/100.f; break;
        default: break;
    }
    (void) resoQ; (void) width;

    // resonant body filter: cutoff tracks pitch env well above the fundamental
    // so wave harmonics (saw/square/FM) survive instead of being stripped
    float trackF = f0 * (3.5f + reso * 4.f) * waveMult;
    float q = 0.5f + reso * 12.f;
    float body = bodyFilt.process (o1 * 0.6f + o2 * 0.4f, juce::jlimit (40.f, 18000.f, trackF * 2.f), q, 0);

    // BOOST knob: resonant bell on the shell (kick/snare/toms/membrane)
    if (part == Kick || part == Snare || part == Tom1 || part == Tom2 || part == LowTom || part == Membrane) {
        float bDb = pr.p[18];
        if (bDb > 0.05f) {
            boost.set (juce::jlimit (30.f, 6000.f, pr.p[17]), 1.1f, bDb, sampleRate);
            body = boost.process (body);
        }
    }

    // modal partials: membrane only (metal/cymbals/SFX have their own renderers)
    float modal = 0.f;
    if (part == Membrane) {
        for (int i = 0; i < 8; ++i) {
            auto& pt = partials[i];
            pt.t += 1.f / sampleRate;
            float d = juce::jmax (0.02f, pt.dec * (1.f - damp * 0.7f));
            float a = pt.amp * std::exp (-pt.t / d);
            float fr = f0 * pt.ratio;
            pt.ph += fr / sampleRate; if (pt.ph >= 1.f) pt.ph -= 1.f;
            modal += std::sin (2.f * juce::MathConstants<float>::pi * pt.ph) * a;
        }
        modal *= 0.22f;
    }

    // noise with own decay-ish envelope (reuse amp env scaled) + resonant noise filter
    float nz = noise.next (noiseTypeCache (pr), noiseTone);
    float sig = 0.f;
    float snarePostCrack = 0.f; // click energy added AFTER saturation so it always cuts
    float snareCleanNz = 0.f; // snare wash added AFTER everything: clean noise, envelope only
    {
        float nCut = 800.f + 9000.f * noiseTone * noiseTone + f0 * 0.5f;
        float nQ = 0.5f + reso * 9.f; // noise is resonant too (requirement)
        float nzF = noiseFilt.process (nz, nCut, nQ, 1);
        // snare: same slot, but the noise stays raw: no filter, own release
        float nzSrc = nzF;
        if (part == Snare) nzSrc = nz * std::exp (-tHit / juce::jmax (0.008f, nDec));
        // wash splits out for snare: clean signal, joins after clip/comp/sat
        // N DECAY shapes the wash on kick/toms (amp env alone ignored the knob)
        if (part == Kick || part == Tom1 || part == Tom2 || part == LowTom)
            nzSrc *= std::exp (-tHit / juce::jmax (0.002f, nDec));
        float washNz = nzSrc * noiseLvl;
        if (part == Snare) { snareCleanNz = washNz; washNz = 0.f; }
        // clicks: raw noise burst + metallic inharmonic ping (one tone per part)
        float click = 0.f, ping = 0.f;
        if (part == Kick || part == Snare || part == Tom1 || part == Tom2 || part == LowTom) {
            float t = tHit;
            float cDec = (part == Kick) ? juce::jmax (0.001f, pr.p[11] / 1000.f) // CLICK DEC
                                        : juce::jmax (0.004f, nDec + 0.004f);
            click = nz * clickMix * std::exp (-t / cDec);
            float curTune = pr.p[0] + tuneOff; // click pitch follows tune live
            if (curTune != lastTuneF) {
                lastTuneF = curTune;
                if (part == Kick || part == Snare) {
                    float b = 900.f * std::pow (2.f, curTune / 12.f);
                    pingFreq[0] = b * 3.98f; pingFreq[1] = b * 7.62f; pingFreq[2] = b * 12.4f;
                } else {
                    float b = 700.f * std::pow (2.f, curTune / 12.f);
                    pingFreq[0] = b * 2.99f; pingFreq[1] = b * 6.1f; pingFreq[2] = b * 10.2f;
                }
            }
            if (pingFreq[0] > 0.f && clickMix > 0.001f) {
                float penv = std::exp (-t / juce::jmax (0.002f, cDec * 0.5f));
                const float w[3] = { 0.5f, 0.3f, 0.2f };
                for (int i = 0; i < 3; ++i) {
                    pingPh[i] += pingFreq[i] / sampleRate;
                    if (pingPh[i] >= 1.f) pingPh[i] -= 1.f;
                    ping += std::sin (2.f * juce::MathConstants<float>::pi * pingPh[i]) * w[i];
                }
                ping *= penv * clickMix;
            }
        }

        sig = body * bodyMix + sb * subMix * 0.9f + modal * (0.3f + 0.7f * metalMix)
              + washNz + waveBleed + o1 * fmMix * 0.3f;
        // beater click + ping join AFTER saturation: tanh on the clipped body
        // would flatten them (they read ~-50 dB in the 1-4 kHz band before)
        snarePostCrack = (click + ping) * 0.8f;
        if (punchAmt > 0.001f && (part == Kick || part == Snare || part == Tom1 || part == Tom2 || part == LowTom || part == Membrane))
            sig += (body * 3.0f + sb * 2.0f) * std::exp (-tHit / 0.012f) * punchAmt; // PUNCH knob
    }

    // snare back-end only: hard clip (DRIVE) + compressor (COMP). Everything
    // above this line is the untouched kick chain.
    if (part == Snare) {
        sig = sig * 0.5f; // gain-stage: kick-chain peaks (~9x) must not flat-top the clipper at DRIVE 0
        float drv = drive;
        float thr = 1.f / (1.f + drv * 6.f);
        sig = juce::jlimit (-thr, thr, sig) * (1.f + drv * 1.5f);
        float ca = juce::jlimit (0.f, 1.f, pr.p[13] / 100.f);
        float peak = std::abs (sig);
        float coef = (peak > snCompEnv) ? (1.f - std::exp (-1.f / (sampleRate * 0.001f)))
                                        : (1.f - std::exp (-1.f / (sampleRate * 0.080f)));
        snCompEnv += (peak - snCompEnv) * coef;
        float cthr = 0.5f - 0.4f * ca;
        float g = 1.f;
        if (snCompEnv > cthr) g = std::pow (cthr / snCompEnv, 0.15f + 0.6f * ca);
        sig = sig * g * (1.f + 0.8f * ca);
    }

    // feedback for SFX (bounded)
    if (fb > 0.001f) {
        float fbs = juce::jlimit (-0.9f, 0.9f, fbState * fb * 0.9f);
        sig += fbs;
        fbState = juce::jlimit (-1.f, 1.f, sig * 0.5f + fbs * 0.5f);
    }

    sat.drive = drive; sat.mode = pr.satMode;
    sig = sat.process (sig);
    sig += snarePostCrack * env; // kick/tom click bypasses saturation: always audible
    if (part == Snare) sig += snareCleanNz; // wash bypasses clip/comp/sat: clean, envelope only
    return sig * env * panL * 0.9f;
}

} // namespace mds

namespace mds {

// ---------------- metallic percussion: modal resonator bank ----------------
// A struck object: a mallet/noise exciter (EXCITE = burst length + level,
// HARDNESS = exciter brightness) drives 8 two-pole resonators. MATERIAL
// morphs the mode ratios cowbell -> bar -> bell -> plate -> gong, INHARM
// stretches them, DECAY is the fundamental's ring time, DAMP shortens the
// upper modes. BEND (+-12 st, default none) glides the modes. OSC LAYER
// adds two oscillators (WAVE) on the lowest modes: 808-style cowbell/agogo.
float DrumVoice::renderMetal (const PartParams& pr) {
    postFx = 0.f;
    if (choke > 0.f) { choke -= 1.f; if (choke <= 0.f) amp.noteOff(); }
    float env = amp.next();
    if (! amp.isActive()) { active = false; return 0.f; }
    const float sr = sampleRate, t = hitT;
    hitT += 1.f / sr;

    static const float sets[5][8] = {
        { 1.f, 1.48f, 2.0f,  2.96f, 3.6f,  4.4f,  5.6f,  6.8f },   // cowbell / agogo
        { 1.f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.8f, 31.9f }, // free bar (glock / anvil)
        { 0.5f, 1.f, 1.183f, 1.506f, 2.0f, 2.514f, 2.662f, 3.011f }, // bell
        { 1.f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.598f }, // plate
        { 1.f, 1.07f, 1.52f, 1.68f, 2.03f, 2.31f, 2.87f, 3.19f },  // gong / tam (dense)
    };
    static const float amps[5][8] = {
        { 1.f, .8f, .35f, .3f, .2f, .15f, .1f, .08f },
        { 1.f, .6f, .4f, .25f, .15f, .1f, .07f, .05f },
        { .7f, 1.f, .8f, .6f, .5f, .4f, .35f, .3f },
        { 1.f, .9f, .8f, .7f, .65f, .6f, .5f, .45f },
        { 1.f, .95f, .9f, .85f, .8f, .75f, .7f, .65f },
    };
    const float mat = juce::jlimit (0.f, 1.f, pr.p[8] / 100.f) * 3.999f;
    const int si = (int) mat; const float sf = mat - (float) si;
    const float stretch = 0.9f + 0.3f * juce::jlimit (0.f, 1.f, pr.p[9] / 100.f);
    const float excite = juce::jlimit (0.f, 1.f, pr.p[10] / 100.f);
    const float hard = juce::jlimit (0.f, 1.f, pr.p[11] / 100.f);
    const float dampAmt = juce::jlimit (0.f, 1.f, pr.p[12] / 100.f);
    const float drive = juce::jlimit (0.f, 1.f, pr.p[13] / 100.f);
    const float layer = juce::jlimit (0.f, 1.f, pr.p[14] / 100.f);
    const float T0 = juce::jmax (0.03f, pr.p[5] / 1000.f);

    // BEND: optional glide of every mode (0 = none, as real metal); not the
    // membrane strike-drop. BEND TIME + bipolar CURVE shape it.
    float bend = pr.p[1] * sweepRemaining (t / juce::jmax (0.001f, pr.p[2] / 1000.f), pr.p[3] / 100.f);
    float f0 = midiToFreq (baseMidiF + pr.p[0] + tuneOff) * std::pow (2.f, bend / 12.f);

    // exciter: unit click + noise burst, low-passed by HARDNESS
    float exLen = 0.0003f + excite * 0.025f;
    // longer bursts pump more energy into high-Q modes: level falls with
    // length so EXCITE changes the strike's character, not the loudness
    float exAmp = (0.3f + 0.7f * excite) * std::pow (0.0003f / exLen, 0.35f);
    float ex = (t < 1.5f / sr ? 1.f : 0.f) + noise.next (noiseTypeCache (pr), hard) * std::exp (-t / exLen) * exAmp;
    float exC = 1.f - std::exp (-juce::MathConstants<float>::twoPi * juce::jmin (300.f * std::pow (2.f, hard * 6.f), sr * 0.45f) / sr);
    mtExLp += exC * (ex - mtExLp);

    float out = 0.f, ampSum = 0.f;
    for (int i = 0; i < 8; ++i) {
        float ratio = sets[si][i] + (sets[juce::jmin (4, si + 1)][i] - sets[si][i]) * sf;
        float a = amps[si][i] + (amps[juce::jmin (4, si + 1)][i] - amps[si][i]) * sf;
        ampSum += a;
        float f = f0 * std::pow (ratio, stretch);
        if (f > sr * 0.45f) continue;
        float T = juce::jmax (0.01f, T0 / (1.f + dampAmt * 2.5f * (ratio - 0.5f)));
        float w = juce::MathConstants<float>::twoPi * f / sr;
        float R = std::exp (-6.91f / (T * sr));
        float y = std::sin (w) * mtExLp - (-2.f * R * std::cos (w)) * mtY[i][0] - R * R * mtY[i][1];
        mtY[i][1] = mtY[i][0]; mtY[i][0] = y;
        out += y * a;
    }
    out *= 0.35f * 3.f / juce::jmax (1.f, ampSum); // equal loudness across materials (gong modes are many + even)

    // OSC LAYER: two oscillators (WAVE) on the two lowest modes
    if (layer > 0.001f) {
        float lay = 0.f;
        for (int k = 0; k < 2; ++k) {
            float ratio = sets[si][k] + (sets[juce::jmin (4, si + 1)][k] - sets[si][k]) * sf;
            mtOscPh[k] += f0 * std::pow (ratio, stretch) / sr;
            if (mtOscPh[k] >= 1.f) mtOscPh[k] -= 1.f;
            float ph = mtOscPh[k], o;
            switch (pr.wave) {
                case 1:  o = 1.f - 4.f * std::abs (ph - 0.5f); break;
                case 2:  o = 2.f * ph - 1.f; break;
                case 3:  o = ph < 0.5f ? 1.f : -1.f; break;
                case 4:  o = std::sin (juce::MathConstants<float>::twoPi * ph + 2.f * std::sin (juce::MathConstants<float>::twoPi * ph * 2.f)); break;
                default: o = std::sin (juce::MathConstants<float>::twoPi * ph); break;
            }
            lay += o * (k == 0 ? 0.6f : 0.45f);
        }
        out += lay * layer * 0.5f * std::exp (-t / (T0 * 0.35f));
    }

    if (drive > 0.0005f) {
        float g = 1.f + drive * 9.f;
        out += juce::jmin (1.f, drive * 4.f) * (std::tanh (out * g) / std::tanh (g) - out);
    }
    // end the voice once the modes have rung out (the amp env only gates)
    mtFollow = juce::jmax (std::abs (out), mtFollow * 0.9995f);
    if (t > 0.05f && mtFollow < 1.0e-4f) { active = false; return 0.f; }
    float velAmp = pr.p[15] / 100.f;
    return out * env * ((1.f - velAmp) + velAmp * velocity) * 1.2f;
}

// ---------------- SFX: sweep synth ----------------
// Osc A (WAVE) phase-modulated by osc B (FM RATIO, FM depth), + NOISE,
// through a resonant low-pass (CUTOFF/RESO) with its own envelope (F.ENV,
// bipolar). SWEEP is bipolar: + starts above and falls (zaps, drops),
// - starts below and rises (risers). SWEEP TIME/CURVE shape both sweeps.
// LFO (RATE, >PITCH, >FILT) for wobbles/trills; DRIVE; CRUSH.
float DrumVoice::renderSfx (const PartParams& pr) {
    postFx = 0.f;
    if (choke > 0.f) { choke -= 1.f; if (choke <= 0.f) amp.noteOff(); }
    float env = amp.next();
    if (! amp.isActive()) { active = false; return 0.f; }
    const float sr = sampleRate, t = hitT;
    hitT += 1.f / sr;
    const float twoPi = juce::MathConstants<float>::twoPi;

    // sweep runs the whole SWEEP TIME. CURVE: 50 linear, < 50 fast start
    // (zaps/drops), > 50 slow start that accelerates (risers/builds)
    float shape = sweepRemaining (t / juce::jmax (0.001f, pr.p[2] / 1000.f), pr.p[3] / 100.f);

    sfxLfoPh += twoPi * juce::jlimit (0.1f, 30.f, pr.p[16]) / sr;
    if (sfxLfoPh > twoPi) sfxLfoPh -= twoPi;
    float lfo = std::sin (sfxLfoPh);

    float semis = pr.p[1] * shape + lfo * pr.p[17];
    float f = juce::jlimit (5.f, sr * 0.45f, midiToFreq (baseMidiF + pr.p[0] + tuneOff) * std::pow (2.f, semis / 12.f));

    // osc B (modulator) -> osc A phase
    float ratio = juce::jlimit (0.25f, 8.f, pr.p[19]);
    sfxB += f * ratio / sr; if (sfxB >= 1.f) sfxB -= 1.f;
    float mod = std::sin (twoPi * sfxB) * (pr.p[8] / 100.f) * 6.f;
    osc.wave = pr.wave == 4 ? 0 : pr.wave; osc.fmAmt = 0.f; osc.driftAmt = 0.f;
    osc.freq = f;
    float a = osc.next();
    if (mod != 0.f) a = std::sin (twoPi * osc.phase + mod) * (pr.wave == 0 || pr.wave == 4 ? 1.f : 0.5f) + (pr.wave == 0 || pr.wave == 4 ? 0.f : a * 0.5f);
    float sig = a * 0.7f + noise.next (noiseTypeCache (pr), 0.7f) * (pr.p[9] / 100.f) * 0.8f;

    // resonant LP: CUTOFF + F.ENV (+-6 oct, follows the sweep) + LFO
    float cutOct = (pr.p[10] / 100.f) * 9.f + (pr.p[11] / 100.f) * 6.f * shape + lfo * (pr.p[18] / 100.f) * 3.f;
    float cut = 35.f * std::pow (2.f, cutOct);
    sig = bodyFilt.process (sig, cut, 0.6f + (pr.p[14] / 100.f) * 11.f, 0);

    float drive = juce::jlimit (0.f, 1.f, pr.p[12] / 100.f);
    if (drive > 0.0005f) {
        float g = 1.f + drive * 9.f;
        sig += juce::jmin (1.f, drive * 4.f) * (std::tanh (sig * g) / std::tanh (g) - sig);
    }
    if (pr.p[13] > 0.5f) { // CRUSH: sample-rate + bit reduction
        float c = pr.p[13] / 100.f;
        if (--crushCnt <= 0) { crushCnt = 1 + (int) (c * 15.f); float steps = std::pow (2.f, 12.f - 9.f * c); crushHold = std::round (sig * steps) / steps; }
        sig = crushHold;
    }
    float velAmp = pr.p[15] / 100.f;
    return sig * env * ((1.f - velAmp) + velAmp * velocity) * 0.9f;
}

} // namespace mds
