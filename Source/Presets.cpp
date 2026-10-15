#include "Presets.h"
#include "PluginProcessor.h"
#include <map>
#include <algorithm>

namespace mds {

// kit recipe -> full state tree (the on-disk factory/user format)
juce::ValueTree kitToStateVT (const Kit& k, int index) {
    juce::ValueTree v ("DRUMSYNTH");
    v.setProperty ("kit", index, nullptr);
    v.setProperty ("kitName", k.name, nullptr);
    v.setProperty ("master", k.master, nullptr);
    v.setProperty ("limiter", true, nullptr);
    v.setProperty ("selected", 0, nullptr);
    v.setProperty ("revDecay", k.revDecay, nullptr); v.setProperty ("revDamp", k.revDamp, nullptr);
    v.setProperty ("dlyTime", k.dlyTime, nullptr);   v.setProperty ("dlyFb", k.dlyFb, nullptr);
    for (int p = 0; p < NumParts; ++p) {
        juce::ValueTree pv = k.parts[p].toVT();
        pv.setProperty ("id", p, nullptr);
        pv.setProperty ("level", k.level[p], nullptr);
        pv.setProperty ("pan", k.pan[p], nullptr);
        pv.setProperty ("mute", k.mute[p], nullptr);
        pv.setProperty ("solo", false, nullptr);
        pv.setProperty ("midi", partMidiNote (p), nullptr);
        pv.setProperty ("sendRev", k.sendRev[p], nullptr);
        pv.setProperty ("sendDly", k.sendDly[p], nullptr);
        pv.addChild (k.partFx[p].toVT(), -1, nullptr);
        v.addChild (pv, -1, nullptr);
    }
    v.addChild (k.globalFx.toVT(), -1, nullptr);
    return v;
}

// recipe helpers: set slots by index, append an FX to the first free slot
static void S (PartParams& pp, std::initializer_list<std::pair<int, float>> v) {
    for (auto& kv : v) pp.p[kv.first] = kv.second;
}
static void addFx (FxChainState& ch, int type, std::initializer_list<float> knobs) {
    for (int s = 0; s < FxChainState::NumSlots; ++s) {
        auto& sl = ch.slots[s];
        if (sl.type.load() != FX_NONE) continue;
        FxProcessor::defaultSlot (type, sl);
        std::atomic<float>* ks[6] = { &sl.p0, &sl.p1, &sl.p2, &sl.p3, &sl.p4, &sl.p5 };
        int n = 0;
        for (float v : knobs) if (n < 6) ks[n++]->store (v);
        return;
    }
}

// One factory kit = one row: it picks a kick type (+ exact pitch), a snare
// design (+ tune / decay / tone offsets), hat, cymbal and tom types, a
// percussion flavour, an FX pack and its send-reverb amount. The type
// tables below are the sound design; rows combine them so kits differ in
// every part, snares included (12 distinct designs, not one with tweaks).

enum Perc { P_ORGANIC, P_TECHY, P_PARTY, P_DEEP, P_BIG, P_AFRO, P_RETRO, P_GLITCH };
enum Pack { FX_P_CLEAN, FX_P_BREAK, FX_P_GRIND, FX_P_WEIGHT, FX_P_BLAST, FX_P_DARK, FX_P_LOFI,
            FX_P_GATED, FX_P_SMASH, FX_P_HARD, FX_P_BOUNCE, FX_P_WARM, FX_P_RUMBLE, FX_P_POLISH };

// kick types: sweep st, sweep ms, decay ms, click, drive, sub, punch
enum KickT { K_PUNCHY, K_ROUND, K_LONG, K_HARD, K_DEEP, K_CLICKY, K_BOOMY, K_TIGHT, K_SOFT, K_DRIVEN, K_MID, K_SUB };
struct KickDef { float sweep, sweepMs, dec, click, drive, sub, punch; };
static const KickDef kKick[] = {
    { 30,  70,  260, 55, 20, 35, 65 },  // PUNCHY
    { 22,  70,  380, 30, 15, 45, 55 },  // ROUND
    { 12,  45, 1800, 20, 25, 75, 60 },  // LONG (sustained sub)
    { 44,  80,  500, 85, 85, 40, 95 },  // HARD
    { 26, 160,  750, 20, 15, 70, 60 },  // DEEP
    { 36,  45,  220, 80, 45, 30, 80 },  // CLICKY
    { 36, 140,  420, 35, 30, 60, 75 },  // BOOMY
    { 20,  40,  200, 60, 15, 25, 60 },  // TIGHT
    { 16,  70,  320, 15, 20, 45, 45 },  // SOFT
    { 34,  70,  450, 70, 70, 45, 85 },  // DRIVEN
    { 28,  60,  320, 45, 30, 40, 70 },  // MID
    { 30,  90,  550, 40, 40, 55, 75 },  // SUB
};

// snare designs (slot values) + WAVE/NOISE type + transient lift
enum SnareT { S_CRACK, S_TIGHT, S_FAT, S_WASH, S_TONAL, S_GRIT, S_DUST, S_METAL, S_ROOM, S_ZAP, S_SOFT, S_BITE };
struct SnareDef {
    float tune, sweep, sweepMs, curve, atk, dec, body, noise, nDec, tone, drive, comp, nzHp, punch;
    int wave, noiseType; bool transient;
};
static const SnareDef kSnare[] = {
//    tune sweep  ms crv  atk  dec body noise ndec tone drv comp  nzHP pun wave nz  trans
    {   1, 26, 32, 18, .1f, 170, 85,  80, 170, 70, 25, 40,  500, 80, 0, 0, true  },  // CRACK: wide drop, crisp wires
    {   7, 10,  8, 25,.05f,  80, 90,  45,  70, 85, 20, 50, 1500, 90, 0, 0, true  },  // TIGHT: short, high, rim-like
    {  -4, 14, 40, 30, .3f, 260, 95,  55, 220, 45, 30, 60,  300, 70, 1, 1, false },  // FAT: low, long, thick tri body
    {   2,  4, 10, 40, .5f,  90, 30, 100, 260, 60, 10, 50,  700, 40, 0, 0, false },  // WASH: noise-forward, clap/brush
    {   3, 36, 60, 20, .1f, 140, 90,  45, 150, 80, 10, 30, 1200, 60, 1, 0, false },  // TONAL: pitched electronic
    {   2, 24, 20, 20, .1f, 150, 80,  85, 160, 75, 70, 70,  800, 85, 3, 4, true  },  // GRIT: square body, digital noise, driven
    {  -2, 10, 30, 35, .4f, 200, 75,  70, 220, 25, 30, 40,  250, 50, 0, 6, false },  // DUST: dark tape wires
    {   5, 18, 25, 25, .1f, 180, 80,  60, 150, 75, 30, 40,  900, 70, 4, 3, false },  // METAL: FM ring, metallic wires
    {   0, 18, 25, 25, .2f, 300, 85,  75, 380, 62, 20, 55,  400, 75, 0, 1, false },  // ROOM: long body + tail
    {   0, 48, 15, 10, .1f, 120, 75,  55, 120, 80, 20, 40, 1000, 80, 0, 2, true  },  // ZAP: laser drop
    {   1,  6, 15, 40, 1.f, 150, 70,  65, 180, 50,  0, 20,  350, 30, 0, 1, false },  // SOFT: gentle, slow attack
    {  10,  8,  6, 20,.05f,  60, 95,  35,  60, 90, 40, 60, 2500, 95, 3, 0, true  },  // BITE: mid-forward rimshot
};

// hats: tune, CHH decay, OHH decay, noise tone, bright, noise type, level, strike
enum HatT { H_BRIGHT, H_CRISP, H_DARK, H_METALLIC, H_TIGHT, H_WASHY, H_DIGITAL, H_DUSTY };
struct HatDef { float tune, chDec, ohDec, tone, bright; int nz; float lvl, strike; };
static const HatDef kHat[] = {
    {  3, 45, 300, 85, 65, 2, .65f, 50 },  // BRIGHT
    {  2, 70, 420, 85, 55, 2, .60f, 60 },  // CRISP
    { -5, 90, 600, 35, 20, 6, .85f, 40 },  // DARK
    {  3, 40, 280, 75, 70, 3, .80f, 55 },  // METALLIC
    {  5, 28, 220, 90, 78, 2, .60f, 65 },  // TIGHT
    {  0, 90, 600, 70, 45, 1, .60f, 35 },  // WASHY
    {  6, 25, 200, 90, 80, 4, .60f, 70 },  // DIGITAL
    { -3, 70, 380, 40, 25, 6, .85f, 40 },  // DUSTY
};

// ride/crash: tune, ride decay, crash decay, damp
enum CymT { C_BRIGHT, C_DARK, C_SHORT, C_BIG, C_MID };
struct CymDef { float tune, ride, crash, damp; };
static const CymDef kCym[] = {
    {  0, 1800, 2600, 30 }, { -4, 2800, 4800, 55 }, { 3, 1200, 1800, 40 }, { 0, 2600, 4200, 22 }, { -2, 2000, 3000, 40 },
};

// toms: tune, decay, sweep
enum TomT { T_TIGHT, T_MID, T_DEEP, T_SYNTH, T_BIG };
struct TomDef { float tune, dec, sweep; };
static const TomDef kTom[] = { { 2, 250, 14 }, { 0, 350, 14 }, { -4, 500, 10 }, { 0, 300, 24 }, { -1, 500, 20 } };

struct KitDef {
    const char* name;
    int kick; float kHz;
    int snare; float sTune, sDecMul, sTone;
    int hat, cym, tom, perc, fx;
    float rev, revDecay, dly; // snare send to the shared reverb, its decay, perc/SFX delay send
};

static const KitDef kKits[] = {
    { "Steel Roller", K_PUNCHY, 58, S_CRACK, 0, 1.0f, 0, H_CRISP, C_BRIGHT, T_MID, P_ORGANIC, FX_P_BREAK, 0.0f, 0.5f, 0.f },
    { "Copper Line", K_CLICKY, 62, S_TIGHT, 0, 1.0f, 0, H_TIGHT, C_SHORT, T_TIGHT, P_RETRO, FX_P_BOUNCE, 0.1f, 0.4f, 0.f },
    { "Glass Hour", K_SOFT, 54, S_SOFT, 0, 1.1f, 10, H_WASHY, C_BIG, T_MID, P_PARTY, FX_P_POLISH, 0.25f, 0.7f, 0.1f },
    { "Night Freight", K_DRIVEN, 52, S_GRIT, 0, 1.0f, 0, H_METALLIC, C_MID, T_SYNTH, P_TECHY, FX_P_GRIND, 0.05f, 0.5f, 0.f },
    { "Velvet Static", K_ROUND, 55, S_DUST, 0, 1.0f, 0, H_DUSTY, C_DARK, T_DEEP, P_ORGANIC, FX_P_LOFI, 0.15f, 0.5f, 0.f },
    { "Iron Bloom", K_HARD, 50, S_BITE, 0, 1.0f, 0, H_METALLIC, C_SHORT, T_SYNTH, P_GLITCH, FX_P_SMASH, 0.0f, 0.5f, 0.f },
    { "Paper Lantern", K_SOFT, 56, S_WASH, 0, 1.0f, 0, H_WASHY, C_MID, T_MID, P_PARTY, FX_P_CLEAN, 0.2f, 0.6f, 0.15f },
    { "Low Orbit", K_DEEP, 44, S_ROOM, -2, 1.0f, -10, H_DARK, C_DARK, T_DEEP, P_DEEP, FX_P_DARK, 0.3f, 0.85f, 0.f },
    { "Signal Fade", K_MID, 56, S_ZAP, 0, 1.0f, 0, H_DIGITAL, C_SHORT, T_SYNTH, P_GLITCH, FX_P_CLEAN, 0.1f, 0.5f, 0.2f },
    { "Chalk Dust", K_TIGHT, 60, S_TIGHT, -2, 1.2f, -5, H_DUSTY, C_MID, T_TIGHT, P_ORGANIC, FX_P_LOFI, 0.1f, 0.4f, 0.f },
    { "Amber Room", K_ROUND, 55, S_ROOM, 0, 1.0f, 0, H_CRISP, C_BRIGHT, T_MID, P_ORGANIC, FX_P_BOUNCE, 0.25f, 0.6f, 0.f },
    { "Cold Front", K_CLICKY, 60, S_CRACK, 2, 0.9f, 5, H_BRIGHT, C_SHORT, T_SYNTH, P_TECHY, FX_P_GRIND, 0.0f, 0.5f, 0.f },
    { "Salt Flats", K_LONG, 46, S_TONAL, 0, 1.0f, 0, H_TIGHT, C_SHORT, T_TIGHT, P_RETRO, FX_P_WARM, 0.05f, 0.5f, 0.1f },
    { "Neon Moss", K_MID, 56, S_METAL, 0, 1.0f, 0, H_METALLIC, C_MID, T_SYNTH, P_TECHY, FX_P_POLISH, 0.15f, 0.6f, 0.1f },
    { "Brick Garden", K_PUNCHY, 58, S_FAT, 0, 1.0f, 0, H_CRISP, C_MID, T_MID, P_ORGANIC, FX_P_BREAK, 0.1f, 0.5f, 0.f },
    { "Tin Halo", K_TIGHT, 62, S_BITE, 2, 1.0f, 0, H_TIGHT, C_SHORT, T_TIGHT, P_AFRO, FX_P_BOUNCE, 0.1f, 0.4f, 0.1f },
    { "Deep Shelf", K_SUB, 48, S_FAT, -1, 1.1f, -10, H_DARK, C_DARK, T_DEEP, P_DEEP, FX_P_WEIGHT, 0.2f, 0.7f, 0.f },
    { "Ember Grid", K_DRIVEN, 52, S_GRIT, 1, 1.0f, 5, H_DIGITAL, C_MID, T_SYNTH, P_GLITCH, FX_P_SMASH, 0.05f, 0.5f, 0.f },
    { "Frost Line", K_CLICKY, 60, S_ZAP, 2, 0.9f, 5, H_BRIGHT, C_SHORT, T_SYNTH, P_TECHY, FX_P_CLEAN, 0.1f, 0.5f, 0.15f },
    { "Rubber Spine", K_BOOMY, 52, S_TONAL, 1, 1.0f, 0, H_CRISP, C_MID, T_MID, P_PARTY, FX_P_BOUNCE, 0.1f, 0.5f, 0.f },
    { "Ghost Ledger", K_SOFT, 52, S_SOFT, -1, 1.2f, -10, H_DUSTY, C_DARK, T_DEEP, P_DEEP, FX_P_DARK, 0.3f, 0.8f, 0.f },
    { "Black Opal", K_HARD, 48, S_ROOM, 0, 1.1f, 0, H_METALLIC, C_BIG, T_BIG, P_BIG, FX_P_BLAST, 0.25f, 0.7f, 0.f },
    { "Silk Engine", K_ROUND, 56, S_CRACK, -1, 1.1f, -5, H_WASHY, C_MID, T_MID, P_PARTY, FX_P_POLISH, 0.15f, 0.6f, 0.f },
    { "Gravel Choir", K_DRIVEN, 50, S_DUST, 1, 1.0f, 0, H_DUSTY, C_MID, T_SYNTH, P_GLITCH, FX_P_SMASH, 0.1f, 0.5f, 0.f },
    { "Rust Belt", K_HARD, 50, S_GRIT, -2, 1.1f, -5, H_METALLIC, C_DARK, T_BIG, P_TECHY, FX_P_SMASH, 0.1f, 0.6f, 0.f },
    { "Quartz Pulse", K_TIGHT, 58, S_METAL, 2, 0.9f, 5, H_DIGITAL, C_SHORT, T_SYNTH, P_GLITCH, FX_P_CLEAN, 0.05f, 0.4f, 0.2f },
    { "Cinder Walk", K_PUNCHY, 56, S_FAT, 1, 0.9f, 5, H_CRISP, C_MID, T_MID, P_ORGANIC, FX_P_BREAK, 0.1f, 0.5f, 0.f },
    { "Hollow Oak", K_SOFT, 54, S_WASH, -1, 1.0f, -10, H_DUSTY, C_DARK, T_DEEP, P_ORGANIC, FX_P_LOFI, 0.2f, 0.6f, 0.f },
    { "Static Bloom", K_MID, 54, S_ZAP, -1, 1.0f, 0, H_METALLIC, C_MID, T_SYNTH, P_TECHY, FX_P_GRIND, 0.1f, 0.5f, 0.1f },
    { "Lunar Dock", K_DEEP, 45, S_TONAL, -2, 1.2f, -5, H_WASHY, C_DARK, T_DEEP, P_DEEP, FX_P_DARK, 0.3f, 0.85f, 0.1f },
    { "Wire Garden", K_TIGHT, 60, S_BITE, 0, 1.0f, 0, H_TIGHT, C_SHORT, T_TIGHT, P_RETRO, FX_P_CLEAN, 0.05f, 0.4f, 0.f },
    { "Stone Circuit", K_DRIVEN, 52, S_CRACK, 0, 1.0f, 0, H_METALLIC, C_MID, T_SYNTH, P_TECHY, FX_P_RUMBLE, 0.1f, 0.6f, 0.f },
    { "Coral Drift", K_ROUND, 54, S_SOFT, 1, 1.0f, 5, H_WASHY, C_BRIGHT, T_MID, P_AFRO, FX_P_POLISH, 0.2f, 0.6f, 0.15f },
    { "Smoke Signal", K_SUB, 48, S_ROOM, -1, 1.2f, -10, H_DARK, C_DARK, T_DEEP, P_DEEP, FX_P_WEIGHT, 0.25f, 0.8f, 0.f },
    { "Thunder Crate", K_HARD, 50, S_FAT, 0, 1.0f, 0, H_CRISP, C_BIG, T_BIG, P_BIG, FX_P_BLAST, 0.2f, 0.7f, 0.f },
    { "Marble Hall", K_ROUND, 56, S_ROOM, 1, 1.0f, 5, H_CRISP, C_BIG, T_BIG, P_BIG, FX_P_GATED, 0.0f, 0.6f, 0.f },
    { "Dust Devil", K_CLICKY, 60, S_DUST, 1, 0.9f, 0, H_DUSTY, C_SHORT, T_TIGHT, P_ORGANIC, FX_P_BREAK, 0.1f, 0.5f, 0.f },
    { "Tidal Lock", K_LONG, 44, S_SOFT, 0, 1.0f, 0, H_WASHY, C_MID, T_DEEP, P_PARTY, FX_P_WARM, 0.25f, 0.7f, 0.1f },
    { "Phantom Gear", K_DRIVEN, 54, S_METAL, -1, 1.0f, 0, H_METALLIC, C_DARK, T_SYNTH, P_TECHY, FX_P_RUMBLE, 0.15f, 0.7f, 0.1f },
    { "Granite Step", K_PUNCHY, 56, S_TIGHT, 1, 1.0f, 0, H_CRISP, C_MID, T_MID, P_RETRO, FX_P_BOUNCE, 0.1f, 0.5f, 0.f },
    { "Lantern Fog", K_DEEP, 46, S_WASH, -2, 1.2f, -10, H_DARK, C_DARK, T_DEEP, P_DEEP, FX_P_DARK, 0.3f, 0.85f, 0.1f },
    { "Echo Quarry", K_MID, 54, S_CRACK, 0, 1.1f, 0, H_CRISP, C_MID, T_MID, P_ORGANIC, FX_P_CLEAN, 0.15f, 0.6f, 0.2f },
    { "Magnet Field", K_HARD, 50, S_ZAP, 0, 1.0f, 0, H_DIGITAL, C_SHORT, T_SYNTH, P_GLITCH, FX_P_HARD, 0.05f, 0.5f, 0.f },
    { "Velour Heat", K_ROUND, 56, S_FAT, 2, 0.9f, 5, H_WASHY, C_MID, T_MID, P_AFRO, FX_P_BOUNCE, 0.1f, 0.5f, 0.f },
    { "Carbon Bloom", K_DRIVEN, 52, S_GRIT, 0, 1.0f, 0, H_METALLIC, C_MID, T_BIG, P_BIG, FX_P_HARD, 0.15f, 0.6f, 0.f },
    { "Rattle Box", K_TIGHT, 60, S_WASH, 2, 0.9f, 10, H_TIGHT, C_SHORT, T_TIGHT, P_AFRO, FX_P_CLEAN, 0.1f, 0.4f, 0.1f },
    { "Sugar Glass", K_MID, 56, S_TONAL, 3, 0.9f, 10, H_BRIGHT, C_BRIGHT, T_SYNTH, P_RETRO, FX_P_POLISH, 0.15f, 0.6f, 0.1f },
    { "Midnight Oil", K_SUB, 48, S_DUST, -2, 1.1f, -10, H_DUSTY, C_DARK, T_DEEP, P_ORGANIC, FX_P_LOFI, 0.2f, 0.6f, 0.f },
    { "Pressure Cell", K_HARD, 52, S_BITE, 1, 1.0f, 0, H_METALLIC, C_SHORT, T_SYNTH, P_TECHY, FX_P_HARD, 0.05f, 0.5f, 0.f },
    { "Orbit Swing", K_PUNCHY, 57, S_ZAP, 1, 0.9f, 0, H_CRISP, C_BRIGHT, T_MID, P_PARTY, FX_P_BOUNCE, 0.1f, 0.5f, 0.15f },
    { "Pixel Rain", K_TIGHT, 58, S_METAL, 3, 0.8f, 10, H_DIGITAL, C_SHORT, T_SYNTH, P_GLITCH, FX_P_LOFI, 0.1f, 0.4f, 0.2f },
    { "Shadow Press", K_DEEP, 46, S_GRIT, -2, 1.1f, -10, H_DARK, C_DARK, T_BIG, P_DEEP, FX_P_WEIGHT, 0.2f, 0.7f, 0.f },
    { "Clay Court", K_ROUND, 58, S_CRACK, 1, 0.9f, 0, H_CRISP, C_MID, T_TIGHT, P_ORGANIC, FX_P_BOUNCE, 0.1f, 0.5f, 0.f },
    { "Hot Wire", K_CLICKY, 62, S_TIGHT, 3, 0.8f, 5, H_BRIGHT, C_SHORT, T_TIGHT, P_TECHY, FX_P_GRIND, 0.0f, 0.5f, 0.f },
    { "Bamboo Ghost", K_SOFT, 54, S_METAL, 1, 1.0f, 0, H_WASHY, C_MID, T_MID, P_AFRO, FX_P_CLEAN, 0.2f, 0.6f, 0.15f },
    { "Canyon Echo", K_BOOMY, 52, S_ROOM, 0, 1.2f, 0, H_CRISP, C_BIG, T_BIG, P_BIG, FX_P_GATED, 0.0f, 0.6f, 0.1f },
    { "Fuse Box", K_DRIVEN, 54, S_ZAP, 0, 1.0f, 0, H_METALLIC, C_MID, T_SYNTH, P_GLITCH, FX_P_SMASH, 0.05f, 0.5f, 0.1f },
    { "Satin Crush", K_ROUND, 55, S_SOFT, 0, 1.0f, 0, H_CRISP, C_BRIGHT, T_MID, P_PARTY, FX_P_POLISH, 0.15f, 0.6f, 0.f },
    { "Polar Static", K_LONG, 47, S_BITE, 2, 1.0f, 0, H_TIGHT, C_SHORT, T_TIGHT, P_RETRO, FX_P_WARM, 0.05f, 0.4f, 0.f },
    { "Rocket Dust", K_HARD, 50, S_CRACK, 1, 1.0f, 5, H_BRIGHT, C_BIG, T_BIG, P_BIG, FX_P_BLAST, 0.2f, 0.7f, 0.f },
    { "Tape Spirit", K_SOFT, 56, S_DUST, 0, 1.0f, 0, H_DUSTY, C_DARK, T_DEEP, P_ORGANIC, FX_P_LOFI, 0.15f, 0.5f, 0.f },
    { "Molten Core", K_HARD, 48, S_FAT, -1, 1.0f, 0, H_METALLIC, C_DARK, T_BIG, P_TECHY, FX_P_HARD, 0.15f, 0.6f, 0.f },
    { "Feather Weight", K_TIGHT, 60, S_SOFT, 2, 0.8f, 5, H_WASHY, C_SHORT, T_TIGHT, P_PARTY, FX_P_CLEAN, 0.1f, 0.4f, 0.1f },
    { "Cobalt Drive", K_BOOMY, 52, S_GRIT, 1, 1.0f, 0, H_CRISP, C_MID, T_SYNTH, P_TECHY, FX_P_RUMBLE, 0.1f, 0.6f, 0.f },
};

int numFactoryKits() { return (int) (sizeof (kKits) / sizeof (kKits[0])); }

static void buildPerc (Kit& k, int flavour) {
    auto& MM = k.parts[Membrane]; auto& MT = k.parts[Metallic]; auto& SX = k.parts[Sfx];
    switch (flavour) {
        case P_ORGANIC: // conga, cowbell, vinyl crackle drop
            S (MM, {{0,3},{1,8},{5,350},{8,70},{9,40},{10,30},{11,60},{14,15}}); MM.noiseType = 0;
            S (MT, {{0,0},{5,400},{8,5},{9,35},{10,20},{11,70},{12,50},{14,20}}); MT.wave = 0; MT.noiseType = 0; // cowbell
            S (SX, {{0,12},{1,24},{2,300},{4,1},{5,600},{6,-60},{8,10},{9,60},{10,70},{11,20},{14,20}}); SX.wave = 0; SX.noiseType = 5; // vinyl crackle drop
            break;
        case P_TECHY:   // FM tom, struck plate, FM zap
            S (MM, {{0,-5},{1,24},{2,40},{5,250},{8,60},{9,55},{10,15},{11,70},{12,40},{14,70}}); MM.wave = 4; MM.noiseType = 4;
            S (MT, {{0,-5},{5,700},{8,70},{9,70},{10,50},{11,85},{12,30},{13,30}}); MT.wave = 0; MT.noiseType = 0; // struck plate
            S (SX, {{0,12},{1,60},{2,120},{3,20},{4,0.1f},{5,250},{6,-60},{8,70},{9,0},{10,90},{11,40},{14,40},{19,1.41f}}); SX.wave = 0; SX.noiseType = 0; // FM zap
            break;
        case P_PARTY:   // bongo, glock ping, noise riser
            S (MM, {{0,5},{1,12},{5,250},{8,75},{9,50},{10,25},{11,65},{14,10}}); MM.noiseType = 0;
            S (MT, {{0,7},{5,900},{8,30},{9,30},{10,10},{11,55},{12,50}}); MT.wave = 0; MT.noiseType = 0; // glock ping
            S (SX, {{0,0},{1,-24},{2,1500},{3,80},{4,1200},{5,400},{6,-60},{8,0},{9,90},{10,40},{11,-70},{14,50}}); SX.wave = 0; SX.noiseType = 0; // noise riser
            break;
        case P_DEEP:    // low membrane, dark gong, laser drop
            S (MM, {{0,-7},{1,16},{5,600},{8,70},{9,30},{10,25},{11,35},{14,30}}); MM.noiseType = 6;
            S (MT, {{0,-12},{5,3500},{8,100},{9,50},{10,70},{11,30},{12,60}}); MT.wave = 0; MT.noiseType = 1; // dark gong
            S (SX, {{0,0},{1,48},{2,1500},{4,1},{5,1800},{6,-60},{8,30},{9,0},{10,60},{11,40},{14,60},{19,0.5f}}); SX.wave = 2; SX.noiseType = 0; // laser drop
            break;
        case P_BIG:     // big membrane, anvil, impact down-sweep
            S (MM, {{0,-2},{1,20},{5,400},{8,70},{9,35},{10,30},{11,60},{12,35},{14,30}}); MM.noiseType = 0;
            S (MT, {{0,-3},{5,1200},{8,40},{9,60},{10,60},{11,90},{12,30},{13,25}}); MT.wave = 0; MT.noiseType = 0; // anvil
            S (SX, {{0,24},{1,60},{2,600},{3,30},{4,0.5f},{5,900},{6,-60},{8,0},{9,60},{10,85},{11,30},{12,40}}); SX.wave = 2; SX.noiseType = 2; // impact down-sweep
            break;
        case P_AFRO:    // log drum (pitched FM membrane), agogo, whistle trill
            S (MM, {{0,-9},{1,10},{2,60},{5,450},{8,80},{9,25},{10,10},{11,40},{12,20},{14,45}}); MM.wave = 4; MM.noiseType = 0;
            S (MT, {{0,5},{5,500},{8,8},{9,30},{10,15},{11,75},{12,40},{14,25}}); MT.wave = 0; MT.noiseType = 0; // agogo bell
            S (SX, {{0,24},{1,2},{2,50},{4,5},{5,400},{6,-60},{8,0},{9,10},{10,90},{11,0},{16,14},{17,1.5f}}); SX.wave = 0; SX.noiseType = 0; // whistle trill
            break;
        case P_RETRO:   // clave, two-osc cowbell, arcade zap
            S (MM, {{0,14},{1,4},{2,10},{5,90},{8,40},{9,70},{10,10},{11,70},{14,0}}); MM.noiseType = 0;
            S (MT, {{0,-2},{5,350},{8,0},{9,30},{10,5},{11,60},{12,60},{14,85}}); MT.wave = 3; MT.noiseType = 0; // classic analog-style cowbell
            S (SX, {{0,12},{1,36},{2,150},{4,0.1f},{5,250},{6,-60},{8,0},{9,0},{10,85},{11,10},{13,30},{14,10}}); SX.wave = 3; SX.noiseType = 0; // arcade zap
            break;
        default:        // P_GLITCH: bit perc, digital ring, wobble burst
            S (MM, {{0,0},{1,36},{2,15},{5,120},{8,50},{9,70},{10,50},{11,80},{12,60},{14,80}}); MM.wave = 3; MM.noiseType = 4;
            S (MT, {{0,5},{5,250},{8,85},{9,100},{10,80},{11,100},{12,20},{13,50},{14,30}}); MT.wave = 4; MT.noiseType = 4; // digital ring
            S (SX, {{0,0},{1,-12},{2,200},{4,0.1f},{5,300},{6,-60},{8,80},{9,40},{10,80},{12,40},{13,60},{16,22},{17,7},{18,50},{19,3.5f}}); SX.wave = 3; SX.noiseType = 4; // digital wobble burst
            break;
    }
}

static void buildFx (Kit& k, int pack) {
    auto& F = k.partFx;
    // reverb lives on the shared send bus (rows set the amount); packs carry
    // the insert processing. Reverb/delay inserts are fully wet at MIX 1.
    switch (pack) {
        case FX_P_CLEAN:
            addFx (k.globalFx, FX_COMP, { 0.62f, 0.14f, 0.3f, 0.35f, 0.4f, 0.6f });
            break;
        case FX_P_BREAK:   // glue + sampled crunch
            addFx (k.globalFx, FX_COMP, { 0.62f, 0.16f, 0.25f, 0.35f, 0.5f, 0.7f });
            addFx (k.globalFx, FX_DIST, { 0.06f, 0.6f, 0.f, 0.5f, 0.f, 0.3f });
            break;
        case FX_P_GRIND:   // clipped kick, fuzzed + squashed snare
            addFx (F[Kick], FX_DIST, { 0.15f, 0.45f, 0.15f, 0.42f, 0.f, 0.5f });
            addFx (F[Kick], FX_EQ, { 0.6f, 0.37f, 0.57f, 0.5f, 0.5f, 1.f });
            addFx (F[Snare], FX_DIST, { 0.2f, 0.5f, 0.3f, 0.4f, 0.f, 0.5f });
            addFx (F[Snare], FX_COMP, { 0.45f, 0.3f, 0.12f, 0.2f, 0.6f, 0.8f });
            addFx (F[Metallic], FX_FILTER, { 0.75f, 0.45f, 0.67f, 0.f, 0.1f, 1.f });
            addFx (k.globalFx, FX_COMP, { 0.6f, 0.2f, 0.2f, 0.3f, 0.4f, 0.8f });
            break;
        case FX_P_WEIGHT:  // heavy, compressed snare
            addFx (F[Kick], FX_TRANSIENT, { 0.7f, 0.45f, 0.3f, 0.5f, 0.5f, 1.f });
            addFx (F[Snare], FX_COMP, { 0.5f, 0.25f, 0.15f, 0.3f, 0.55f, 1.f });
            addFx (k.globalFx, FX_COMP, { 0.65f, 0.14f, 0.3f, 0.35f, 0.4f, 0.6f });
            break;
        case FX_P_BLAST:   // driven kick, saturated + compressed snare
            addFx (F[Kick], FX_DIST, { 0.12f, 0.55f, 0.f, 0.45f, 0.f, 0.5f });
            addFx (F[Kick], FX_TRANSIENT, { 0.7f, 0.5f, 0.3f, 0.5f, 0.5f, 1.f });
            addFx (F[Snare], FX_DIST, { 0.15f, 0.55f, 0.55f, 0.45f, 0.f, 0.4f });
            addFx (F[Snare], FX_COMP, { 0.45f, 0.3f, 0.15f, 0.25f, 0.6f, 0.9f });
            addFx (k.globalFx, FX_COMP, { 0.6f, 0.2f, 0.25f, 0.3f, 0.4f, 0.75f });
            break;
        case FX_P_DARK:
            addFx (F[Snare], FX_EQ, { 0.55f, 0.45f, 0.42f, 0.35f, 0.4f, 1.f });
            addFx (k.globalFx, FX_EQ, { 0.55f, 0.5f, 0.45f, 0.3f, 0.45f, 1.f });
            addFx (k.globalFx, FX_COMP, { 0.65f, 0.12f, 0.35f, 0.4f, 0.3f, 0.5f });
            break;
        case FX_P_LOFI:
            addFx (k.globalFx, FX_EQ, { 0.55f, 0.52f, 0.42f, 0.25f, 0.4f, 1.f });   // dull top
            addFx (k.globalFx, FX_DIST, { 0.1f, 0.3f, 0.69f, 0.5f, 0.f, 0.25f });   // bitcrush blend
            addFx (k.globalFx, FX_COMP, { 0.6f, 0.2f, 0.35f, 0.4f, 0.4f, 0.7f });
            break;
        case FX_P_GATED:   // big snare room, chopped by a fast-release comp
            addFx (F[Snare], FX_REVERB, { 0.55f, 0.8f, 0.45f, 0.1f, 0.6f, 0.3f });
            addFx (F[Snare], FX_COMP, { 0.5f, 0.3f, 0.15f, 0.12f, 0.5f, 1.f });
            for (int t : { (int) Tom1, (int) Tom2, (int) LowTom })
                addFx (F[t], FX_REVERB, { 0.5f, 0.7f, 0.5f, 0.1f, 0.5f, 0.2f });
            addFx (k.globalFx, FX_COMP, { 0.62f, 0.16f, 0.3f, 0.35f, 0.4f, 0.65f });
            break;
        case FX_P_SMASH:   // fuzz kick, folded snare, crushed hats, hard bus
            addFx (F[Kick], FX_DIST, { 0.25f, 0.45f, 0.3f, 0.4f, 0.f, 0.6f });
            addFx (F[Snare], FX_DIST, { 0.2f, 0.5f, 0.4f, 0.4f, 0.f, 0.5f });
            addFx (F[ClosedHat], FX_DIST, { 0.15f, 0.6f, 0.69f, 0.45f, 0.f, 0.4f });
            addFx (k.globalFx, FX_COMP, { 0.55f, 0.3f, 0.15f, 0.25f, 0.5f, 0.8f });
            break;
        case FX_P_HARD:    // hard-clipped, punch-shaped kick
            addFx (F[Kick], FX_DIST, { 0.35f, 0.5f, 0.15f, 0.38f, 0.f, 0.7f });
            addFx (F[Kick], FX_EQ, { 0.6f, 0.35f, 0.6f, 0.5f, 0.5f, 1.f });
            addFx (F[Kick], FX_TRANSIENT, { 0.7f, 0.55f, 0.3f, 0.5f, 0.5f, 1.f });
            addFx (k.globalFx, FX_COMP, { 0.6f, 0.2f, 0.25f, 0.3f, 0.4f, 0.75f });
            break;
        case FX_P_BOUNCE:
            addFx (F[Kick], FX_TRANSIENT, { 0.65f, 0.5f, 0.35f, 0.5f, 0.5f, 1.f });
            addFx (k.globalFx, FX_COMP, { 0.62f, 0.16f, 0.3f, 0.3f, 0.45f, 0.7f });
            break;
        case FX_P_WARM:    // warm grit on a long kick
            addFx (F[Kick], FX_DIST, { 0.1f, 0.5f, 0.f, 0.47f, 0.f, 0.3f });
            addFx (k.globalFx, FX_COMP, { 0.62f, 0.14f, 0.3f, 0.35f, 0.4f, 0.6f });
            break;
        case FX_P_RUMBLE:  // kick into a dark short room, dotted hat echo
            addFx (F[Kick], FX_DIST, { 0.1f, 0.4f, 0.f, 0.47f, 0.f, 0.4f });
            addFx (F[Kick], FX_REVERB, { 0.45f, 0.4f, 0.85f, 0.f, 0.15f, 0.1f });
            addFx (F[ClosedHat], FX_DELAY, { 0.3f, 0.25f, 0.4f, 0.3f, 0.2f, 0.1f });
            addFx (k.globalFx, FX_COMP, { 0.6f, 0.2f, 0.3f, 0.3f, 0.4f, 0.7f });
            break;
        default: // FX_P_POLISH
            addFx (F[Snare], FX_COMP, { 0.55f, 0.2f, 0.2f, 0.3f, 0.5f, 0.8f });
            addFx (k.globalFx, FX_COMP, { 0.62f, 0.16f, 0.3f, 0.35f, 0.45f, 0.7f });
            break;
    }
}

void mintFactoryKit (int i, Kit& k) {
    for (int p = 0; p < NumParts; ++p) {
        defaultParamsForPart (p, k.parts[p]);
        FxProcessor::clearChain (k.partFx[p]);
        k.level[p] = 0.7f; k.pan[p] = 0.5f; k.mute[p] = false;
        if (isMembranePart (p)) k.parts[p].p[18] = 0.f; // BOOST off unless a kit wants it
    }
    FxProcessor::clearChain (k.globalFx);
    const KitDef& d = kKits[juce::jlimit (0, numFactoryKits() - 1, i)];
    k.name = d.name; k.genre = {};

    auto& K  = k.parts[Kick],    &SN = k.parts[Snare];
    auto& CH = k.parts[ClosedHat], &OH = k.parts[OpenHat];
    auto& RD = k.parts[Ride],    &CR = k.parts[Crash];

    // kick: base note is 98 Hz, TUNE in semitones
    const KickDef& kd = kKick[d.kick];
    S (K, {{0,12.f * std::log2 (d.kHz / 98.f)},{1,kd.sweep},{2,kd.sweepMs},{3,25},{4,0.2f},{5,kd.dec},{7,80},
           {8,kd.sub},{9,0},{10,kd.click},{11,5},{12,kd.drive},{13,15},{14,15},{15,15},{16,kd.punch}});

    const SnareDef& sd = kSnare[d.snare];
    S (SN, {{0,sd.tune + d.sTune},{1,sd.sweep},{2,sd.sweepMs},{3,sd.curve},{4,sd.atk},{5,sd.dec * d.sDecMul},
            {8,sd.body},{9,sd.noise},{10,sd.nDec * d.sDecMul},{11,sd.tone + d.sTone},{12,sd.drive},{13,sd.comp},
            {14,sd.nzHp},{16,sd.punch},{19,0}});
    SN.wave = sd.wave; SN.noiseType = sd.noiseType;
    if (sd.transient) addFx (k.partFx[Snare], FX_TRANSIENT, { 0.72f, 0.55f, 0.35f, 0.5f, 0.5f, 1.f });

    const HatDef& hd = kHat[d.hat];
    S (CH, {{0,hd.tune},{1,10},{5,hd.chDec},{8,65},{9,65},{10,hd.tone},{11,hd.bright},{13,hd.strike},{14,80},{15,70}});
    S (OH, {{0,hd.tune},{1,10},{5,hd.ohDec},{8,65},{9,62},{10,hd.tone},{11,hd.bright - 5},{13,hd.strike},{14,80}});
    CH.noiseType = OH.noiseType = hd.nz;
    const CymDef& cd = kCym[d.cym];
    S (RD, {{0,cd.tune},{5,cd.ride},{8,75},{9,45},{10,70},{11,cd.damp},{13,50},{14,50}});
    S (CR, {{0,cd.tune},{5,cd.crash},{8,75},{9,58},{10,72},{11,cd.damp * 0.8f},{13,50},{14,50}});
    const TomDef& td = kTom[d.tom];
    float tt[3] = { td.tune + 4.f, td.tune, td.tune - 1.f }; // base notes A3/E3/A2: LOW TOM already sits 7 st under TOM 2
    int tomPart[3] = { Tom1, Tom2, LowTom };
    for (int t = 0; t < 3; ++t)
        S (k.parts[tomPart[t]], {{0,tt[t]},{1,td.sweep},{2,140.f + 30.f * t},{5,td.dec + 60.f * t},{8,40.f + 10.f * t},
                                  {9,35},{10,20},{11,50},{12,60},{13,35},{16,60}});
    buildPerc (k, d.perc);
    buildFx (k, d.fx);

    // shared returns: the snare leads the room, toms/perc follow at a lower level
    k.revDecay = d.revDecay; k.revDamp = 0.5f; k.dlyTime = 0.42f; k.dlyFb = 0.35f;
    k.sendRev[Snare] = d.rev;
    for (int p : { (int) Tom1, (int) Tom2, (int) LowTom, (int) Membrane }) k.sendRev[p] = d.rev * 0.5f;
    k.sendRev[ClosedHat] = k.sendRev[OpenHat] = d.rev * 0.25f;
    k.sendDly[Metallic] = k.sendDly[Sfx] = k.sendDly[Membrane] = d.dly;

    k.level[Kick] = 1.0f; k.level[Snare] = 0.9f;
    k.level[ClosedHat] = hd.lvl; k.level[OpenHat] = hd.lvl * 0.9f;
    k.level[Ride] = 0.45f; k.level[Crash] = 0.5f;
    k.level[Tom1] = k.level[Tom2] = 0.65f; k.level[LowTom] = 0.7f;
    k.level[Membrane] = 0.6f; k.level[Metallic] = 0.5f; k.level[Sfx] = 0.45f;
    k.pan[ClosedHat] = 0.42f; k.pan[OpenHat] = 0.42f; k.pan[Ride] = 0.62f; k.pan[Crash] = 0.56f; // panning works now
    k.pan[Tom1] = 0.38f; k.pan[Tom2] = 0.5f; k.pan[LowTom] = 0.62f;
    k.pan[Membrane] = 0.6f; k.pan[Metallic] = 0.4f;
    k.master = 0.7f;

    // keep every slot inside its meta range (rows combine hand-typed tables)
    for (int p = 0; p < NumParts; ++p)
        for (int s = 0; s < 20; ++s)
            if (auto* m = slotMeta (p, s))
                k.parts[p].p[s] = juce::jlimit (m->min, m->max, k.parts[p].p[s]);
}

// ---------------- file-backed factory + user presets ----------------
// Filenames: "NNN Name [Genre].xml" (factory) or "Name.xml" (user).

static juce::File customFactoryDir, customFactoryOverrideDir, customUserDir;
static bool useCustomFactoryDir = false, useCustomFactoryOverrideDir = false, useCustomUserDir = false;

void setFactoryDirsForTest (const juce::File& bundleDir, const juce::File& overrideDir) {
    customFactoryDir = bundleDir; useCustomFactoryDir = true;
    customFactoryOverrideDir = overrideDir; useCustomFactoryOverrideDir = true;
}
void setUserPresetDirForTest (const juce::File& dir) { customUserDir = dir; useCustomUserDir = true; }

juce::File getUserPresetDir() {
    if (useCustomUserDir) return customUserDir;
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Pastel Drum Synth Presets");
}

static juce::File factoryBundleDir() {
    if (useCustomFactoryDir) return customFactoryDir;
    // VST3 bundle layout: <bundle>/Contents/<arch>/Pastel Drum Synth.so -> ../Resources/Factory
    auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto res = exe.getParentDirectory().getParentDirectory().getChildFile ("Resources/Factory");
    if (res.isDirectory()) return res;
    // standalone: Factory next to the executable
    auto sib = exe.getParentDirectory().getChildFile ("Factory");
    if (sib.isDirectory()) return sib;
    // installed shared data
    auto share = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile (".local/share/PastelDrumSynth/Factory");
    if (share.isDirectory()) return share;
    // dev fallbacks (repo / build tree)
    const char* cands[] = { "./Factory", "../Factory", "../../Factory",
                            "../../../Factory", "../../../../Factory" };
    for (auto c : cands) {
        juce::File d (c);
        if (d.isDirectory() && d.findChildFiles (juce::File::findFiles, false, "*.xml").size() > 0)
            return d;
    }
    return juce::File();
}

static juce::File factoryOverrideDir() {
    if (useCustomFactoryOverrideDir) return customFactoryOverrideDir;
    return getUserPresetDir().getChildFile ("Factory");
}

// "NNN Name [Genre].xml" (genre tag optional, any style name).
static bool parseFactoryFileName (const juce::String& fn, int& indexOut, juce::String& nameOut,
                                  juce::String& genreOut) {
    if (! fn.endsWithIgnoreCase (".xml") || fn.length() < 8) return false;
    if (! juce::CharacterFunctions::isDigit (fn[0]) || ! juce::CharacterFunctions::isDigit (fn[1])
        || ! juce::CharacterFunctions::isDigit (fn[2]) || fn[3] != ' ') return false;
    indexOut = fn.substring (0, 3).getIntValue();
    juce::String rest = fn.substring (4, fn.length() - 4);
    genreOut = {};
    int ob = rest.lastIndexOfChar ('[');
    if (ob > 0 && rest.endsWithChar (']')) {
        genreOut = rest.substring (ob + 1, rest.length() - 1).trim();
        rest = rest.substring (0, ob).trim();
    }
    nameOut = rest;
    return indexOut >= 0 && indexOut < 1000 && nameOut.isNotEmpty();
}

static std::vector<FactoryPreset> factory;
static bool factoryScanned = false;

void rescanFactoryPresets() {
    factory.clear();
    std::map<int, FactoryPreset> byIndex;
    auto collect = [&] (const juce::File& dir, bool shadowed) {
        if (! dir.isDirectory()) return;
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml")) {
            int idx; juce::String nm, genre;
            if (! parseFactoryFileName (f.getFileName(), idx, nm, genre)) continue;
            byIndex[idx] = { nm, genre, f, idx, shadowed };
        }
    };
    collect (factoryBundleDir(), false);
    collect (factoryOverrideDir(), true); // user overwrites shadow shipped files
    for (auto& kv : byIndex) factory.push_back (kv.second);
    factoryScanned = true;
}

const std::vector<FactoryPreset>& getFactoryPresets() {
    if (! factoryScanned) rescanFactoryPresets();
    return factory;
}

juce::String presetGenreOf (int index) {
    if (index < 0) return "User";
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) return pr.genre;
    return "Init";
}

bool deleteFactoryShadow (int index) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == index && pr.shadowed) {
            bool ok = pr.file.deleteFile();
            rescanFactoryPresets();
            return ok;
        }
    return false;
}

std::vector<UserPreset> scanUserPresets() {
    std::vector<UserPreset> out;
    auto dir = getUserPresetDir();
    if (! dir.isDirectory()) return out;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.xml"))
        out.push_back ({ f.getFileNameWithoutExtension(), f });
    std::sort (out.begin(), out.end(),
               [] (const UserPreset& a, const UserPreset& b)
               { return a.name.compareIgnoreCase (b.name) < 0; });
    return out;
}

juce::String sanitisePresetName (juce::String name) {
    name = name.trim().substring (0, 64);
    name = name.retainCharacters ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-");
    name = name.trim();
    return name.isEmpty() ? juce::String ("Untitled") : name;
}

juce::String presetName (int index) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) return pr.name;
    return "Init";
}

bool deleteUserPreset (const juce::File& file) {
    if (file.getParentDirectory() != getUserPresetDir()) return false; // safety: own dir only
    return file.existsAsFile() && file.deleteFile();
}

} // namespace mds
