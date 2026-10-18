#pragma once
#include <JuceHeader.h>

// ============================================================
// Shared tempo-sync helper: the note divisions of Pastel Chromatic
// Sampler's DSPEngine delay (same order, so saved indices mean the same
// thing in every Pastel plugin) and the host tempo from the playhead.
// ============================================================

namespace mds::TempoSync {

constexpr int NumDivisions = 6;

inline const char* divisionName (int i) {
    static const char* n[NumDivisions] = { "1/4", "1/8", "1/8 D", "1/4 T", "1/16", "1/2" };
    return n[juce::jlimit (0, NumDivisions - 1, i)];
}

inline double divisionBeats (int i) {   // in quarter notes
    static const double b[NumDivisions] = { 1.0, 0.5, 0.75, 2.0 / 3.0, 0.25, 2.0 };
    return b[juce::jlimit (0, NumDivisions - 1, i)];
}

inline double divisionSeconds (int i, double bpm) {
    return divisionBeats (i) * 60.0 / (bpm > 20.0 ? bpm : 120.0);
}

// audio thread: host tempo, or the fallback when the host gives none (standalone, offline)
inline double hostBpm (juce::AudioPlayHead* ph, double fallback = 120.0) {
    if (ph != nullptr)
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 20.0 && *bpm < 999.0) return *bpm;
    return fallback;
}

} // namespace mds::TempoSync
