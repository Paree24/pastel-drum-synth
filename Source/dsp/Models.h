#pragma once
#include <JuceHeader.h>
#include <vector>

// ============================================================
// Engine MODELS: real-time ports of the Pastel kit generator's
// recipes (tools/kitgen in the drum sampler). Model 0 of every
// part is CLASSIC (the original voice, untouched); models 1..N
// pick one of these engines with their own 0..1 parameters.
// Everything is allocated in prepare(); trigger()/render() never
// allocate.
// ============================================================

namespace mds {

constexpr int NumModelParams = 16;

enum ModelEngine {
    ME_None = 0,
    ME_Kick, ME_Rumble, ME_Boom,
    ME_Snare, ME_Snare909, ME_Clap,
    ME_Hat, ME_Shaker, ME_Tambourine, ME_Ride, ME_Crash,
    ME_Tom, ME_Conga, ME_Rim, ME_Wood, ME_Glass, ME_Snap, ME_Clang, ME_Bell,
    ME_Riser, ME_Impact, ME_Laser, ME_Siren, ME_SubDrop, ME_Vox, ME_Zap, ME_NoiseHit, ME_Horn,
    ME_Count
};

struct ModelParam {
    const char* label;
    float min, max, def;
    const char* fmt;    // "Hz" "ms" "s" "%" "dB" "x" "st" "drv" "word" "shape" "int"
    bool log = false;   // exponential mapping (frequencies, times)
};

struct ModelDef {
    const char* name;
    int engine;
    int nparams;
    ModelParam p[NumModelParams];
    float fixed[8] = {};   // engine-specific constants that are not knobs
    float gain = 1.f;      // level calibration (default settings peak ~0.8, like CLASSIC parts)
};

// registry (part type -> models; index 0 = CLASSIC = nullptr)
int numModels (int partType);
const ModelDef* modelDef (int partType, int model);
juce::StringArray modelNames (int partType);
float modelCook (const ModelParam&, float v01);
float modelUncook (const ModelParam&, float cooked);
juce::String modelFormat (const ModelParam&, float cooked);
void modelDefaults01 (int partType, int model, float* out01);
// envelope-like knobs (DECAY, LENGTH, TAIL, ATTACK, HOLD, GATE) and VEL>AMP shape how a hit
// plays, not its timbre: RANDOMIZE / MUTATE leave them alone (like the CLASSIC envelope slots)
bool modelParamIsEnvelope (const ModelParam&);

// ---- small DSP pieces (public so tests/tools can use them) ----
struct MRng {
    juce::uint32 s = 0x9e3779b9u;
    void seed (juce::uint32 v) { s = v ? v : 0x9e3779b9u; }
    juce::uint32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (2.f / 16777216.f) - 1.f; }   // -1..1
    float u01() { return (float) (next() >> 8) * (1.f / 16777216.f); }
};

struct MBq {   // RBJ biquad, transposed direct form II
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    enum Type { LP, HP, BP, Peak, LowShelf, HighShelf };
    void set (Type t, float f, float q, float gainDb, float sr);
    void reset() { z1 = z2 = 0.f; }
    float tick (float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

struct MEnv {   // linear attack, hold, exponential decay to -60 dB at 'decay' seconds
    float v = 0.f, inc = 1.f, coef = 0.f; int hold = 0; int stage = 3;
    void start (float attack, float holdS, float decay, float sr);
    float next();
};

struct MPhasor {   // decaying sinusoid by complex rotation (modal partial)
    float re = 0, im = 0, cr = 1, ci = 0;
    void start (float freq, float decay, float amp, float phase, float sr);
    float tick() { float r2 = re * cr - im * ci; im = re * ci + im * cr; re = r2; return im; }
};

struct MRoom {   // small Schroeder room: 4 damped combs + 2 allpasses (mono)
    std::vector<float> c[4], a[2];
    int cl[4] = {}, al[2] = {}, cp[4] = {}, ap[2] = {};
    float fb = 0.7f, damp = 0.3f, lpS[4] = {};
    void prepare (float sr);
    void set (float rt60, float dampAmt, float sr);
    void clear();
    float tick (float x);
};

struct ModelVoice {
    bool active = false;
    void prepare (float sr);
    // c = cooked params; tuneSt = part TUNE offset (semitones); seed = per-hit randomness
    void trigger (const ModelDef& def, const float* cooked, float velocity, float tuneSt, juce::uint32 seed);
    float render();
    void choke (float ms);

private:
    float sr = 44100.f;
    int engine = 0; const ModelDef* d = nullptr;
    float c[NumModelParams] = {};
    float vel = 1.f, ps = 1.f, gain = 1.f;
    double t = 0.0; int n = 0, len = 0;
    MRng rng;
    // oscillator phases
    double ph[8] = {};
    float fEnd = 0, fStart = 0, tauK = 0, fCur = 0;
    MEnv e[8];
    MBq f[10];
    float pink[7] = {}, brown = 0.f, lpS[6] = {};
    float noiseScale[4] = { 1, 1, 1, 1 };
    static constexpr int MaxPartials = 72;
    MPhasor part[MaxPartials]; int nPart = 0;
    MPhasor part2[8]; int nPart2 = 0;
    MRoom room; float roomWet = 0.f; MBq roomHp, roomLp;
    // dynamics
    float compEnv = 0, trF = 0, trS = 0, chokeGain = 1.f, chokeStep = 0.f, dcX = 0, dcY = 0;
    // clap / tambourine / shaker schedules
    int burstAt[8] = {}; float burstDec[8] = {}, burstAmp[8] = {}; int nBurst = 0; float burstEnv[8] = {};
    struct Grain { int pos = 0, len = 0; } grains[24]; int nextGrain = 0; float grainRate = 0.f;
    // crush
    float crushHold = 0.f; int crushCnt = 0, crushStep = 1; float crushQ = 0.f;
    // vox
    int word = 0; float fmt[4][4] = {}; int fmtPts = 0; float fmtT[4] = {}; int fmtBlock = 0; float h_len = 0.f;
    float drv = 0, drvKind = 0;

    float white() { return rng.uni(); }
    float pinkNoise();
    float colored (float slope);
    float coloredChainRms (float slope, const MBq* fs, int nf);   // level of a noise chain, measured with ITS noise colour
    float shape (float x, float db, int kind) const;
    float renderEngine();
};

} // namespace mds
