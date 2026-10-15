// Model probe (dev only, never shipped): renders every engine model of every part at its default
// settings (and each part's CLASSIC default for level reference), prints peak / length / RMS and
// writes WAVs for listening + comparison with the kit generator.
//   ModelProbe <outDir>
#include "dsp/DrumEngine.h"
#include <cstdio>

using namespace mds;

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out (argc > 1 ? argv[1] : "probe_models");
    out.createDirectory();
    const float sr = 44100.f;
    juce::WavAudioFormat wav;
    if (argc > 3) {   // ModelProbe <out> <part> <model> [knobIndex=cookedValue ...] -> <out>/custom.wav
        int part = juce::String (argv[2]).getIntValue(), model = juce::String (argv[3]).getIntValue();
        DrumVoice v; v.prepare (sr); v.noise.rng.setSeed (99);
        PartParams pp; defaultParamsForPart (part, pp);
        pp.model = model; modelDefaults01 (part, model, pp.m);
        if (auto* md = modelDef (part, model))
            for (int a = 4; a < argc; ++a) {
                auto kv = juce::String (argv[a]);
                int k = kv.upToFirstOccurrenceOf ("=", false, false).getIntValue();
                pp.m[k] = modelUncook (md->p[k], kv.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            }
        v.trigger (part, pp, 1.f, 0.f);
        std::vector<float> x;
        for (int i = 0; i < (int) (sr * 7) && v.active; ++i) x.push_back (v.renderWithParams (pp));
        float pk = 1e-6f; for (float q : x) pk = std::max (pk, std::abs (q));
        juce::AudioBuffer<float> b (1, (int) x.size());
        for (size_t i = 0; i < x.size(); ++i) b.setSample (0, (int) i, x[i] * 0.9f / pk);
        auto f = out.getChildFile ("custom.wav"); f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 24, {}, 0));
        if (w) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
        printf ("peak %.3f len %.0f ms\n", pk, x.size() / sr * 1000.f);
        return 0;
    }
    for (int part = 0; part < NumParts; ++part) {
        for (int model = 0; model < numModels (part); ++model) {
            DrumVoice v; v.prepare (sr); v.noise.rng.setSeed (99);
            PartParams pp; defaultParamsForPart (part, pp);
            pp.model = model; modelDefaults01 (part, model, pp.m);
            v.trigger (part, pp, 1.f, 0.f);
            std::vector<float> x;
            for (int i = 0; i < (int) (sr * 7) && v.active; ++i) x.push_back (v.renderWithParams (pp));
            float pk = 0.f; double e = 0; bool finite = true;
            for (float s : x) { pk = std::max (pk, std::abs (s)); e += (double) s * s; finite &= std::isfinite (s); }
            int last = (int) x.size(); while (last > 0 && std::abs (x[(size_t) last - 1]) < pk * 1e-3f) --last;
            auto name = modelNames (part)[model];
            printf ("%-9s %-13s peak %6.3f  len(-60dB) %6.0f ms  rms %7.4f %s\n", partName (part), name.toRawUTF8(), pk,
                    last / sr * 1000.f, std::sqrt (e / std::max<size_t> (1, x.size())), finite ? "" : "NON-FINITE");
            juce::AudioBuffer<float> b (1, (int) x.size());
            for (size_t i = 0; i < x.size(); ++i) b.setSample (0, (int) i, x[i] * (0.9f / std::max (pk, 1.0e-6f)));   // normalised: never clip the file
            auto f = out.getChildFile (juce::String (part).paddedLeft ('0', 2) + "_" + juce::String (partName (part)).replace (" ", "_")
                                       + "_" + juce::String (model) + "_" + name.replace (" ", "_") + ".wav");
            f.deleteFile();
            std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 1, 24, {}, 0));
            if (w) { os.release(); w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
        }
    }
    return 0;
}
