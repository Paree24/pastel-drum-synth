// Regression renderer (dev only, never shipped): renders every part of every
// factory kit through the real processor (voices + part FX + sends + global FX +
// master), with all voice RNGs seeded, and prints one hash per render. Used to
// prove that new engine work leaves existing kits byte-identical.
//   RegressRender > hashes.txt      (then diff against a baseline)
#include "PluginProcessor.h"
#include <cstdio>

using namespace mds;

static juce::uint64 fnv (const float* d, int n, juce::uint64 h) {
    auto* b = reinterpret_cast<const unsigned char*> (d);
    for (size_t i = 0; i < (size_t) n * sizeof (float); ++i) { h ^= b[i]; h *= 1099511628211ull; }
    return h;
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0; const int bs = 256, len = (int) (sr * 2.5);
    int count = 0;
    for (auto& fk : getFactoryPresets()) {
        for (int part = 0; part < NumParts; ++part) {
            DrumProcessor proc;
            proc.prepareToPlay (sr, bs);
            proc.loadKit (fk.index);
            for (int p = 0; p < NumParts; ++p)
                for (int v = 0; v < DrumProcessor::MaxVoicesPerPart; ++v)
                    proc.parts[p].voices[v].noise.rng.setSeed (1000 + p * 10 + v);
            juce::AudioBuffer<float> buf (2, bs);
            juce::uint64 h = 1469598103934665603ull;
            for (int pos = 0; pos < len; pos += bs) {
                juce::MidiBuffer mb;
                if (pos == 0) mb.addEvent (juce::MidiMessage::noteOn (1, partMidiNote (part), (juce::uint8) 110), 0);
                buf.clear();
                proc.processBlock (buf, mb);
                h = fnv (buf.getReadPointer (0), bs, h);
                h = fnv (buf.getReadPointer (1), bs, h);
            }
            printf ("%03d %-16s %-9s %016llx\n", fk.index, fk.name.substring (0, 16).toRawUTF8(), partName (part), (unsigned long long) h);
            ++count;
        }
    }
    fprintf (stderr, "%d renders\n", count);
    return 0;
}
