#include "Models.h"
#include <cmath>

namespace mds {

static constexpr float kTwoPi = 6.283185307179586f;
static constexpr float kRumbleGain = 1.2f;   // tuned against the generator (feature error 1.1 dB)
static inline float dbg (float d) { return std::pow (10.f, d / 20.f); }
static inline float clampf (float v, float a, float b) { return v < a ? a : (v > b ? b : v); }

// ============================================================ parameter schemas (one per engine)
// knob order is the engine's contract: the render code reads c[index]
using MP = ModelParam;
static const MP S_KICK[] = {
    { "TUNE", 25, 120, 55, "Hz", true }, { "SWEEP", 1, 12, 6, "x" }, { "SWEEP TIME", 2, 80, 15, "ms", true },
    { "DECAY", 40, 2000, 250, "ms", true }, { "CLICK", 0, 150, 100, "%" }, { "CLICK TONE", 1000, 9000, 5500, "Hz", true },
    { "NOISE", 0, 60, 0, "%" }, { "DRIVE", 0, 24, 12, "dB" }, { "DRIVE TYPE", 0, 3, 0, "drv" }, { "PUNCH", 0, 12, 8, "dB" },
    { "LOW", -6, 6, 0, "dB" }, { "TOP", -6, 6, 0, "dB" }, { "HOLD", 0, 300, 0, "ms" }, { "TAPE", 0, 12, 0, "dB" },
    { "CRUSH", 0, 100, 0, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_RUMBLE[] = {
    { "TUNE", 25, 120, 46, "Hz", true }, { "SWEEP", 1, 12, 6, "x" }, { "SWEEP TIME", 2, 80, 22, "ms", true },
    { "DECAY", 40, 2000, 350, "ms", true }, { "CLICK", 0, 150, 50, "%" }, { "DRIVE", 0, 24, 9, "dB" }, { "PUNCH", 0, 12, 3, "dB" },
    { "RUMBLE", 0, 150, 90, "%" }, { "RUMBLE DECAY", 0.4f, 4, 1.6f, "s", true }, { "RUMBLE TONE", 60, 500, 180, "Hz", true },
    { "RUMBLE DRIVE", 0, 24, 10, "dB" }, { "DUCK", 50, 600, 220, "ms", true }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_BOOM[] = {
    { "TUNE", 25, 90, 43.65f, "Hz", true }, { "DECAY", 0.2f, 4, 1.6f, "s", true }, { "GLIDE", 1, 4, 1.5f, "x" },
    { "GLIDE TIME", 5, 300, 60, "ms", true }, { "DRIVE", 0, 18, 8, "dB" }, { "CLICK", 0, 100, 25, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_SNARE[] = {
    { "TUNE", 100, 400, 165, "Hz", true }, { "SWEEP FROM", 150, 1600, 776, "Hz", true }, { "SWEEP TIME", 1, 20, 5.7f, "ms", true },
    { "BODY DECAY", 30, 500, 176, "ms", true }, { "BODY", 0, 150, 100, "%" }, { "NOISE", 0, 300, 104, "%" },
    { "NOISE HP", 100, 3000, 824, "Hz", true }, { "NOISE SWELL", 0.5f, 80, 70, "ms", true }, { "TAIL", -40, 0, -4, "dB" },
    { "TAIL DECAY", 50, 1200, 800, "ms", true }, { "CLICK", 0, 150, 125, "%" }, { "SNAP", 0, 200, 108, "%" },
    { "CLIP", 0, 12, 5.3f, "dB" }, { "ROOM", 0, 100, 25, "%" }, { "CLAP", 0, 100, 50, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_909[] = {
    { "TUNE", 150, 400, 236, "Hz", true }, { "SPREAD", 1.1f, 2, 1.33f, "x" }, { "BEND", 0, 0.6f, 0.15f, "x" },
    { "TONE DECAY", 30, 600, 332, "ms", true }, { "TONE", 0, 200, 56, "%" }, { "TONE TAIL", -60, -10, -33.7f, "dB" },
    { "SNAPPY", 0, 200, 61, "%" }, { "SNAP HOLD", 0, 150, 67, "ms" }, { "SNAP DECAY", 40, 800, 320, "ms", true },
    { "SNAP HP", 300, 5000, 3197, "Hz", true }, { "NOISE LP", 3000, 19000, 10221, "Hz", true }, { "GRIT", 0, 200, 150, "%" },
    { "GRIT TONE", 400, 5000, 2413, "Hz", true }, { "CLICK", 0, 100, 20, "%" }, { "DRIVE", 0, 12, 0.8f, "dB" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_CLAP[] = {
    { "BURSTS", 2, 6, 4, "int" }, { "SPREAD", 4, 20, 10, "ms" }, { "TONE", 600, 3000, 1300, "Hz", true }, { "Q", 0.5f, 3, 1.1f, "x" },
    { "TAIL", 60, 600, 220, "ms", true }, { "BRIGHT", -6, 9, 2, "dB" }, { "DRIVE", 0, 18, 4, "dB" }, { "ROOM", 0, 100, 25, "%" },
    { "ROOM SIZE", 0.2f, 2, 0.7f, "s", true }, { "CRUSH", 0, 100, 0, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_HAT[] = {
    { "TUNE", 1500, 6000, 3100, "Hz", true }, { "DECAY", 15, 1500, 45, "ms", true }, { "FM", 0, 10, 3.5f, "x" },
    { "METAL", 0, 150, 60, "%" }, { "NOISE", 0, 150, 70, "%" }, { "HP", 1500, 9000, 4300, "Hz", true },
    { "TONE", 3000, 14000, 8200, "Hz", true }, { "TONE GAIN", -6, 9, 3, "dB" }, { "ATTACK", 0.1f, 30, 0.4f, "ms", true },
    { "DRIVE", 0, 18, 2, "dB" }, { "CRUSH", 0, 100, 0, "%" }, { "VEL>AMP", 0, 100, 55, "%" } };
static const MP S_SHAKER[] = {
    { "LENGTH", 60, 500, 180, "ms", true }, { "TONE", 3000, 12000, 7500, "Hz", true }, { "DENSITY", 200, 3000, 900, "x", true },
    { "SWELL", 5, 80, 20, "%" }, { "VEL>AMP", 0, 100, 55, "%" } };
static const MP S_TAMB[] = {
    { "LENGTH", 80, 600, 220, "ms", true }, { "HITS", 1, 6, 3, "int" }, { "SPREAD", 4, 40, 12, "ms" },
    { "BRIGHT", 3000, 9000, 5500, "Hz", true }, { "SKIN", 0, 100, 15, "%" }, { "VEL>AMP", 0, 100, 55, "%" } };
static const MP S_RIDE[] = {
    { "TUNE", 200, 900, 420, "Hz", true }, { "DECAY", 0.5f, 6, 3, "s", true }, { "BELL", 0, 150, 40, "%" },
    { "SHIMMER", 0, 150, 35, "%" }, { "STICK", 0, 150, 50, "%" }, { "VEL>AMP", 0, 100, 55, "%" } };
static const MP S_CRASH[] = {
    { "TUNE", 800, 4000, 1900, "Hz", true }, { "DECAY", 0.4f, 6, 2.4f, "s", true }, { "BRIGHT", -6, 9, 0, "dB" },
    { "ATTACK", 0.5f, 60, 2, "ms", true }, { "DARK", 3000, 16000, 12500, "Hz", true }, { "METAL", 0, 150, 50, "%" },
    { "VEL>AMP", 0, 100, 55, "%" } };
static const MP S_TOM[] = {
    { "TUNE", 50, 400, 140, "Hz", true }, { "DECAY", 80, 1500, 450, "ms", true }, { "DROP", 1, 3, 1.5f, "x" },
    { "STICK", 0, 150, 40, "%" }, { "OVERTONES", 0, 150, 35, "%" }, { "DRIVE", 0, 18, 4, "dB" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_CONGA[] = {
    { "TUNE", 120, 700, 330, "Hz", true }, { "DECAY", 60, 800, 250, "ms", true }, { "SLAP", 0, 150, 30, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_RIM[] = {
    { "TONE", 800, 4000, 1700, "Hz", true }, { "BODY", 150, 900, 420, "Hz", true }, { "DECAY", 15, 200, 45, "ms", true },
    { "CRUSH", 0, 100, 0, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_WOOD[] = {
    { "TUNE", 400, 4000, 2200, "Hz", true }, { "DECAY", 20, 300, 70, "ms", true }, { "CLICK", 0, 150, 40, "%" },
    { "DRIVE", 0, 12, 2, "dB" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_GLASS[] = {
    { "TUNE", 400, 4000, 1250, "Hz", true }, { "DECAY", 100, 2000, 600, "ms", true }, { "STRIKE", 0, 100, 20, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_SNAP[] = {
    { "TONE", 1000, 5000, 2300, "Hz", true }, { "DECAY", 10, 120, 30, "ms", true }, { "TAIL", 0, 100, 70, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_CLANG[] = {
    { "TUNE", 60, 1200, 210, "Hz", true }, { "DECAY", 0.1f, 3, 1.4f, "s", true }, { "DRIVE", 0, 24, 10, "dB" },
    { "STRIKE", 0, 150, 80, "%" }, { "CRUSH", 0, 100, 0, "%" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_BELL[] = {
    { "TUNE", 200, 1500, 560, "Hz", true }, { "DECAY", 50, 1500, 280, "ms", true }, { "SHAPE", 0, 2, 0, "shape" },
    { "TONE", 500, 4000, 1400, "Hz", true }, { "DRIVE", 0, 18, 3, "dB" }, { "VEL>AMP", 0, 100, 70, "%" } };
static const MP S_RISER[] = {
    { "LENGTH", 0.5f, 6, 2.5f, "s", true }, { "START", 100, 2000, 300, "Hz", true }, { "END", 2000, 16000, 9000, "Hz", true },
    { "TONE", 0, 150, 100, "%" }, { "RESO", 0.7f, 8, 2.5f, "x" }, { "VEL>AMP", 0, 100, 40, "%" } };
static const MP S_IMPACT[] = {
    { "BOOM", 25, 120, 55, "Hz", true }, { "DECAY", 0.4f, 5, 2.5f, "s", true }, { "CRACK", 0, 150, 60, "%" },
    { "DRIVE", 0, 24, 10, "dB" }, { "ROOM", 0, 100, 60, "%" }, { "VEL>AMP", 0, 100, 40, "%" } };
static const MP S_LASER[] = {
    { "START", 500, 12000, 5000, "Hz", true }, { "END", 40, 2000, 120, "Hz", true }, { "LENGTH", 40, 1500, 250, "ms", true },
    { "FM", 0, 10, 3, "x" }, { "DRIVE", 0, 18, 6, "dB" }, { "VEL>AMP", 0, 100, 40, "%" } };
static const MP S_SIREN[] = {
    { "LOW", 100, 2000, 650, "Hz", true }, { "HIGH", 200, 4000, 1150, "Hz", true }, { "RATE", 0.2f, 16, 4, "Hz", true },
    { "LENGTH", 0.3f, 4, 1.6f, "s", true }, { "VEL>AMP", 0, 100, 40, "%" } };
static const MP S_SUBDROP[] = {
    { "START", 40, 200, 90, "Hz", true }, { "END", 20, 80, 28, "Hz", true }, { "LENGTH", 0.3f, 4, 1.8f, "s", true },
    { "DRIVE", 0, 18, 6, "dB" }, { "VEL>AMP", 0, 100, 40, "%" } };
static const MP S_VOX[] = {
    { "WORD", 0, 6, 0, "word" }, { "PITCH", 70, 500, 210, "Hz", true }, { "LENGTH", 100, 1500, 400, "ms", true },
    { "GENDER", 0.7f, 1.4f, 1, "x" }, { "DRIVE", 0, 24, 4, "dB" }, { "BREATH", 0, 50, 8, "%" }, { "VEL>AMP", 0, 100, 40, "%" } };
static const MP S_ZAP[] = {
    { "START", 200, 12000, 1800, "Hz", true }, { "END", 30, 3000, 300, "Hz", true }, { "SWEEP", 1, 100, 20, "ms", true },
    { "DECAY", 10, 1000, 80, "ms", true }, { "FM", 0, 8, 0, "x" }, { "DRIVE", 0, 18, 4, "dB" }, { "VEL>AMP", 0, 100, 60, "%" } };
static const MP S_NOISEHIT[] = {
    { "DECAY", 20, 1000, 150, "ms", true }, { "HP", 100, 8000, 800, "Hz", true }, { "GATE", 0, 80, 0, "Hz" },
    { "DRIVE", 0, 24, 14, "dB" }, { "CRUSH", 0, 100, 70, "%" }, { "VEL>AMP", 0, 100, 60, "%" } };
static const MP S_HORN[] = {
    { "PITCH", 150, 1200, 466, "Hz", true }, { "LENGTH", 0.2f, 3, 0.9f, "s", true }, { "STABS", 1, 6, 3, "int" },
    { "DRIVE", 0, 18, 10, "dB" }, { "VEL>AMP", 0, 100, 40, "%" } };

template <size_t N>
static ModelDef make (const char* name, int engine, const MP (&schema)[N], std::initializer_list<std::pair<int, float>> defs = {},
                      std::initializer_list<float> fixed = {}, float gain = 1.f) {
    ModelDef d {};
    d.name = name; d.engine = engine; d.nparams = (int) N; d.gain = gain;
    for (size_t i = 0; i < N; ++i) d.p[i] = schema[i];
    for (auto& kv : defs) d.p[kv.first].def = kv.second;
    int k = 0;
    for (float v : fixed) if (k < 8) d.fixed[k++] = v;
    return d;
}

// snare fixed: body_attack, body_drv, noise_slope, burst_decay, snap_hz, sat_db, room_decay, clap tone
static const std::initializer_list<float> FX_LAYERED = { 0.0003f, 3.3f, -1.42f, 0.124f, 1492.f, 0.f, 1.0f, 1300.f };
static const std::initializer_list<float> FX_SWEPT   = { 0.025f, 0.f, -2.04f, 0.03f, 3462.f, 6.f, 0.6f, 1300.f };
static const std::initializer_list<float> FX_909     = { 0.0182f, 0.768f, 0.05f, 3.f, 0.15f };

struct Registry {
    std::vector<ModelDef> models[12];
    Registry() {
        // KICK
        models[0] = { make ("PUNCH", ME_Kick, S_KICK),
                      make ("RUMBLE", ME_Rumble, S_RUMBLE),
                      make ("BOOM", ME_Boom, S_BOOM) };
        // SNARE
        models[1] = { make ("LAYERED", ME_Snare, S_SNARE, {}, FX_LAYERED),
                      make ("SWEPT", ME_Snare, S_SNARE, { { 0, 230 }, { 1, 900 }, { 2, 11.2f }, { 3, 285 }, { 5, 191 }, { 6, 1172 },
                                                          { 7, 24.5f }, { 8, -4.75f }, { 9, 484 }, { 10, 15 }, { 11, 0 }, { 12, 0 },
                                                          { 13, 50 }, { 14, 0 } }, FX_SWEPT),
                      make ("DRUM MACHINE", ME_Snare909, S_909, {}, FX_909),
                      make ("CLAP", ME_Clap, S_CLAP) };
        // CHH / OHH
        models[2] = { make ("FM METAL", ME_Hat, S_HAT),
                      make ("SHAKER", ME_Shaker, S_SHAKER),
                      make ("TAMBOURINE", ME_Tambourine, S_TAMB) };
        models[3] = { make ("FM METAL", ME_Hat, S_HAT, { { 1, 400 }, { 2, 4 } }),
                      make ("SHAKER", ME_Shaker, S_SHAKER, { { 0, 260 }, { 3, 25 } }),
                      make ("TAMBOURINE", ME_Tambourine, S_TAMB, { { 0, 320 } }) };
        // RIDE / CRASH
        models[4] = { make ("CLOUD", ME_Ride, S_RIDE),
                      make ("BELL", ME_Bell, S_BELL, { { 0, 700 }, { 1, 900 }, { 2, 1 }, { 3, 2000 } }) };
        models[5] = { make ("CLOUD", ME_Crash, S_CRASH),
                      make ("CLOUD DARK", ME_Crash, S_CRASH, { { 4, 6500 }, { 1, 3 } }) };
        // TOMS
        models[6] = { make ("MEMBRANE", ME_Tom, S_TOM, { { 0, 170 }, { 1, 400 } }), make ("CONGA", ME_Conga, S_CONGA, { { 0, 340 } }) };
        models[7] = { make ("MEMBRANE", ME_Tom, S_TOM, { { 0, 130 }, { 1, 450 } }), make ("CONGA", ME_Conga, S_CONGA, { { 0, 260 } }) };
        models[8] = { make ("MEMBRANE", ME_Tom, S_TOM, { { 0, 95 }, { 1, 600 }, { 2, 1.8f } }), make ("CONGA", ME_Conga, S_CONGA, { { 0, 200 }, { 1, 320 } }) };
        // MEM PERC
        models[9] = { make ("CONGA", ME_Conga, S_CONGA), make ("BONGO", ME_Conga, S_CONGA, { { 0, 450 }, { 1, 140 }, { 2, 50 } }),
                      make ("RIM", ME_Rim, S_RIM), make ("WOOD", ME_Wood, S_WOOD), make ("GLASS", ME_Glass, S_GLASS),
                      make ("SNAP", ME_Snap, S_SNAP), make ("CLAP", ME_Clap, S_CLAP) };
        // MET PERC
        models[10] = { make ("CLANG", ME_Clang, S_CLANG), make ("COWBELL", ME_Bell, S_BELL),
                       make ("AGOGO", ME_Bell, S_BELL, { { 0, 700 }, { 1, 300 }, { 2, 1 }, { 3, 1600 } }),
                       make ("TAMBOURINE", ME_Tambourine, S_TAMB), make ("GLASS", ME_Glass, S_GLASS, { { 0, 1600 } }) };
        // SFX
        models[11] = { make ("RISER", ME_Riser, S_RISER), make ("IMPACT", ME_Impact, S_IMPACT), make ("LASER", ME_Laser, S_LASER),
                       make ("SIREN", ME_Siren, S_SIREN), make ("SUB DROP", ME_SubDrop, S_SUBDROP), make ("VOX", ME_Vox, S_VOX),
                       make ("ZAP", ME_Zap, S_ZAP), make ("NOISE HIT", ME_NoiseHit, S_NOISEHIT), make ("AIR HORN", ME_Horn, S_HORN),
                       make ("BOOM", ME_Boom, S_BOOM) };
        // level calibration: default settings peak like the part's CLASSIC voice (measured with ModelProbe)
        static const std::vector<float> cal[12] = {
        { 0.54f, 0.867f, 0.707f },   // KICK
        { 1.942f, 2.608f, 3.152f, 2.552f },   // SNARE
        { 1.706f, 1.685f, 0.632f },   // CHH
        { 1.84f, 2.099f, 0.668f },   // OHH
        { 0.565f, 1.227f },   // RIDE
        { 0.746f, 0.82f },   // CRASH
        { 1.119f, 1.113f },   // TOM 1
        { 1.16f, 1.149f },   // TOM 2
        { 1.164f, 1.156f },   // LOW TOM
        { 0.838f, 0.853f, 0.852f, 0.786f, 0.558f, 1.323f, 0.858f },   // MEM PERC
        { 1.783f, 1.611f, 1.606f, 0.767f, 1.121f },   // MET PERC
        { 0.854f, 0.41f, 1.016f, 1.367f, 0.656f, 1.449f, 1.017f, 0.814f, 0.832f, 0.606f },   // SFX
        };
        for (int pt = 0; pt < 12; ++pt)
            for (size_t i = 0; i < models[pt].size() && i < cal[pt].size(); ++i) models[pt][i].gain = cal[pt][i];
    }
};
static Registry& reg() { static Registry r; return r; }

int numModels (int partType) {
    if (partType < 0 || partType >= 12) return 1;
    return 1 + (int) reg().models[partType].size();
}
const ModelDef* modelDef (int partType, int model) {
    if (partType < 0 || partType >= 12 || model <= 0) return nullptr;
    auto& v = reg().models[partType];
    return model <= (int) v.size() ? &v[(size_t) model - 1] : nullptr;
}
juce::StringArray modelNames (int partType) {
    juce::StringArray s { "CLASSIC" };
    if (partType >= 0 && partType < 12) for (auto& m : reg().models[partType]) s.add (m.name);
    return s;
}
float modelCook (const ModelParam& p, float v) {
    v = clampf (v, 0.f, 1.f);
    float c = p.log && p.min > 0.f ? p.min * std::pow (p.max / p.min, v) : p.min + (p.max - p.min) * v;
    juce::String f (p.fmt);
    if (f == "int" || f == "drv" || f == "word" || f == "shape") c = std::round (c);
    return c;
}
float modelUncook (const ModelParam& p, float c) {
    c = clampf (c, p.min, p.max);
    if (p.log && p.min > 0.f) return std::log (c / p.min) / std::log (p.max / p.min);
    return (c - p.min) / (p.max - p.min);
}
bool modelParamIsEnvelope (const ModelParam& p) {
    juce::String l (p.label);
    for (auto* w : { "DECAY", "LENGTH", "TAIL", "ATTACK", "HOLD", "GATE", "VEL>" })
        if (l.contains (w)) return true;
    return false;
}
juce::String modelFormat (const ModelParam& p, float v) {
    juce::String f (p.fmt);
    if (f == "Hz") return v < 1000.f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.f, 2) + " kHz";
    if (f == "ms") return v < 10.f ? juce::String (v, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
    if (f == "s") return juce::String (v, 2) + " s";
    if (f == "%") return juce::String (juce::roundToInt (v)) + "%";
    if (f == "dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (f == "x") return juce::String (v, 2) + "x";
    if (f == "int") return juce::String (juce::roundToInt (v));
    if (f == "drv") { static const char* k[4] = { "SOFT", "CUBIC", "HARD", "FOLD" }; return k[juce::jlimit (0, 3, juce::roundToInt (v))]; }
    if (f == "shape") { static const char* k[3] = { "COWBELL", "AGOGO", "ANVIL" }; return k[juce::jlimit (0, 2, juce::roundToInt (v))]; }
    if (f == "word") { static const char* k[7] = { "HEY", "YEAH", "AH", "OH", "OI", "GO", "UH" }; return k[juce::jlimit (0, 6, juce::roundToInt (v))]; }
    return juce::String (v, 2);
}
void modelDefaults01 (int partType, int model, float* out) {
    for (int i = 0; i < NumModelParams; ++i) out[i] = 0.5f;
    if (auto* d = modelDef (partType, model))
        for (int i = 0; i < d->nparams; ++i) out[i] = modelUncook (d->p[i], d->p[i].def);
}

// ============================================================ DSP pieces
void MBq::set (Type t, float fc, float q, float gainDb, float sr) {
    fc = clampf (fc, 10.f, sr * 0.49f);
    float w = kTwoPi * fc / sr, cw = std::cos (w), sw = std::sin (w), al = sw / (2.f * q), A = std::pow (10.f, gainDb / 40.f);
    float B0, B1, B2, A0, A1, A2;
    switch (t) {
        case LP: B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
        case HP: B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
        case BP: B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break;
        case Peak: B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break;
        case LowShelf: { float s = 2 * std::sqrt (A) * al;
            B0 = A * ((A + 1) - (A - 1) * cw + s); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - s);
            A0 = (A + 1) + (A - 1) * cw + s; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - s; break; }
        default: { float s = 2 * std::sqrt (A) * al;
            B0 = A * ((A + 1) + (A - 1) * cw + s); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - s);
            A0 = (A + 1) - (A - 1) * cw + s; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - s; break; }
    }
    b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
}

void MEnv::start (float attack, float holdS, float decay, float sr) {
    hold = (int) (holdS * sr);
    coef = std::exp (std::log (0.001f) / std::max (1.f, decay * sr));
    if (attack * sr < 1.f) { v = 1.f; stage = 1; }
    else { v = 0.f; inc = 1.f / (attack * sr); stage = 0; }
}
float MEnv::next() {
    float out = v;
    switch (stage) {
        case 0: v += inc; if (v >= 1.f) { v = 1.f; stage = 1; } break;
        case 1: if (--hold <= 0) stage = 2; break;
        case 2: v *= coef; break;
        default: break;
    }
    return out;
}

void MPhasor::start (float freq, float decay, float amp, float phase, float sr) {
    if (freq >= sr * 0.45f || freq <= 0.f) { re = im = 0.f; cr = 1.f; ci = 0.f; return; }
    float w = kTwoPi * freq / sr, r = std::exp (std::log (0.001f) / std::max (1.f, decay * sr));
    cr = r * std::cos (w); ci = r * std::sin (w);
    re = amp * std::cos (phase); im = amp * std::sin (phase);
}

void MRoom::prepare (float sr) {
    static const float cms[4] = { 25.31f, 26.94f, 28.96f, 30.75f }, ams[2] = { 5.0f, 1.7f };
    for (int i = 0; i < 4; ++i) { cl[i] = std::max (8, (int) (cms[i] * 0.001f * sr)); c[i].assign ((size_t) cl[i], 0.f); }
    for (int i = 0; i < 2; ++i) { al[i] = std::max (4, (int) (ams[i] * 0.001f * sr)); a[i].assign ((size_t) al[i], 0.f); }
    clear();
}
void MRoom::set (float rt60, float dampAmt, float sr) {
    float avg = (cl[0] + cl[1] + cl[2] + cl[3]) / 4.f;
    fb = std::pow (10.f, -3.f * avg / std::max (1.f, rt60 * sr));
    damp = clampf (dampAmt, 0.f, 0.95f);
}
void MRoom::clear() {
    for (auto& v : c) std::fill (v.begin(), v.end(), 0.f);
    for (auto& v : a) std::fill (v.begin(), v.end(), 0.f);
    for (int i = 0; i < 4; ++i) { cp[i] = 0; lpS[i] = 0.f; }
    ap[0] = ap[1] = 0;
}
float MRoom::tick (float x) {
    if (c[0].empty()) return 0.f;
    float out = 0.f;
    for (int i = 0; i < 4; ++i) {
        float y = c[(size_t) i][(size_t) cp[i]];
        lpS[i] = y * (1.f - damp) + lpS[i] * damp;
        c[(size_t) i][(size_t) cp[i]] = x + lpS[i] * fb;
        cp[i] = (cp[i] + 1) % cl[i];
        out += y;
    }
    // energy of 4 combs ~ 4/(1-fb^2): scale back to ~unit energy (like a normalised convolution IR)
    out *= std::sqrt ((1.f - fb * fb) / 4.f);
    for (int i = 0; i < 2; ++i) {
        float z = a[(size_t) i][(size_t) ap[i]];
        float y = -0.6f * out + z;
        a[(size_t) i][(size_t) ap[i]] = out + 0.6f * y;
        ap[i] = (ap[i] + 1) % al[i];
        out = y;
    }
    return out;
}

// ============================================================ voice
void ModelVoice::prepare (float s) { sr = s; room.prepare (s); active = false; }

void ModelVoice::choke (float ms) {
    if (! active) return;
    chokeStep = std::max (chokeStep, chokeGain / std::max (1.f, ms * 0.001f * sr));
}

float ModelVoice::pinkNoise() {
    float w = white();
    pink[0] = 0.99886f * pink[0] + w * 0.0555179f; pink[1] = 0.99332f * pink[1] + w * 0.0750759f;
    pink[2] = 0.96900f * pink[2] + w * 0.1538520f; pink[3] = 0.86650f * pink[3] + w * 0.3104856f;
    pink[4] = 0.55000f * pink[4] + w * 0.5329522f; pink[5] = -0.7616f * pink[5] - w * 0.0168980f;
    float p = pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6] + w * 0.5362f;
    pink[6] = w * 0.115926f;
    return p * 0.11f;   // ~ unit-variance-matched to white
}
// spectral tilt between white (0), pink (-3) and brown (-6 dB/oct)
float ModelVoice::colored (float slope) {
    float w = white();
    if (slope >= -0.01f) return w;
    float p = pinkNoise();
    if (slope >= -3.f) { float k = -slope / 3.f; return w * (1.f - k) + p * k; }
    brown = 0.997f * brown + 0.06f * w;
    float k = clampf ((-slope - 3.f) / 3.f, 0.f, 1.f);
    return p * (1.f - k) + brown * k;
}

float ModelVoice::shape (float x, float dB, int kind) const {
    float g = dbg (dB);
    switch (kind) {
        case 1: { float y = clampf (x * g, -1.f, 1.f); return 1.5f * y - 0.5f * y * y * y; }
        case 2: return clampf (x * g, -1.f, 1.f);
        case 3: return std::sin (1.5707963f * x * g);
        case 4: { float k = std::tanh (g + 0.25f) - std::tanh (0.25f); return (std::tanh (x * g + 0.25f) - std::tanh (0.25f)) / k; }
        default: return g > 1.f ? std::tanh (x * g) / std::tanh (g) : x;
    }
}

// estimate the peak of a noise chain the offline recipe would have normalised: run a short
// burst through a scratch copy of the filters and measure (allocation-free)
static float chainRms (MRng r, const MBq* fs, int nf, float slopeIgnored) {
    (void) slopeIgnored;
    MBq tmp[4];
    for (int i = 0; i < nf && i < 4; ++i) { tmp[i] = fs[i]; tmp[i].reset(); }
    double acc = 0.0;
    for (int i = 0; i < 1024; ++i) {
        float x = r.uni();
        for (int k = 0; k < nf && k < 4; ++k) x = tmp[k].tick (x);
        if (i >= 256) acc += (double) x * x;
    }
    return (float) std::sqrt (acc / 768.0) + 1e-6f;
}

// stiff plate modes: sorted m^2 + n^2 (m, n = 1..7) relative to the first, as in the generator
static const float kPlate[26] = { 1.f, 2.5f, 4.f, 5.f, 6.5f, 8.5f, 9.f, 10.f, 12.5f, 13.f, 14.5f, 16.f, 17.f, 18.5f, 20.f,
                                  20.5f, 22.5f, 25.f, 26.f, 26.5f, 29.f, 30.5f, 32.5f, 34.f, 36.f, 37.f };

float ModelVoice::coloredChainRms (float slope, const MBq* fs, int nf) {
    // run the real noise colour through scratch copies of the filters, then restore the generator
    MRng r0 = rng; float p0[7]; for (int i = 0; i < 7; ++i) p0[i] = pink[i]; float b0 = brown;
    MBq tmp[4];
    for (int i = 0; i < nf && i < 4; ++i) { tmp[i] = fs[i]; tmp[i].reset(); }
    double acc = 0.0;
    for (int i = 0; i < 2048; ++i) {
        float x = colored (slope);
        for (int k = 0; k < nf && k < 4; ++k) x = tmp[k].tick (x);
        if (i >= 512) acc += (double) x * x;
    }
    rng = r0; for (int i = 0; i < 7; ++i) pink[i] = p0[i]; brown = b0;
    return (float) std::sqrt (acc / 1536.0) + 1e-6f;
}

void ModelVoice::trigger (const ModelDef& def, const float* cooked, float velocity, float tuneSt, juce::uint32 seed) {
    d = &def; engine = def.engine;
    for (int i = 0; i < NumModelParams; ++i) c[i] = cooked[i];
    rng.seed (seed);
    vel = clampf (velocity, 0.05f, 1.f);
    float va = c[def.nparams - 1] * 0.01f;
    gain = def.gain * ((1.f - va) + va * vel);
    ps = std::pow (2.f, tuneSt / 12.f);
    t = 0.0; n = 0;
    for (auto& p : ph) p = 0.0;
    for (auto& b : f) b.reset();
    for (auto& v : pink) v = 0.f;
    brown = 0.f; compEnv = trF = trS = 0.f; chokeGain = 1.f; chokeStep = 0.f; dcX = dcY = 0.f;
    for (auto& v : lpS) v = 0.f;
    nPart = nPart2 = 0; nBurst = 0; roomWet = 0.f; crushStep = 1; crushCnt = 0; crushHold = 0.f; crushQ = 0.f;
    room.clear();
    auto setCrush = [this] (float amt01) {
        if (amt01 <= 0.001f) { crushStep = 1; crushQ = 0.f; return; }
        crushQ = std::pow (2.f, 15.f - 8.f * amt01);           // 16 .. 8 bits
        crushStep = std::max (1, (int) std::round (1.f / (1.f - 0.5f * amt01)));
        f[9].set (MBq::LP, sr * 0.45f * (1.f - 0.5f * amt01), 0.707f, 0.f, sr);
    };
    float L = 0.5f;

    switch (engine) {
        case ME_Kick: {
            fEnd = c[0] * ps; L = c[3] * 0.001f * 1.25f + c[12] * 0.001f + 0.04f;
            e[0].start (0.0002f, c[12] * 0.001f, c[3] * 0.001f, sr);
            e[1].start (0.0005f, 0.f, c[3] * 0.001f * 0.35f, sr);
            e[2].start (0.0005f, 0.f, 0.008f, sr); e[3].start (0.0005f, 0.f, 0.006f, sr); e[4].start (0.0005f, 0.f, 0.03f, sr);
            f[0].set (MBq::BP, c[5], 0.8f, 0, sr); f[1].set (MBq::HP, 2500, 0.707f, 0, sr);
            f[2].set (MBq::LowShelf, 90, 0.707f, c[10], sr); f[3].set (MBq::HighShelf, 4000, 0.707f, c[11], sr);
            f[4].set (MBq::LP, 14000, 0.707f, 0, sr);
            setCrush (c[14] * 0.01f);
            noiseScale[0] = 1.f / (3.f * chainRms (rng, &f[0], 1, 0));
            break;
        }
        case ME_Rumble: {
            fEnd = c[0] * ps; L = c[3] * 0.001f * 1.25f + 0.04f + c[8] * 1.1f;
            e[0].start (0.0002f, 0.f, c[3] * 0.001f, sr); e[1].start (0.0005f, 0.f, c[3] * 0.001f * 0.35f, sr);
            e[2].start (0.0005f, 0.f, 0.008f, sr); e[3].start (0.0005f, 0.f, 0.006f, sr);
            f[0].set (MBq::BP, 5000, 0.8f, 0, sr);
            f[1].set (MBq::LP, c[9], 0.707f, 0, sr); f[2].set (MBq::LP, c[9], 0.707f, 0, sr);
            f[3].set (MBq::LP, c[9] * 1.2f, 0.707f, 0, sr); f[4].set (MBq::LP, c[9] * 1.2f, 0.707f, 0, sr);
            f[5].set (MBq::LP, c[9] * 1.2f, 0.707f, 0, sr); f[6].set (MBq::LP, c[9] * 1.2f, 0.707f, 0, sr);
            room.set (c[8], 0.6f, sr);
            noiseScale[0] = 1.f / (3.f * chainRms (rng, &f[0], 1, 0));
            break;
        }
        case ME_Boom: {
            fEnd = c[0] * ps; L = c[1] * 1.2f + 0.05f;
            e[0].start (0.0003f, 0.f, c[1], sr); e[1].start (0.0005f, 0.f, 0.004f, sr);
            f[0].set (MBq::BP, 3000, 1.0f, 0, sr);
            noiseScale[0] = 1.f / (3.f * chainRms (rng, &f[0], 1, 0));
            break;
        }
        case ME_Snare: {
            float bodyHz = c[0] * ps, k = c[0] / d->p[0].def;
            fEnd = bodyHz; fStart = c[1] * ps; tauK = c[2] * 0.001f;
            L = std::max (c[9], c[3]) * 0.001f * 1.25f + 0.08f + (c[13] > 0.f ? d->fixed[6] : 0.f) + (c[14] > 0.f ? 0.4f : 0.f);
            e[0].start (d->fixed[0], 0.f, c[3] * 0.001f, sr);                                   // body
            float na = c[7] * 0.001f;
            e[1].start (na, 0.f, d->fixed[3], sr);                                              // noise burst
            e[2].start (std::max (0.004f, na), 0.f, c[9] * 0.001f, sr);                         // noise tail
            e[3].start (0.0003f, 0.f, 0.012f, sr); e[4].start (0.0001f, 0.f, 0.003f, sr);       // snap, click
            f[0].set (MBq::HP, c[6], 0.707f, 0, sr); f[1].set (MBq::HP, c[6], 0.707f, 0, sr); f[2].set (MBq::LP, 15000, 0.707f, 0, sr);
            f[3].set (MBq::BP, d->fixed[4] * ps * k, 1.2f, 0, sr); f[4].set (MBq::HP, 3000, 0.707f, 0, sr);
            // clap layer (clap recipe defaults: tail 0.25, tone scales with TUNE like the transposed snares)
            f[5].set (MBq::BP, d->fixed[7] * ps * k, 1.1f, 0, sr); f[6].set (MBq::HP, 600, 0.707f, 0, sr);
            f[7].set (MBq::HighShelf, 5000, 0.707f, 2.f, sr);
            roomHp.set (MBq::HP, 350, 0.707f, 0, sr); roomHp.reset(); roomLp.set (MBq::HP, 350, 0.707f, 0, sr); roomLp.reset();
            room.set (d->fixed[6], 0.35f, sr);
            noiseScale[0] = 1.f / (4.f * coloredChainRms (d->fixed[2], &f[0], 3));
            noiseScale[1] = 1.f / (3.f * chainRms (rng, &f[3], 1, 0));
            noiseScale[2] = 1.f / (2.5f * chainRms (rng, &f[4], 1, 0));
            nBurst = 4; float tt = 0.f;
            for (int b = 0; b < nBurst; ++b) {
                if (b) tt += 0.010f * (0.75f + 0.5f * rng.u01());
                burstAt[b] = (int) (tt * sr); burstDec[b] = b == nBurst - 1 ? 0.25f : 0.007f + 0.004f * rng.u01();
                burstAmp[b] = 0.8f + 0.2f * rng.u01(); burstEnv[b] = 0.f;
            }
            setCrush (0.f);
            break;
        }
        case ME_Snare909: {
            fEnd = c[0] * ps;
            L = std::max ({ c[8] * 0.001f + c[7] * 0.001f, c[3] * 0.001f, d->fixed[1] }) * 1.3f + 0.06f;
            float td = c[3] * 0.001f, tl = d->fixed[1];
            e[0].start (0.0003f, 0.f, td, sr); e[1].start (0.0003f, 0.f, tl, sr);
            e[2].start (0.0003f, 0.f, td * 0.8f, sr); e[3].start (0.0003f, 0.f, tl * 0.8f, sr);
            e[4].start (0.0005f, c[7] * 0.001f, c[8] * 0.001f, sr);
            e[5].start (0.0005f, 0.f, d->fixed[4], sr); e[6].start (0.0001f, 0.f, 0.0025f, sr);
            f[0].set (MBq::LP, c[10], 0.707f, 0, sr); f[1].set (MBq::LP, c[10], 0.707f, 0, sr);
            f[2].set (MBq::HP, c[9], 0.707f, 0, sr); f[3].set (MBq::HP, c[9], 0.707f, 0, sr);
            f[4].set (MBq::BP, c[12], 0.5f, 0, sr); f[5].set (MBq::HP, 4000, 0.707f, 0, sr);
            MBq sn[4] = { f[0], f[1], f[2], f[3] }; MBq gr[3] = { f[0], f[1], f[4] };
            noiseScale[0] = 1.f / (3.5f * chainRms (rng, sn, 4, 0));
            noiseScale[1] = 1.f / (3.5f * chainRms (rng, gr, 3, 0));
            noiseScale[2] = 1.f / (2.5f * chainRms (rng, &f[5], 1, 0));
            break;
        }
        case ME_Clap: {
            nBurst = juce::jlimit (2, 6, (int) c[0]); float tt = 0.f;
            for (int b = 0; b < nBurst; ++b) {
                if (b) tt += c[1] * 0.001f * (0.75f + 0.5f * rng.u01());
                burstAt[b] = (int) (tt * sr); burstDec[b] = b == nBurst - 1 ? c[4] * 0.001f : 0.007f + 0.004f * rng.u01();
                burstAmp[b] = 0.8f + 0.2f * rng.u01(); burstEnv[b] = 0.f;
            }
            L = c[4] * 0.001f * 1.5f + tt + 0.05f + (c[7] > 0.f ? c[8] : 0.f);
            f[0].set (MBq::BP, c[2] * ps, c[3], 0, sr); f[1].set (MBq::HP, 600, 0.707f, 0, sr); f[2].set (MBq::HighShelf, 5000, 0.707f, c[5], sr);
            room.set (c[8], 0.25f, sr);
            setCrush (c[9] * 0.01f);
            break;
        }
        case ME_Hat: {
            L = c[1] * 0.001f * 1.4f + 0.02f;
            static const float mults[3] = { 1.f, 1.3428f, 1.7231f };
            for (int k = 0; k < 3; ++k) { lpS[k] = c[0] * ps * mults[k] * (1.f + 0.004f * rng.uni()); lpS[3 + k] = 1.414f * (1.f + 0.02f * rng.uni()); }
            e[0].start (c[8] * 0.001f, 0.f, c[1] * 0.001f, sr);
            f[0].set (MBq::HP, c[5], 0.707f, 0, sr); f[1].set (MBq::HP, c[5], 0.707f, 0, sr); f[2].set (MBq::BP, c[5] * 0.7f, 1.2f, 0, sr);
            f[3].set (MBq::Peak, c[6], 0.8f, c[7], sr); f[4].set (MBq::LP, 14000, 0.707f, 0, sr);
            setCrush (c[10] * 0.01f);
            break;
        }
        case ME_Shaker: {
            L = c[0] * 0.001f + 0.02f; grainRate = c[2];
            for (auto& g : grains) g.len = 0;
            nextGrain = (int) (-std::log (std::max (1e-6f, rng.u01())) / grainRate * sr);
            f[0].set (MBq::BP, c[1] * ps, 0.9f, 0, sr); f[1].set (MBq::HP, 4000, 0.707f, 0, sr);
            break;
        }
        case ME_Tambourine: {
            float len = c[0] * 0.001f; int hits = juce::jlimit (1, 6, (int) c[1]);
            L = len + hits * c[2] * 0.001f + 0.05f;
            nBurst = hits; float tt = 0.f;
            for (int b = 0; b < hits; ++b) { burstAt[b] = (int) (tt * sr); burstAmp[b] = std::pow (0.65f, (float) b); burstEnv[b] = 0.f; tt += c[2] * 0.001f * (0.7f + 0.6f * rng.u01()); }
            burstDec[0] = std::exp (std::log (0.001f) / std::max (1.f, len * sr));
            nPart = 40;
            float lo = c[3] * ps, hi = 13000.f * ps;
            for (int i = 0; i < nPart; ++i) {
                float fr = lo * std::pow (hi / lo, rng.u01());
                part[i].start (fr, len * (0.3f + 0.7f * rng.u01()), 1.f / std::sqrt ((float) nPart), rng.u01() * kTwoPi, sr);
            }
            e[0].start (0.0005f, 0.f, 0.008f, sr); e[1].start (0.0005f, 0.f, 0.02f, sr);
            f[0].set (MBq::HP, 6000, 0.707f, 0, sr); f[1].set (MBq::BP, 900, 1.2f, 0, sr); f[2].set (MBq::HP, 2500, 0.707f, 0, sr);
            break;
        }
        case ME_Ride: {
            float dec = c[1]; L = dec * 1.1f + 0.05f;
            nPart = 56;
            for (int i = 0; i < nPart; ++i) {
                float fr = 2500.f * std::pow (12000.f / 2500.f, rng.u01()) * ps;
                part[i].start (fr, dec * (0.35f + 0.65f * rng.u01()) * std::pow (fr / 3000.f, -0.5f), (0.5f + 0.5f * rng.u01()) / std::sqrt ((float) nPart), rng.u01() * kTwoPi, sr);
            }
            static const float pr[5] = { 2.4f, 3.1f, 4.6f, 5.9f, 7.3f }, pd[5] = { 0.6f, 0.5f, 0.4f, 0.35f, 0.3f }, pa[5] = { 1.f, 0.8f, 0.6f, 0.45f, 0.35f };
            nPart2 = 5;
            for (int i = 0; i < 5; ++i) part2[i].start (c[0] * ps * pr[i] * (1.f + 0.01f * rng.uni()), dec * pd[i], pa[i] / 2.5f, rng.u01() * kTwoPi, sr);
            e[0].start (0.002f, 0.f, dec * 0.5f, sr); e[1].start (0.0005f, 0.f, 0.01f, sr);
            f[0].set (MBq::HP, 5000, 0.707f, 0, sr); f[1].set (MBq::HP, 3000, 0.707f, 0, sr);
            f[2].set (MBq::HP, 500, 0.707f, 0, sr); f[3].set (MBq::LP, 14000, 0.707f, 0, sr);
            noiseScale[0] = 1.f / (4.f * coloredChainRms (-2.f, &f[0], 1));
            break;
        }
        case ME_Crash: {
            float dec = c[1], att = c[3] * 0.001f; L = dec * 1.1f + 0.05f;
            static const float mults[4] = { 1.f, 1.3428f, 1.7231f, 2.1879f };
            for (int k = 0; k < 4; ++k) { lpS[k] = c[0] * ps * mults[k] * (1.f + 0.004f * rng.uni()); }
            nPart = 48;
            for (int i = 0; i < nPart; ++i) {
                float fr = 1500.f * std::pow (9000.f / 1500.f, rng.u01()) * ps;
                part[i].start (fr, dec * (0.4f + 0.5f * rng.u01()), 1.2f / std::sqrt ((float) nPart), rng.u01() * kTwoPi, sr);
            }
            e[0].start (att, 0.f, dec, sr); e[1].start (att, 0.f, dec * 0.75f, sr);
            f[0].set (MBq::HP, 700, 0.707f, 0, sr); f[1].set (MBq::HP, 700, 0.707f, 0, sr); f[2].set (MBq::HP, 400, 0.707f, 0, sr);
            f[3].set (MBq::LP, c[4], 0.707f, 0, sr); f[4].set (MBq::HighShelf, 6000, 0.707f, c[2], sr);
            noiseScale[0] = 1.f / (4.f * coloredChainRms (-3.f, &f[0], 2));
            break;
        }
        case ME_Tom: {
            fEnd = c[0] * ps; float dec = c[1] * 0.001f; L = dec * 1.3f + 0.05f;
            e[0].start (0.0004f, 0.f, dec, sr); e[1].start (0.0005f, 0.f, 0.006f, sr);
            static const float r[5] = { 1.594f, 2.136f, 2.296f, 2.653f, 2.918f }, a[5] = { 0.6f, 0.4f, 0.3f, 0.25f, 0.2f };
            nPart = 5;
            for (int i = 0; i < 5; ++i) part[i].start (fEnd * r[i], dec * 0.4f / (i + 1), a[i] * c[4] * 0.01f, rng.u01() * kTwoPi, sr);
            f[0].set (MBq::BP, 3000, 0.8f, 0, sr);
            break;
        }
        case ME_Conga: {
            fEnd = c[0] * ps; float dec = c[1] * 0.001f; L = dec * 1.4f + 0.04f;
            e[0].start (0.0005f, 0.f, dec, sr); e[1].start (0.0005f, 0.f, 0.02f, sr);
            static const float r[3] = { 1.594f, 2.136f, 2.65f }, dd[3] = { 0.4f, 0.25f, 0.18f }, a[3] = { 0.5f, 0.35f, 0.2f };
            nPart = 3;
            for (int i = 0; i < 3; ++i) part[i].start (fEnd * r[i], dec * dd[i], 0.4f * a[i], rng.u01() * kTwoPi, sr);
            f[0].set (MBq::BP, 2500, 0.9f, 0, sr); f[1].set (MBq::HP, 900, 0.707f, 0, sr);
            break;
        }
        case ME_Rim: {
            float tone = c[0] * ps, dec = c[2] * 0.001f; fEnd = c[1] * ps; L = dec * 2.f + 0.06f;
            static const float r[3] = { 1.f, 2.42f, 3.9f }, dd[3] = { 1.f, 0.6f, 0.4f }, a[3] = { 1.f, 0.45f, 0.2f };
            nPart = 3;
            for (int i = 0; i < 3; ++i) part[i].start (tone * r[i], dec * dd[i], a[i], rng.u01() * kTwoPi, sr);
            e[0].start (0.0005f, 0.f, dec * 0.8f, sr); e[1].start (0.0005f, 0.f, 0.002f, sr);
            f[0].set (MBq::HP, 3000, 0.707f, 0, sr);
            setCrush (c[3] * 0.01f);
            break;
        }
        case ME_Wood: {
            float fr = c[0] * ps, dec = c[1] * 0.001f; L = dec * 2.f + 0.03f;
            static const float r[3] = { 1.f, 2.76f, 5.4f }, dd[3] = { 1.f, 0.5f, 0.25f }, a[3] = { 1.f, 0.4f, 0.2f };
            nPart = 3;
            for (int i = 0; i < 3; ++i) part[i].start (fr * r[i], dec * dd[i], a[i], rng.u01() * kTwoPi, sr);
            e[0].start (0.0005f, 0.f, 0.0015f, sr); f[0].set (MBq::HP, 5000, 0.707f, 0, sr);
            break;
        }
        case ME_Glass: {
            float fr = c[0] * ps, dec = c[1] * 0.001f; L = dec * 1.2f + 0.03f;
            static const float r[4] = { 1.f, 2.63f, 5.12f, 8.3f }, dd[4] = { 1.f, 0.5f, 0.3f, 0.2f }, a[4] = { 1.f, 0.5f, 0.3f, 0.15f };
            nPart = 4;
            for (int i = 0; i < 4; ++i) part[i].start (fr * r[i], dec * dd[i], a[i], rng.u01() * kTwoPi, sr);
            e[0].start (0.0005f, 0.f, 0.006f, sr); f[0].set (MBq::HP, 500, 0.707f, 0, sr);
            break;
        }
        case ME_Snap: {
            L = 0.25f;
            e[0].start (0.0003f, 0.f, c[1] * 0.001f, sr); e[1].start (0.0005f, 0.f, 0.05f, sr); e[2].start (0.0005f, 0.f, 0.004f, sr);
            f[0].set (MBq::BP, c[0] * ps, 2.f, 0, sr); f[1].set (MBq::HP, 6000, 0.707f, 0, sr);
            break;
        }
        case ME_Clang: {
            float dec = c[1]; L = dec * 1.2f + 0.05f;
            nPart = 26;
            for (int i = 0; i < nPart; ++i) {
                float r = kPlate[i] * (1.f + 0.01f * rng.uni());
                part[i].start (c[0] * ps * r, dec * (0.3f + 0.7f * rng.u01()) / std::sqrt (r), (0.4f + 0.6f * rng.u01()) / std::sqrt ((float) i + 1.f), rng.u01() * kTwoPi, sr);
            }
            e[0].start (0.0005f, 0.f, 0.006f, sr);
            f[0].set (MBq::HP, 120, 0.707f, 0, sr);
            setCrush (c[4] * 0.01f);
            break;
        }
        case ME_Bell: {
            float fr = c[0] * ps, dec = c[1] * 0.001f; L = dec * 1.4f + 0.03f;
            static const float sets[3][4] = { { 1.f, 1.504f, 2.11f, 2.68f }, { 1.f, 1.5f, 2.2f, 3.0f }, { 1.f, 2.43f, 4.1f, 6.3f } };
            static const float dd[4] = { 1.f, 0.7f, 0.5f, 0.4f }, a[4] = { 1.f, 0.8f, 0.3f, 0.2f };
            int s = juce::jlimit (0, 2, (int) c[2]);
            nPart = 4;
            for (int i = 0; i < 4; ++i) part[i].start (fr * sets[s][i], dec * dd[i], a[i], rng.u01() * kTwoPi, sr);
            f[0].set (MBq::BP, c[3] * ps, 0.6f, 0, sr);
            break;
        }
        case ME_Riser: {
            L = c[0];
            f[0].set (MBq::BP, c[1], c[4], 0, sr);
            for (int k = 0; k < 3; ++k) ph[k] = rng.u01();
            break;
        }
        case ME_Impact: {
            fEnd = c[0] * ps * 0.6f; float dec = c[1]; L = dec * 1.1f + 0.05f + (c[4] > 0.f ? dec : 0.f);
            e[0].start (0.001f, 0.f, dec * 0.7f, sr); e[1].start (0.0005f, 0.f, 0.25f, sr);
            f[0].set (MBq::HP, 600, 0.707f, 0, sr);
            roomLp.set (MBq::LP, 4000, 0.707f, 0, sr); roomLp.reset();
            room.set (dec, 0.5f, sr);
            noiseScale[0] = 1.f / (4.f * coloredChainRms (-2.f, &f[0], 1));
            break;
        }
        case ME_Laser: {
            L = c[2] * 0.001f; e[0].start (0.0005f, 0.f, L, sr); e[1].start (0.001f, 0.f, L, sr);
            break;
        }
        case ME_Siren: {
            L = c[3]; f[0].set (MBq::LP, 4000, 0.707f, 0, sr);
            break;
        }
        case ME_SubDrop: {
            L = c[2];
            break;
        }
        case ME_Vox: {
            word = juce::jlimit (0, 6, (int) c[0]); L = c[2] * 0.001f;
            f[0].set (MBq::LP, 3500, 0.5f, 0, sr);
            fmtBlock = 0; jassert (L > 0.f);
            break;
        }
        case ME_Zap: {
            float dec = c[3] * 0.001f; L = dec * 1.5f + 0.02f;
            e[0].start (0.0003f, 0.f, dec, sr); e[1].start (0.0005f, 0.f, dec * 0.5f, sr);
            break;
        }
        case ME_NoiseHit: {
            float dec = c[0] * 0.001f; L = dec * 1.4f + 0.05f;
            e[0].start (0.001f, 0.f, dec, sr);
            f[0].set (MBq::HP, c[1], 0.707f, 0, sr); f[1].set (MBq::LP, 400, 0.707f, 0, sr);
            setCrush (c[4] * 0.01f);
            noiseScale[0] = 1.f / (4.f * coloredChainRms (-1.f, &f[0], 1));
            break;
        }
        case ME_Horn: {
            L = c[1];
            f[0].set (MBq::Peak, 1100, 2.f, 9.f, sr); f[1].set (MBq::Peak, 2400, 2.5f, 6.f, sr); f[2].set (MBq::HP, 300, 0.707f, 0, sr);
            f[3].set (MBq::LP, 150, 0.707f, 0, sr);
            for (int k = 0; k < 4; ++k) ph[k] = rng.u01();
            break;
        }
        default: break;
    }
    len = std::max (16, (int) (L * sr));
    active = true;
}

float ModelVoice::render() {
    if (! active) return 0.f;
    if (n >= len) { active = false; return 0.f; }
    float y = renderEngine();
    // dc blocker (offline recipes dc-block the result)
    float dy = y - dcX + 0.9993f * dcY; dcX = y; dcY = dy; y = dy;
    // sample-and-hold + quantise crush
    if (crushQ > 0.f) {
        if (crushCnt++ % crushStep == 0) crushHold = std::round (y * crushQ) / crushQ;
        y = f[9].tick (crushHold);
    }
    // anti-click fade over the last 5 ms, choke
    int remain = len - n;
    int fadeN = (int) (0.005f * sr);
    if (remain < fadeN) y *= (float) remain / (float) fadeN;
    if (chokeStep > 0.f) { chokeGain -= chokeStep; if (chokeGain <= 0.f) { active = false; return 0.f; } y *= chokeGain; }
    ++n; t += 1.0 / sr;
    if (! std::isfinite (y)) y = 0.f;
    return y * gain;
}

float ModelVoice::renderEngine() {
    const float T = (float) t, isr = 1.f / sr;
    switch (engine) {
        // ---------------------------------------------------------------- kicks
        case ME_Kick: case ME_Rumble: {
            bool rum = engine == ME_Rumble;
            float ratio = c[1], tau = c[2] * 0.001f;
            float clickAmt = c[4] * 0.01f, clickHz = rum ? 5000.f : c[5];
            float fr = fEnd * (1.f + (ratio - 1.f) * std::exp (-T / tau));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            ph[1] += clickHz * 0.45f * isr; ph[1] -= std::floor (ph[1]);
            float body = std::sin (kTwoPi * (float) ph[0]) * e[0].next() + 0.12f * std::sin (2.f * kTwoPi * (float) ph[0]) * e[1].next();
            float snap = f[0].tick (white()) * noiseScale[0] * e[2].next() + 0.5f * std::sin (kTwoPi * (float) ph[1]) * e[3].next();
            float x = body + 1.6f * clickAmt * snap;
            if (! rum && c[6] > 0.f) x += c[6] * 0.01f * f[1].tick (white()) * 1.7f * e[4].next();
            x = shape (x, rum ? c[5] : c[7], rum ? 0 : (int) c[8]);
            float punch = rum ? c[6] : c[9];
            if (punch > 0.f) {
                float ax = std::abs (x);
                float pf = std::exp (-1.f / (0.0005f * sr)), psl = std::exp (-1.f / (0.006f * sr));
                trF = (1.f - pf) * ax + pf * trF; trS = (1.f - psl) * ax + psl * trS;
                float diff = clampf (20.f * std::log10 ((trF + 1e-6f) / (trS + 1e-6f)), 0.f, 24.f) / 24.f;
                x *= dbg (punch * diff);
            }
            if (rum) {
                float wet = room.tick (x);
                wet = f[2].tick (f[1].tick (wet)) * 4.f;
                wet = shape (wet, c[10], 0);
                wet = f[6].tick (f[5].tick (f[4].tick (f[3].tick (wet))));
                float duck = 1.f - 0.95f * std::exp (-T / (c[11] * 0.001f / 3.f));
                x += wet * duck * c[7] * 0.01f * kRumbleGain;
                return x;
            }
            if (c[10] != 0.f) x = f[2].tick (x);
            if (c[11] != 0.f) x = f[3].tick (x);
            if (c[13] > 0.f) x = f[4].tick (shape (x, c[13], 4));
            return x;
        }
        case ME_Boom: {
            float fr = fEnd * (1.f + (c[2] - 1.f) * std::exp (-T / (c[3] * 0.001f)));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float x = std::sin (kTwoPi * (float) ph[0]) * e[0].next() + c[5] * 0.01f * f[0].tick (white()) * noiseScale[0] * e[1].next();
            return shape (x, c[4], 4);
        }
        // ---------------------------------------------------------------- snares
        case ME_Snare: {
            float fr = fEnd + (fStart - fEnd) * std::exp (-T / tauK);
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float body = shape (std::sin (kTwoPi * (float) ph[0]) * e[0].next(), d->fixed[1], 0);
            float nz = f[2].tick (f[1].tick (f[0].tick (colored (d->fixed[2])))) * noiseScale[0];
            float nenv = std::max (e[1].next(), dbg (c[8]) * e[2].next());
            float sn = f[3].tick (white()) * noiseScale[1] * e[3].next();
            float ck = f[4].tick (white()) * noiseScale[2] * e[4].next();
            float bodyAmt = c[4] * 0.01f, noiseAmt = c[5] * 0.01f, snapAmt = c[11] * 0.01f, clickAmt = c[10] * 0.01f;
            float x = bodyAmt * body + noiseAmt * nz * nenv + snapAmt * sn + clickAmt * ck;
            // the generator normalises this sum to its true peak; q-norm fitted to that (see tools/kitgen)
            float pk = std::pow (std::pow (1.188f * bodyAmt, 2.736f) + std::pow (1.002f * noiseAmt, 2.736f)
                                 + std::pow (1.056f * snapAmt, 2.736f) + std::pow (0.878f * clickAmt, 2.736f), 1.f / 2.736f);
            x /= std::max (0.2f, pk);
            x = shape (x, d->fixed[5], 0);
            if (c[12] > 0.f) x = shape (x, c[12], 1);
            if (c[14] > 0.f) {   // clap layer
                float env = 0.f;
                for (int b = 0; b < nBurst; ++b) {
                    if (n < burstAt[b]) continue;
                    int k = n - burstAt[b]; float a = 0.0004f * sr;
                    float v = k < a ? k / a : std::exp (std::log (0.001f) * (k - a) / std::max (1.f, burstDec[b] * sr));
                    env = std::max (env, v * burstAmp[b]);
                }
                float src = f[7].tick (f[6].tick (f[5].tick (white())));
                x += c[14] * 0.01f * shape (src * env * 3.f, 6.f, 0);
            }
            if (c[13] > 0.f) {
                float wet = room.tick (x);
                wet = roomLp.tick (roomHp.tick (wet));
                x += wet * c[13] * 0.01f;
            }
            return x;
        }
        case ME_Snare909: {
            float bend = 1.f + c[2] * std::exp (-T / d->fixed[0]);
            float tri = d->fixed[2], shp = d->fixed[3], spread = c[1];
            auto osc = [&] (double& p, float freq) {
                p += freq * isr; p -= std::floor (p);
                float ph1 = (float) p, triv = 2.f * std::abs (2.f * (ph1 - std::floor (ph1 + 0.5f))) - 1.f;
                return shape ((1.f - tri) * std::sin (kTwoPi * ph1) + tri * triv, shp, 0);
            };
            float tail = dbg (c[5]);
            float o1 = osc (ph[0], fEnd * bend) * std::max (e[0].next(), tail * e[1].next());
            float o2 = osc (ph[1], fEnd * spread * bend) * std::max (e[2].next(), tail * e[3].next());
            float tone = (o1 + 0.75f * o2) / 1.75f;
            float nz = f[1].tick (f[0].tick (white()));
            float sn = f[3].tick (f[2].tick (nz)) * noiseScale[0] * e[4].next();
            float gr = f[4].tick (nz) * noiseScale[1] * e[5].next();
            float ck = f[5].tick (white()) * noiseScale[2] * e[6].next();
            float ta = c[4] * 0.01f, sa = c[6] * 0.01f, ga = c[11] * 0.01f, ca = c[13] * 0.01f;
            float x = ta * tone + sa * sn + ga * gr + ca * ck;
            float pk = std::pow (std::pow (1.344f * ta, 2.961f) + std::pow (0.762f * sa, 2.961f)
                                 + std::pow (0.869f * ga, 2.961f) + std::pow (0.953f * ca, 2.961f), 1.f / 2.961f);
            x /= std::max (0.2f, pk);
            return shape (x, c[14], 0);
        }
        case ME_Clap: {
            float env = 0.f;
            for (int b = 0; b < nBurst; ++b) {
                if (n < burstAt[b]) continue;
                int k = n - burstAt[b]; float a = 0.0004f * sr;
                float v = k < a ? k / a : std::exp (std::log (0.001f) * (k - a) / std::max (1.f, burstDec[b] * sr));
                env = std::max (env, v * burstAmp[b]);
            }
            float src = f[2].tick (f[1].tick (f[0].tick (white())));
            float x = shape (src * env * 3.f, c[6], 0);
            if (c[7] > 0.f) x += room.tick (x) * c[7] * 0.01f;
            return x;
        }
        // ---------------------------------------------------------------- metal / noise
        case ME_Hat: {
            float m = 0.f;
            for (int k = 0; k < 3; ++k) {
                ph[k] += lpS[k] * isr; ph[k] -= std::floor (ph[k]);
                ph[3 + k] += lpS[k] * lpS[3 + k] * isr; ph[3 + k] -= std::floor (ph[3 + k]);
                m += std::sin (kTwoPi * (float) ph[k] + c[2] * std::sin (kTwoPi * (float) ph[3 + k]));
            }
            float x = c[3] * 0.01f * m / 3.f + c[4] * 0.01f * white();
            float h = f[1].tick (f[0].tick (x)) + 0.18f * f[2].tick (x);
            h = f[4].tick (f[3].tick (h)) * e[0].next();
            return shape (h, c[9], 0);
        }
        case ME_Shaker: {
            while (nextGrain <= n) {
                for (auto& g : grains) if (g.len == 0) { g.len = 20 + (int) (rng.u01() * 40.f); g.pos = 0; break; }
                nextGrain += 1 + (int) (-std::log (std::max (1e-6f, rng.u01())) / grainRate * sr);
            }
            float gsum = 0.f;
            for (auto& g : grains)
                if (g.len > 0) {
                    float w = 0.5f - 0.5f * std::cos (kTwoPi * (float) g.pos / (float) (g.len - 1));
                    gsum += white() * w;
                    if (++g.pos >= g.len) g.len = 0;
                }
            float x = f[1].tick (f[0].tick (gsum));
            float Lc = c[0] * 0.001f, sw = c[3] * 0.01f * Lc, mid = 0.5f * Lc;
            float env = T < sw ? T / sw : (T < mid ? 1.f - 0.55f * (T - sw) / std::max (1e-4f, mid - sw) : 0.45f * std::max (0.f, 1.f - (T - mid) / std::max (1e-4f, Lc - mid)));
            return x * env;
        }
        case ME_Tambourine: {
            float exc = 0.f;
            for (int b = 0; b < nBurst; ++b) {
                if (n < burstAt[b]) continue;
                if (n == burstAt[b]) burstEnv[b] = burstAmp[b];
                else burstEnv[b] *= burstDec[0];
                exc = std::max (exc, burstEnv[b]);
            }
            float ring = 0.f;
            for (int i = 0; i < nPart; ++i) ring += part[i].tick();
            float nz = f[0].tick (white());
            float x = (ring * 2.f + 0.5f * nz) * exc + 0.6f * nz * e[0].next() + c[4] * 0.01f * f[1].tick (white()) * e[1].next();
            return f[2].tick (x);
        }
        case ME_Ride: {
            float cloud = 0.f, ping = 0.f;
            for (int i = 0; i < nPart; ++i) cloud += part[i].tick();
            for (int i = 0; i < nPart2; ++i) ping += part2[i].tick();
            float wash = f[0].tick (colored (-2.f)) * noiseScale[0] * e[0].next();
            float stick = f[1].tick (white()) * e[1].next();
            float x = 1.6f * cloud + (0.35f + c[2] * 0.01f) * ping + c[3] * 0.01f * 0.6f * wash + c[4] * 0.01f * stick;
            return f[3].tick (f[2].tick (x));
        }
        case ME_Crash: {
            float nz = f[1].tick (f[0].tick (colored (-3.f))) * noiseScale[0] * e[0].next();
            float m = 0.f;
            for (int k = 0; k < 4; ++k) {
                ph[k] += lpS[k] * isr; ph[k] -= std::floor (ph[k]);
                ph[4 + k] += lpS[k] * 1.414f * isr; ph[4 + k] -= std::floor (ph[4 + k]);
                m += std::sin (kTwoPi * (float) ph[k] + 6.f * std::sin (kTwoPi * (float) ph[4 + k]));
            }
            float modal = 0.f;
            for (int i = 0; i < nPart; ++i) modal += part[i].tick();
            float x = nz + c[5] * 0.01f * (m / 4.f) * e[1].next() + modal;
            x = f[3].tick (f[2].tick (x));
            return c[2] != 0.f ? f[4].tick (x) : x;
        }
        case ME_Tom: {
            float fr = fEnd * (1.f + (c[2] - 1.f) * std::exp (-T / 0.03f));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float x = std::sin (kTwoPi * (float) ph[0]) * e[0].next();
            for (int i = 0; i < nPart; ++i) x += part[i].tick();
            x += c[3] * 0.01f * f[0].tick (white()) * 1.7f * e[1].next();
            return shape (x, c[5], 0);
        }
        case ME_Conga: {
            float fr = fEnd * (1.f + 0.12f * std::exp (-T / 0.01f));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float x = std::sin (kTwoPi * (float) ph[0]) * e[0].next();
            for (int i = 0; i < nPart; ++i) x += part[i].tick();
            x += c[2] * 0.01f * f[1].tick (f[0].tick (white())) * 1.7f * e[1].next();
            return shape (x, 3.f, 0);
        }
        case ME_Rim: {
            float w = 0.f;
            for (int i = 0; i < nPart; ++i) w += part[i].tick();
            float fr = fEnd * (1.f + 0.3f * std::exp (-T / 0.004f));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float body = std::sin (kTwoPi * (float) ph[0]) * e[0].next();
            float clk = f[0].tick (white()) * 1.7f * e[1].next();
            return shape (w + 0.6f * body + 0.7f * clk, 6.f, 0);
        }
        case ME_Wood: {
            float x = 0.f;
            for (int i = 0; i < nPart; ++i) x += part[i].tick();
            x += c[2] * 0.01f * f[0].tick (white()) * 1.7f * e[0].next();
            return shape (x, c[3], 0);
        }
        case ME_Glass: {
            float x = 0.f;
            for (int i = 0; i < nPart; ++i) x += part[i].tick();
            x += c[2] * 0.01f * white() * e[0].next();
            return f[0].tick (x);
        }
        case ME_Snap: {
            float env = e[0].next();
            if (T >= 0.006f) env = std::max (env, c[2] * 0.01f * e[1].next());
            return f[0].tick (white()) * 2.f * env + 0.3f * f[1].tick (white()) * 1.7f * e[2].next();
        }
        case ME_Clang: {
            float x = 0.f;
            for (int i = 0; i < nPart; ++i) x += part[i].tick();
            x += c[3] * 0.01f * white() * e[0].next();
            return shape (f[0].tick (x), c[2], 3);
        }
        case ME_Bell: {
            float x = 0.f;
            for (int i = 0; i < nPart; ++i) x += part[i].tick();
            x = f[0].tick (x) * 2.f + 0.3f * x;
            return shape (x, c[4], 0);
        }
        // ---------------------------------------------------------------- fx
        case ME_Riser: {
            float Lr = c[0], k = (T / Lr) * (T / Lr);
            if ((n & 31) == 0) f[0].set (MBq::BP, c[1] * std::pow (c[2] / c[1], k), c[4], 0, sr);
            float x = f[0].tick (white()) * 1.7f;
            if (c[3] > 0.f) {
                float fr = 220.f * ps * std::pow (2.f, k), s = 0.f;
                static const float det[3] = { -20.f, 0.f, 20.f };
                for (int v = 0; v < 3; ++v) {
                    ph[v] += fr * std::pow (2.f, det[v] / 1200.f) * isr; ph[v] -= std::floor (ph[v]);
                    s += 2.f * (float) ph[v] - 1.f;
                }
                x += c[3] * 0.01f * 0.3f * s / 1.732f * k;
            }
            float env = T < 0.9f * Lr ? T / (0.9f * Lr) : std::max (0.f, (Lr - T) / (0.1f * Lr));
            return x * env;
        }
        case ME_Impact: {
            float fr = fEnd * (1.f + 2.f * std::exp (-T / 0.08f));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float boom = std::sin (kTwoPi * (float) ph[0]) * e[0].next();
            float cr = c[2] * 0.01f * f[0].tick (colored (-2.f)) * noiseScale[0] * e[1].next();
            float x = shape (boom + cr, c[3], 0);
            // darkened room like the generator's IR (damp_hz 4000); its long noise IR lifts the boom tail, hence x2
            if (c[4] > 0.f) x += room.tick (roomLp.tick (x)) * c[4] * 0.02f;
            return x;
        }
        case ME_Laser: {
            float Lr = c[2] * 0.001f;
            float fr = c[1] + (c[0] - c[1]) * std::exp (-T / (Lr * 0.18f));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            ph[1] += fr * 0.5f * isr; ph[1] -= std::floor (ph[1]);
            float x = std::sin (kTwoPi * (float) ph[0] + c[3] * e[0].next() * std::sin (kTwoPi * (float) ph[1])) * e[1].next();
            return shape (x, c[4], 0);
        }
        case ME_Siren: {
            float Lr = c[3];
            float fr = (c[0] + (c[1] - c[0]) * (0.5f - 0.5f * std::cos (kTwoPi * c[2] * T))) * ps;
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float p = (float) ph[0];
            float tri = 2.f * std::abs (2.f * (p - std::floor (p + 0.5f))) - 1.f, sq = p < 0.3f ? 1.f : -1.f;
            float x = f[0].tick (0.7f * tri + 0.15f * sq);
            float env = T < 0.02f ? T / 0.02f : (T < Lr - 0.1f ? 1.f : std::max (0.f, (Lr - T) / 0.1f));
            return x * env;
        }
        case ME_SubDrop: {
            float Lr = c[2];
            float fr = (c[1] + (c[0] - c[1]) * std::exp (-T / (Lr * 0.35f))) * ps;
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            float env = T < 0.01f ? T / 0.01f : (T < 0.7f * Lr ? 1.f - 0.2f * (T - 0.01f) / (0.7f * Lr - 0.01f) : 0.8f * std::max (0.f, (Lr - T) / (0.3f * Lr)));
            return shape (std::sin (kTwoPi * (float) ph[0]) * env, c[3], 4);
        }
        case ME_Vox: {
            // vowel formant tables (F1..F4) and word tracks, as in the generator
            static const float V[6][4] = { { 730, 1090, 2440, 3400 }, { 530, 1840, 2480, 3500 }, { 300, 2200, 3000, 3700 },
                                           { 500, 850, 2400, 3300 }, { 330, 800, 2250, 3200 }, { 640, 1190, 2390, 3300 } };
            enum { A = 0, E, I, O, U, UH };
            float Lr = c[2] * 0.001f, x01 = T / Lr;
            int v0 = A, v1 = A; float w = 0.f;   // vowel blend along the word
            float pc = 1.f;                        // pitch contour
            switch (word) {
                case 0: v0 = E; v1 = x01 < 0.6f ? E : I; w = x01 < 0.6f ? 0.f : (x01 - 0.6f) / 0.4f; pc = 1.f - 0.14f * x01; break;   // hey
                case 1: if (T < 0.07f) { v0 = I; v1 = E; w = T / 0.07f; } else { v0 = E; v1 = A; w = clampf ((T - 0.07f) / std::max (0.01f, 0.45f * Lr - 0.07f), 0.f, 1.f); }
                        pc = 1.f - 0.18f * x01; break;                                                                                // yeah
                case 2: v0 = v1 = A; pc = 1.f - 0.03f * x01; break;                                                                    // ah
                case 3: v0 = O; v1 = U; w = x01; pc = 1.f - 0.1f * x01; break;                                                          // oh
                case 4: v0 = O; v1 = I; w = clampf ((x01 - 0.4f) / 0.35f, 0.f, 1.f); pc = 1.f + 0.15f * x01; break;                     // oi
                case 5: if (T < 0.03f) { v0 = U; v1 = O; w = T / 0.03f; } else { v0 = v1 = O; } pc = T < 0.05f ? 0.9f + 3.f * T : 1.05f - 0.2f * x01; break; // go
                default: v0 = v1 = UH; pc = 1.f - 0.2f * x01; break;                                                                   // uh
            }
            if ((n & 63) == 0) {
                static const float q[4] = { 6.f, 9.f, 12.f, 14.f };
                for (int k = 0; k < 4; ++k)
                    f[1 + k].set (MBq::BP, (V[v0][k] * (1.f - w) + V[v1][k] * w) * c[3], q[k], 0, sr);
            }
            float vib = 1.f + 0.012f * std::sin (kTwoPi * 5.5f * T) * clampf ((T - 0.12f) / 0.2f, 0.f, 1.f);
            lpS[0] = 0.999f * lpS[0] + 0.001f * white() * 30.f;                 // slow jitter
            float f0 = c[1] * ps * pc * vib * (1.f + 0.004f * lpS[0]);
            ph[0] += f0 * isr; ph[0] -= std::floor (ph[0]);
            float p = (float) ph[0];
            float src = f[0].tick (0.6f * (2.f * p - 1.f) + 0.4f * (p < 0.1f ? 1.f : -1.f)) + c[5] * 0.01f * white();
            if (word == 0 && T < 0.06f) src = white() * 0.5f;                   // aspirated "h"
            if (word == 5 && T < 0.012f) src += white() * 1.0f;                  // velar burst
            static const float g[4] = { 1.f, 0.5f, 0.25f, 0.12f };
            float x = 0.f;
            for (int k = 0; k < 4; ++k) x += g[k] * f[1 + k].tick (src);
            float env = T < 0.012f ? T / 0.012f : (x01 < 0.7f ? 1.f - 0.15f * (T - 0.012f) / (0.7f * Lr - 0.012f) : 0.85f * std::max (0.f, (1.f - x01) / 0.3f));
            x = shape (x * env * 3.f, c[4], 0);
            // gentle compressor (-14 dB, 3:1, 3/60 ms, +4 dB)
            float lvl = 20.f * std::log10 (std::abs (x) + 1e-9f), over = lvl + 14.f, gr = over > 0.f ? over * (1.f - 1.f / 3.f) : 0.f;
            float co = gr > compEnv ? std::exp (-1.f / (0.003f * sr)) : std::exp (-1.f / (0.06f * sr));
            compEnv = co * compEnv + (1.f - co) * gr;
            return x * dbg (4.f - compEnv);
        }
        case ME_Zap: {
            float fr = c[1] + (c[0] - c[1]) * std::exp (-T / (c[2] * 0.001f));
            ph[0] += fr * isr; ph[0] -= std::floor (ph[0]);
            ph[1] += fr * 1.5f * isr; ph[1] -= std::floor (ph[1]);
            float idx = c[4] * e[1].next();
            float x = c[4] > 0.f ? std::sin (kTwoPi * (float) ph[0] + idx * std::sin (kTwoPi * (float) ph[1])) : std::sin (kTwoPi * (float) ph[0]);
            return shape (x * e[0].next(), c[5], 0);
        }
        case ME_NoiseHit: {
            float x = f[0].tick (colored (-1.f)) * noiseScale[0] * e[0].next();
            if (c[2] > 0.f) x *= f[1].tick (std::sin (kTwoPi * c[2] * T) > -0.2f ? 1.f : 0.f);
            return shape (x, c[3], 2);
        }
        case ME_Horn: {
            float Lr = c[1]; int stabs = juce::jlimit (1, 6, (int) c[2]);
            float vib = 1.f + 0.01f * std::sin (kTwoPi * 6.f * T), s = 0.f;
            static const float det[4] = { -14.f, -5.f, 4.f, 13.f };
            for (int v = 0; v < 4; ++v) {
                ph[v] += c[0] * ps * std::pow (2.f, det[v] / 1200.f) * vib * isr; ph[v] -= std::floor (ph[v]);
                s += 2.f * (float) ph[v] - 1.f;
            }
            float x = f[2].tick (f[1].tick (f[0].tick (s / 4.f)));
            x = shape (x, c[3], 0);
            float seg = Lr / (stabs + 0.6f), gate = 0.f;
            for (int k = 0; k < stabs; ++k) { float st = k * seg, en = st + seg * (k < stabs - 1 ? 0.75f : 1.4f); if (T >= st && T < en) gate = 1.f; }
            return x * f[3].tick (gate);
        }
        default: return 0.f;
    }
}

} // namespace mds
