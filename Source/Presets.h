#pragma once
#include "dsp/DrumEngine.h"
#include "dsp/Effects.h"
#include <vector>

// ============================================================
// Presets: factory kits live as data files (never compiled code).
//   Factory/NNN Name [Genre].xml, shipped in the bundle, with
//   per-user overwrites shadowing them. User kits in Documents.
// File content = full kit state XML (synth + mixer + chains).
// ============================================================

namespace mds {

class DrumProcessor;

// ---- kit recipe: used ONLY to mint the factory XML data files ----
struct Kit {
    juce::String name, genre;
    PartParams parts[NumParts];
    float level[NumParts] = {};
    float pan[NumParts] = {};
    bool mute[NumParts] = {};
    float master = 0.8f;
    FxChainState partFx[NumParts];
    FxChainState globalFx;
    float sendRev[NumParts] = {}, sendDly[NumParts] = {};               // FX SEND levels
    float revDecay = 0.5f, revDamp = 0.5f, dlyTime = 0.35f, dlyFb = 0.35f; // shared returns
};

int numFactoryKits();
void mintFactoryKit (int i, Kit& out); // deterministic recipe (synth + FX)
juce::ValueTree kitToStateVT (const Kit& k, int index); // -> DRUMSYNTH tree for the file

// ---- factory presets: data files ----
struct FactoryPreset {
    juce::String name, genre;
    juce::File file;
    int index = -1;
    bool shadowed = false; // user-overwritten copy shadows the shipped file
};

const std::vector<FactoryPreset>& getFactoryPresets(); // sorted by index
void rescanFactoryPresets();
void setFactoryDirsForTest (const juce::File& bundleDir, const juce::File& overrideDir);
bool applyPreset (DrumProcessor& proc, int index); // factory by index
juce::String presetName (int index);
juce::String presetGenreOf (int index); // genre tag, or "User"/"Init"
bool overwriteFactoryPreset (int index, DrumProcessor& proc);
bool deleteFactoryShadow (int index); // restores the shipped file (true if one existed)

// ---- loading / saving kit state files (factory + user share the format) ----
bool loadPresetFile (DrumProcessor& proc, const juce::File& file); // no undo step (callers push)
bool savePresetFile (DrumProcessor& proc, const juce::File& file);

// ---- user presets (XML files on disk) ----
struct UserPreset { juce::String name; juce::File file; };
void setUserPresetDirForTest (const juce::File& dir); // harness override, else Documents
juce::File getUserPresetDir();
std::vector<UserPreset> scanUserPresets();
juce::String sanitisePresetName (juce::String name);
bool saveUserPreset (const juce::String& name, DrumProcessor& proc);
bool loadUserPreset (const juce::File& file, DrumProcessor& proc);
bool deleteUserPreset (const juce::File& file);

} // namespace mds
