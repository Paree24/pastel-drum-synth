// Kit probe (dev tool, never shipped): renders every factory recipe through
// the real processor (synth + part FX + global FX + limiter), prints per-part
// metrics and writes one-shot + groove WAVs for auditioning.
//   KitProbe <outDir> [kitIndex]
#include "PluginProcessor.h"
#include <cstdio>

using namespace mds;

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr) {
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (! os) return;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, (unsigned) b.getNumChannels(), 24, {}, 0));
    if (! w) return;
    os.release();
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// render a MIDI event list through the processor
static juce::AudioBuffer<float> render (DrumProcessor& proc, const std::vector<std::pair<int,int>>& hits /*sample,note*/,
                                        int len, double sr) {
    juce::AudioBuffer<float> out (2, len);
    out.clear();
    const int bs = 256;
    for (int pos = 0; pos < len; pos += bs) {
        int n = juce::jmin (bs, len - pos);
        juce::AudioBuffer<float> blk (2, n); blk.clear();
        juce::MidiBuffer mb;
        for (auto& h : hits)
            if (h.first >= pos && h.first < pos + n)
                mb.addEvent (juce::MidiMessage::noteOn (1, h.second, (juce::uint8) 120), h.first - pos);
        // short gate like a sequencer would send
        for (auto& h : hits)
            if (h.first + 2000 >= pos && h.first + 2000 < pos + n)
                mb.addEvent (juce::MidiMessage::noteOff (1, h.second), h.first + 2000 - pos);
        proc.processBlock (blk, mb);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, blk, c, 0, n);
    }
    return out;
}

static float db (double x) { return (float) (20.0 * std::log10 (juce::jmax (1e-9, x))); }

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("probe");
    int only = argc > 2 ? juce::String (argv[2]).getIntValue() : -1;
    outDir.createDirectory();
    const double sr = 44100.0;

    for (int ki = 0; ki < numFactoryKits(); ++ki) {
        if (only >= 0 && ki != only) continue;
        Kit k; mintFactoryKit (ki, k);
        printf ("\n=== %d %s ===\n", ki + 1, k.name.toRawUTF8());
        printf ("%-8s %6s %6s %7s %7s %6s | %5s %5s %5s %5s %5s %5s  (band dB rel. total)\n",
                "part", "peak", "rms50", "t-20ms", "t-40ms", "f0", "<100", "-300", "-1k", "-4k", "-10k", ">10k");
        auto kitDir = outDir.getChildFile (juce::String (ki + 1) + "_" + k.name.replace (" ", "_"));
        kitDir.createDirectory();

        for (int p = 0; p < NumParts; ++p) {
            DrumProcessor proc;
            proc.prepareToPlay (sr, 256);
            proc.loadFullStateVT (kitToStateVT (k, ki + 1));
            int len = (int) (sr * 2.5);
            auto b = render (proc, { { 256, partMidiNote (p) } }, len, sr); // block-aligned: MIDI is handled at block start
            writeWav (kitDir.getChildFile (juce::String (p).paddedLeft ('0', 2) + "_" + juce::String (partName (p)).replace (" ", "_") + ".wav"), b, sr);
            const float* x = b.getReadPointer (0);
            double peak = 0; int peakI = 0;
            for (int i = 0; i < len; ++i) if (std::abs (x[i]) > peak) { peak = std::abs (x[i]); peakI = i; }
            double rms = 0; for (int i = 256; i < 256 + 2205; ++i) rms += x[i] * x[i];
            rms = std::sqrt (rms / 2205);
            // decay: last time the 5 ms RMS window exceeds peak-20 / peak-40 dB
            auto lastAbove = [&] (float dbBelow) {
                double thr = peak * std::pow (10.0, -dbBelow / 20.0);
                int last = 256;
                for (int i = 256; i + 220 < len; i += 110) {
                    double e = 0; for (int j = 0; j < 220; ++j) e = juce::jmax (e, (double) std::abs (x[i + j]));
                    if (e > thr) last = i;
                }
                return (last - 256) / sr * 1000.0;
            };
            // f0: zero crossings of 300 Hz-lowpassed signal in 30..130 ms
            double f0 = 0;
            {
                float lp = 0, lp2 = 0, prev = 0; int zc = 0, a = 256 + (int) (0.03 * sr), e = 256 + (int) (0.13 * sr);
                float c = 1.f - std::exp (-2.f * 3.14159f * 300.f / (float) sr);
                for (int i = 256; i < e; ++i) {
                    lp += c * (x[i] - lp); lp2 += c * (lp - lp2);
                    if (i >= a && prev <= 0 && lp2 > 0) ++zc;
                    prev = lp2;
                }
                f0 = zc / 0.1;
            }
            // band balance over the first 300 ms (FFT 16384)
            const int order = 14, N = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<float> buf ((size_t) N * 2, 0.f);
            int win = (int) (0.3 * sr);
            for (int i = 0; i < win && 256 + i < len; ++i) {
                // flat from the onset (keeps the transient), cosine fade-out
                float w = i < win / 2 ? 1.f : 0.5f + 0.5f * std::cos (3.14159f * (i - win / 2) / (win / 2));
                buf[(size_t) i] = x[256 + i] * w;
            }
            fft.performFrequencyOnlyForwardTransform (buf.data());
            const double edges[7] = { 20, 100, 300, 1000, 4000, 10000, 20000 };
            double band[6] = {}, tot = 0;
            for (int bin = 1; bin < N / 2; ++bin) {
                double f = bin * sr / N, e = buf[(size_t) bin] * buf[(size_t) bin];
                for (int q = 0; q < 6; ++q) if (f >= edges[q] && f < edges[q + 1]) band[q] += e;
                tot += e;
            }
            printf ("%-8s %6.1f %6.1f %7.0f %7.0f %6.0f |", partName (p), db (peak), db (rms), lastAbove (20.f), lastAbove (40.f), f0);
            for (int q = 0; q < 6; ++q) printf (" %5.1f", 10.0 * std::log10 (juce::jmax (1e-12, band[q] / juce::jmax (1e-12, tot))));
            printf ("\n");
            (void) peakI;
        }

        // groove: DnB = 174 bpm two-step, Dubstep = 140 bpm half-time
        {
            DrumProcessor proc;
            proc.prepareToPlay (sr, 256);
            proc.loadFullStateVT (kitToStateVT (k, ki + 1));
            bool dnb = (ki % 2) == 0; // alternate a 174 two-step and a 140 half-time groove
            double bpm = dnb ? 174.0 : 140.0;
            int s16 = (int) (sr * 60.0 / bpm / 4.0);
            std::vector<std::pair<int,int>> h;
            for (int bar = 0; bar < 4; ++bar) {
                int b0 = bar * 16 * s16 + 256;
                auto at = [&] (int step, int part) { h.push_back ({ b0 + step * s16, partMidiNote (part) }); };
                if (dnb) {
                    at (0, Kick); at (10, Kick); at (4, Snare); at (12, Snare);
                    for (int s = 0; s < 16; s += 2) at (s, ClosedHat);
                    if (bar % 2 == 1) { at (14, OpenHat); at (7, Snare); }
                    if (bar == 0) at (0, Crash);
                    if (bar == 3) { at (13, Tom1); at (14, Tom2); at (15, LowTom); }
                } else {
                    at (0, Kick); at (8, Snare);
                    if (bar % 2 == 1) at (11, Kick);
                    for (int s = 0; s < 16; s += 2) at (s, ClosedHat);
                    at (6, OpenHat);
                    if (bar == 0) at (0, Crash);
                    if (bar == 3) { at (14, Snare); at (15, Snare); }
                }
            }
            auto b = render (proc, h, 4 * 16 * s16 + (int) sr, sr);
            double pk = 0; for (int i = 0; i < b.getNumSamples(); ++i) pk = juce::jmax (pk, (double) std::abs (b.getSample (0, i)));
            printf ("groove peak %.1f dBFS\n", db (pk));
            writeWav (kitDir.getChildFile ("groove.wav"), b, sr);
        }
    }
    printf ("\nwrote WAVs to %s\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}
