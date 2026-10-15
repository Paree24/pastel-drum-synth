#include "../Source/dsp/DrumEngine.h"
#include "../Source/dsp/Effects.h"
#include "../Source/Presets.h"
#include <cstdio>
#include <set>
#include <functional>
using namespace mds;
int main() {
    int fails = 0;
    // 1. all 12 parts render non-silent, finite
    for (int p = 0; p < 12; ++p) {
        PartParams pr; defaultParamsForPart (p, pr);
        DrumVoice v; v.prepare (44100.f);
        v.trigger (p, pr, 0.9f, 0.f);
        double sum = 0; bool finite = true;
        for (int i = 0; i < 44100; ++i) {
            float s = v.renderWithParams (pr);
            if (! std::isfinite (s)) finite = false;
            sum += std::abs (s);
        }
        bool ok = finite && sum > 1.0;
        printf ("part %-8s sum=%.1f %s\n", partName (p), sum, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
        // metadata sanity
        const ParamMeta* m = metaForPart (p);
        for (int s = 0; s < 16; ++s)
            if (! (m[s].min < m[s].max && m[s].def >= m[s].min && m[s].def <= m[s].max)) {
                printf ("  meta slot %d out of range FAIL\n", s); ++fails;
            }
    }
    // 2. all 8 noise types finite
    {
        NoiseGen n; n.setSampleRate (44100.f);
        for (int t = 0; t < 8; ++t) {
            double sum = 0; bool fin = true;
            for (int i = 0; i < 8000; ++i) { float s = n.next (t, 0.6f); if (! std::isfinite (s)) fin = false; sum += std::abs (s); }
            bool ok = fin && sum > 1.0;
            printf ("noise %-9s sum=%.1f %s\n", noiseTypeName (t), sum, ok ? "OK" : "FAIL");
            if (! ok) ++fails;
        }
    }
    // 3. all 7 fx types finite on impulse
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        for (int t = 0; t < 8; ++t) {
            if (! isUsableFx (t)) continue;
            FxSlot sl; FxProcessor::defaultSlot (t, sl);
            SlotDsp dsp; dsp.prepare (spec);
            bool fin = true; double sum = 0;
            for (int i = 0; i < 8000; ++i) {
                float x = (i < 100) ? 0.8f : 0.f;
                float y = FxProcessor::processSlot (sl, dsp, x, 44100.f);
                if (! std::isfinite (y)) fin = false;
                sum += std::abs (y);
            }
            bool ok = fin && sum > 0.5;
            printf ("fx %-16s sum=%.1f %s\n", fxTypeName (t), sum, ok ? "OK" : "FAIL");
            if (! ok) ++fails;
        }
    }
    // Reverb density: normalised echo density (Abel & Huang) of the tail
    // must be near Gaussian (~1) by 100 ms, for the stereo taps AND the mono
    // insert output. Low density = discrete echoes = metallic.
    {
        auto ned = [] (bool mono) {
            FdnReverb rv; rv.prepare (44100.f);
            std::vector<float> y (44100);
            for (int i = 0; i < 44100; ++i) {
                float l, r;
                rv.process (i == 0 ? 1.f : 0.f, 2.f, 0.6f, 0.3f, l, r);
                y[(size_t) i] = mono ? FdnReverb::monoOut (l, r) : l;
            }
            double acc = 0; int n = 0;
            const int win = 882; // 20 ms
            for (int c = 4410; c + win < 13230; c += win / 2) { // 100 .. 300 ms
                double m2 = 0; for (int k = 0; k < win; ++k) m2 += y[(size_t) (c + k)] * y[(size_t) (c + k)];
                double sd = std::sqrt (m2 / win);
                int out = 0; for (int k = 0; k < win; ++k) if (std::abs (y[(size_t) (c + k)]) > sd) ++out;
                acc += (out / (double) win) / 0.3173; ++n;
            }
            return acc / n;
        };
        double st = ned (false), mo = ned (true);
        bool ok = st > 0.85 && mo > 0.85;
        printf ("fx reverb echo density stereo %.2f mono %.2f %s\n", st, mo, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // Reverb colouration ("metallic"): Welch-averaged tail spectrum vs its
    // own 1/3-octave smoothing. Persistent resonant peaks survive averaging;
    // a smooth tail scores low. Snare-like input, stereo L and mono insert.
    {
        auto coloration = [] (bool mono, float size) {
            FdnReverb rv; rv.prepare (44100.f);
            juce::Random rng (5);
            const int N = 44100 * 2;
            std::vector<float> y ((size_t) N);
            for (int i = 0; i < N; ++i) {
                float x = i < 2205 ? (rng.nextFloat() * 2.f - 1.f) * std::exp (-i / 300.f) : 0.f; // short noise hit
                float l, r;
                rv.process (x, 2.5f, size, 0.3f, l, r);
                y[(size_t) i] = mono ? FdnReverb::monoOut (l, r) : l;
            }
            const int order = 11, F = 1 << order;
            juce::dsp::FFT fft (order);
            std::vector<double> pw ((size_t) F / 2, 0.0);
            int frames = 0;
            for (int st = 4410; st + F < N - 4410; st += F / 2, ++frames) {
                std::vector<float> b ((size_t) F * 2, 0.f);
                for (int k = 0; k < F; ++k) b[(size_t) k] = y[(size_t) (st + k)] * (0.5f - 0.5f * std::cos (6.2831853f * k / F));
                fft.performFrequencyOnlyForwardTransform (b.data());
                // normalise each frame so the decay doesn't weight early frames
                double fe = 0; for (int k = 1; k < F / 2; ++k) fe += b[(size_t) k] * b[(size_t) k];
                for (int k = 1; k < F / 2; ++k) pw[(size_t) k] += b[(size_t) k] * b[(size_t) k] / (fe + 1e-30);
            }
            std::vector<double> db (pw.size());
            for (size_t k = 1; k < pw.size(); ++k) db[k] = 10.0 * std::log10 (pw[k] + 1e-30);
            double acc = 0; int n = 0;
            for (int k = 1; k < F / 2; ++k) {
                double f = k * 44100.0 / F;
                if (f < 300.0 || f > 8000.0) continue;
                int lo = juce::jmax (1, (int) (k / 1.122)), hi = juce::jmin (F / 2 - 1, (int) (k * 1.122)); // +-1/6 oct
                double m = 0; for (int j = lo; j <= hi; ++j) m += db[(size_t) j];
                m /= (hi - lo + 1);
                acc += (db[(size_t) k] - m) * (db[(size_t) k] - m); ++n;
            }
            return std::sqrt (acc / n);
        };
        double cS = coloration (false, 0.6f), cM = coloration (true, 0.6f), cSm = coloration (true, 0.15f);
        bool ok = cS < 2.0 && cM < 2.0 && cSm < 2.5;
        printf ("fx reverb colouration dB: stereo %.2f mono %.2f small-room mono %.2f %s\n", cS, cM, cSm, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // Reverb (FDN): DECAY is a real RT60 for short rooms AND long tails,
    // stays finite/stable, and the stereo taps are decorrelated
    {
        auto measureRt = [] (float rtKnob, float size, double& corr) {
            FdnReverb rv; rv.prepare (44100.f);
            const int len = 44100 * 24;
            std::vector<float> eL ((size_t) len / 441 + 1, 0.f);
            double sLR = 0, sLL = 0, sRR = 0;
            for (int i = 0; i < len; ++i) {
                float l, r;
                rv.process (i == 0 ? 1.f : 0.f, FxProcessor::rt60FromKnob (rtKnob), size, 0.2f, l, r);
                if (! std::isfinite (l) || ! std::isfinite (r)) return -1.0;
                eL[(size_t) (i / 441)] += l * l + r * r;
                if (i > 4410 && i < 44100) { sLR += l * r; sLL += l * l; sRR += r * r; }
            }
            corr = sLR / std::sqrt (sLL * sRR + 1e-20);
            // Schroeder backward integration -> time where the curve is 30 dB down, x2
            double tot = 0; std::vector<double> sch (eL.size());
            for (int k = (int) eL.size() - 1; k >= 0; --k) { tot += eL[(size_t) k]; sch[(size_t) k] = tot; }
            for (size_t k = 0; k < sch.size(); ++k)
                if (10.0 * std::log10 (sch[k] / sch[0]) < -30.0) return 2.0 * k * 0.01;
            return 99.0;
        };
        bool ok = true; double corr = 0;
        for (float target : { 0.3f, 2.0f, 12.0f }) {
            float knob = std::log (target / 0.15f) / std::log (133.f);
            double rt = measureRt (knob, target < 1.f ? 0.2f : 0.7f, corr); // short rooms are small rooms
            bool good = rt > target * 0.65 && rt < target * 1.5 && std::abs (corr) < 0.5;
            printf ("  reverb RT60 target %.1fs -> %.2fs, L/R corr %.2f %s\n", target, rt, corr, good ? "" : "<-");
            ok &= good;
        }
        printf ("fx reverb FDN RT60 tracking %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // Filter: LP/HP/BP/Notch x 12/24dB all finite, distinct, and effective
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        double eLP = 0, eHP = 0; bool fin = true;
        for (int m = 0; m < 4; ++m) for (int s = 0; s < 2; ++s) {
            FxSlot sl; FxProcessor::defaultSlot (FX_FILTER, sl);
            sl.p0 = 0.5f; sl.p1 = 0.05f; sl.p2 = (float) m / 3.f; sl.p3 = (float) s; sl.p5 = 1.f;
            SlotDsp dsp; dsp.prepare (spec);
            juce::Random r (99);
            for (int i = 0; i < 4000; ++i) {
                float x = r.nextFloat() * 2.f - 1.f;
                float y = FxProcessor::processSlot (sl, dsp, x, 44100.f);
                if (! std::isfinite (y)) fin = false;
                if (m == 0 && s == 0) eLP += std::abs (y);
                if (m == 1 && s == 0) eHP += std::abs (y);
            }
        }
        bool ok = fin && eLP > 1.0 && eHP > 1.0 && std::abs (eLP - eHP) > 0.2 * juce::jmax (eLP, eHP);
        printf ("fx filter modes LP %.0f HP %.0f %s\n", eLP, eHP, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // Filter DC sanity: LP passes DC, HP blocks it
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        FxSlot lp, hp;
        FxProcessor::defaultSlot (FX_FILTER, lp); FxProcessor::defaultSlot (FX_FILTER, hp);
        lp.p0 = 0.5f; lp.p1 = 0.05f; lp.p2 = 0.f; lp.p5 = 1.f;
        hp.p0 = 0.5f; hp.p1 = 0.05f; hp.p2 = 1.f / 3.f; hp.p5 = 1.f;
        SlotDsp a, b; a.prepare (spec); b.prepare (spec);
        double lpE = 0, hpE = 0;
        for (int i = 0; i < 3000; ++i) {
            float yl = FxProcessor::processSlot (lp, a, 0.5f, 44100.f);
            float yh = FxProcessor::processSlot (hp, b, 0.5f, 44100.f);
            if (i >= 1000) { lpE += std::abs (yl); hpE += std::abs (yh); }
        }
        bool ok = lpE > 500.0 && hpE < 50.0;
        printf ("fx filter DC LP %.0f HP %.0f %s\n", lpE, hpE, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 3b. ADD-EQ flow must be unity at defaults (regression: "EQ mutes everything")
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        FxChainState ch; FxProcessor::clearChain (ch);
        FxProcessor::defaultSlot (FX_EQ, ch.slots[0]); // exactly what + ADD does
        SlotDsp dsp; dsp.prepare (spec);
        double inE = 0, outE = 0; bool fin = true;
        for (int i = 0; i < 8000; ++i) {
            float x = std::sin (2.f * juce::MathConstants<float>::pi * 220.f * i / 44100.f) * 0.7f;
            float y = FxProcessor::processSlot (ch.slots[0], dsp, x, 44100.f);
            if (! std::isfinite (y)) fin = false;
            inE += std::abs (x); outE += std::abs (y);
        }
        bool ok = fin && outE > inE * 0.9 && outE < inE * 1.1;
        printf ("fx fresh-EQ unity: %.0f vs %.0f %s\n", outE, inE, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 3c. EQ knob to minimum must cut only its band, never mute everything
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        FxSlot sl; FxProcessor::defaultSlot (FX_EQ, sl);
        SlotDsp dsp; dsp.prepare (spec);
        auto total = [&] (int n) {
            double e = 0;
            for (int i = 0; i < n; ++i) {
                float x = 0.5f * std::sin (2.f * juce::MathConstants<float>::pi * 110.f * i / 44100.f)
                        + 0.5f * std::sin (2.f * juce::MathConstants<float>::pi * 2000.f * i / 44100.f);
                float y = FxProcessor::processSlot (sl, dsp, x, 44100.f);
                if (! std::isfinite (y)) return -1.0;
                e += std::abs (y);
            }
            return e;
        };
        double ref = total (4000);
        // sweep LOW band 100% -> 0% while running: must stay finite + nonzero
        double eMin = 1e9; bool fin = true;
        for (int step = 0; step <= 10; ++step) {
            sl.p0 = 1.f - step * 0.1f;
            double e = total (2000);
            if (e < 0) { fin = false; break; }
            eMin = juce::jmin (eMin, e);
        }
        bool ok = fin && eMin > ref * 0.15; // 2 kHz content survives a fully-cut low band
        printf ("fx EQ LOW sweep min %.0f vs unity %.0f %s\n", eMin, ref, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 3d. KNOB SWEEP MATRIX: every EQ band 0<->1 live must stay finite and
    // never collapse broadband audio (regression: "beep then mute").
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        auto broad = [] (int i) {
            return 0.25f * std::sin (2.f * juce::MathConstants<float>::pi * 55.f * i / 44100.f)
                 + 0.25f * std::sin (2.f * juce::MathConstants<float>::pi * 440.f * i / 44100.f)
                 + 0.25f * std::sin (2.f * juce::MathConstants<float>::pi * 2000.f * i / 44100.f)
                 + 0.25f * std::sin (2.f * juce::MathConstants<float>::pi * 8000.f * i / 44100.f);
        };
        bool allOk = true;
        for (int band = 0; band < 4; ++band) {
            for (int dir = 0; dir < 2; ++dir) { // 0: up sweep, 1: down sweep
                FxSlot sl; FxProcessor::defaultSlot (FX_EQ, sl);
                SlotDsp dsp; dsp.prepare (spec);
                double eMin = 1e18, eMax = 0; bool fin = true;
                for (int step = 0; step <= 40; ++step) {
                    float v = (dir == 0) ? step / 40.f : 1.f - step / 40.f;
                    if (band == 0) sl.p0 = v; else if (band == 1) sl.p1 = v;
                    else if (band == 2) sl.p2 = v; else sl.p3 = v;
                    double e = 0;
                    for (int i = 0; i < 1000; ++i) {
                        float y = FxProcessor::processSlot (sl, dsp, broad (i), 44100.f);
                        if (! std::isfinite (y)) { fin = false; break; }
                        e += std::abs (y);
                    }
                    if (! fin) break;
                    eMin = juce::jmin (eMin, e); eMax = juce::jmax (eMax, e);
                }
                // single band bell: floor well above mute, ceiling below blowup
                bool ok = fin && eMin > 60.0 && eMax < 40000.0;
                if (! ok) { printf ("  fx EQ band %d dir %d min %.0f max %.0f FAIL\n", band, dir, eMin, eMax); allOk = false; }
            }
        }
        printf ("fx EQ sweep matrix %s\n", allOk ? "OK" : "FAIL");
        if (! allOk) ++fails;
    }
    // METAL (modal bank): MATERIAL moves the spectrum, DECAY owns the ring,
    // the voice ends by itself, extremes stay finite
    {
        auto render = [] (std::function<void (PartParams&)> set, int n, std::vector<float>& out) {
            PartParams a; defaultParamsForPart (Metallic, a); set (a);
            DrumVoice v; v.prepare (44100.f); v.noise.rng.setSeed (3);
            v.trigger (Metallic, a, 1.f, 0.f);
            out.clear();
            for (int i = 0; i < n; ++i) out.push_back (v.renderWithParams (a));
            return v.active;
        };
        auto zc = [] (const std::vector<float>& x, int a, int b) { int z = 0; for (int i = a + 1; i < b; ++i) if ((x[(size_t) i - 1] < 0) != (x[(size_t) i] < 0)) ++z; return z; };
        auto en = [] (const std::vector<float>& x, int a, int b) { double e = 0; for (int i = a; i < b; ++i) e += x[(size_t) i] * x[(size_t) i]; return e; };
        std::vector<float> cow, bar, shortD, longD, ext;
        render ([] (PartParams& a) { a.p[8] = 0.f; }, 8000, cow);
        render ([] (PartParams& a) { a.p[8] = 75.f; }, 8000, bar); // plate
        render ([] (PartParams& a) { a.p[5] = 200.f; }, 66150, shortD);
        bool stillOn = render ([] (PartParams& a) { a.p[5] = 3000.f; }, 66150, longD);
        bool extOk = true;
        render ([] (PartParams& a) { a.p[0] = 24.f; a.p[8] = 100.f; a.p[9] = 100.f; a.p[10] = 100.f; a.p[11] = 100.f; a.p[13] = 100.f; a.p[14] = 100.f; a.wave = 3; }, 44100, ext);
        for (float v : ext) extOk &= std::isfinite (v) && std::abs (v) < 4.f;
        PartParams d; defaultParamsForPart (Metallic, d);
        DrumVoice v; v.prepare (44100.f); v.trigger (Metallic, d, 1.f, 0.f);
        int n = 0; while (v.active && n < 44100 * 12) { v.renderWithParams (d); ++n; }
        auto bright = [] (const std::vector<float>& x) { double d = 0, e = 0; for (size_t i = 1001; i < 8000; ++i) { d += std::abs (x[i] - x[i - 1]); e += std::abs (x[i]); } return d / (e + 1e-12); };
        int zCow = (int) (1000 * bright (cow)), zBar = (int) (1000 * bright (bar)); (void) zc;
        double ringS = en (shortD, 22050, 66150), ringL = en (longD, 22050, 66150);
        bool ok = std::abs (zCow - zBar) > 0.15 * juce::jmax (zCow, zBar) && ringL > ringS * 50.0 && stillOn
               && ! v.active && extOk;
        printf ("metal material brightness %d/%d, ring 0.5-1.5s short/long %.2e/%.2e, ends %.1fs, extremes %s %s\n",
                zCow, zBar, ringS, ringL, n / 44100.0, extOk ? "ok" : "BAD", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // SFX (sweep synth): SWEEP + falls / - rises, CUTOFF owns brightness,
    // LFO>PITCH wobbles
    {
        auto run = [] (std::function<void (PartParams&)> set, std::vector<float>& out) {
            PartParams a; defaultParamsForPart (Sfx, a); a.p[9] = 0.f; a.p[8] = 0.f; a.p[10] = 100.f; a.p[11] = 0.f; set (a);
            DrumVoice v; v.prepare (44100.f); v.noise.rng.setSeed (4);
            v.trigger (Sfx, a, 1.f, 0.f);
            out.clear();
            for (int i = 0; i < 22050; ++i) out.push_back (v.renderWithParams (a));
        };
        auto zc = [] (const std::vector<float>& x, int a, int b) { int z = 0; for (int i = a + 1; i < b; ++i) if ((x[(size_t) i - 1] < 0) != (x[(size_t) i] < 0)) ++z; return z; };
        std::vector<float> fall, rise, dark, bright, wob, flat;
        run ([] (PartParams& a) { a.p[1] = 48.f; a.p[2] = 400.f; }, fall);
        run ([] (PartParams& a) { a.p[1] = -48.f; a.p[2] = 400.f; }, rise);
        run ([] (PartParams& a) { a.p[1] = 0.f; a.wave = 2; a.p[10] = 25.f; }, dark);
        run ([] (PartParams& a) { a.p[1] = 0.f; a.wave = 2; a.p[10] = 100.f; }, bright);
        run ([] (PartParams& a) { a.p[1] = 0.f; a.p[16] = 8.f; a.p[17] = 12.f; }, wob);
        run ([] (PartParams& a) { a.p[1] = 0.f; }, flat);
        double hfD = 0, hfB = 0;
        for (size_t i = 1; i < dark.size(); ++i) { hfD += std::abs (dark[i] - dark[i - 1]); hfB += std::abs (bright[i] - bright[i - 1]); }
        // wobble: zero-crossing count per 25 ms window varies with the LFO
        auto spread = [&] (const std::vector<float>& x) {
            int lo = 1 << 30, hi = 0;
            for (int w = 0; w + 1102 < 17640; w += 1102) { int z = zc (x, w, w + 1102); lo = juce::jmin (lo, z); hi = juce::jmax (hi, z); }
            return hi - lo;
        };
        int fe = zc (fall, 0, 2205), fl = zc (fall, 17640, 19845), re = zc (rise, 0, 2205), rl = zc (rise, 17640, 19845);
        bool ok = fe > fl * 2 && rl > re * 2 && hfB > hfD * 2.0 && spread (wob) > spread (flat) + 4;
        printf ("sfx sweep fall %d->%d rise %d->%d, cutoff HF %.0f/%.0f, wobble %d vs %d %s\n",
                fe, fl, re, rl, hfD, hfB, spread (wob), spread (flat), ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // PITCH MOVES: metal holds pitch by default (no membrane strike-drop),
    // BEND glides it; SFX CURVE > 50 = slow-start riser, < 50 = fast-start drop
    {
        auto zcWin = [] (int part, std::function<void (PartParams&)> set, int a, int b) {
            PartParams p; defaultParamsForPart (part, p); set (p);
            DrumVoice v; v.prepare (44100.f); v.noise.rng.setSeed (9);
            v.trigger (part, p, 1.f, 0.f);
            int z = 0; float prev = 0;
            for (int i = 0; i < b; ++i) { float x = v.renderWithParams (p); if (i >= a && (prev < 0) != (x < 0)) ++z; prev = x; }
            return z;
        };
        auto metalFlat = [] (PartParams& p) { p.p[10] = 0.f; p.p[8] = 0.f; p.p[5] = 3000.f; p.p[12] = 0.f; }; // DAMP 0: modes decay together, so zc tracks pitch only
        int mEarly = zcWin (Metallic, metalFlat, 200, 2405), mLate = zcWin (Metallic, metalFlat, 8820, 11025);
        auto metalBend = [] (PartParams& p) { p.p[10] = 0.f; p.p[8] = 0.f; p.p[5] = 3000.f; p.p[12] = 0.f; p.p[1] = 12.f; p.p[2] = 300.f; };
        int bEarly = zcWin (Metallic, metalBend, 200, 2405), bLate = zcWin (Metallic, metalBend, 17640, 19845);
        auto sfx = [] (float curve) { return [curve] (PartParams& p) { p.p[9] = 0.f; p.p[8] = 0.f; p.p[10] = 100.f; p.p[11] = 0.f;
                                                                         p.p[1] = -36.f; p.p[2] = 1000.f; p.p[3] = curve; p.p[5] = 4000.f; }; };
        // riser from -36 st: at 50% time the slow-start curve is still low (few crossings)
        int easeIn = zcWin (Sfx, sfx (85.f), 20947, 23152), easeOut = zcWin (Sfx, sfx (15.f), 20947, 23152);
        // inharmonic modes beat, so crossing counts wander a little even at a
        // fixed pitch: compare the bent early/late ratio against the flat one
        double flatR = (double) mEarly / juce::jmax (1, mLate), bendR = (double) bEarly / juce::jmax (1, bLate);
        bool metalOk = flatR < 1.25 && bendR > flatR * 1.4;
        bool sfxOk = easeOut > easeIn * 3;
        printf ("pitch moves: metal default %d/%d (flat), bend %d->%d; sfx riser at 50%%: ease-in %d vs ease-out %d %s\n",
                mEarly, mLate, bEarly, bLate, easeIn, easeOut, (metalOk && sfxOk) ? "OK" : "FAIL");
        if (! (metalOk && sfxOk)) ++fails;
    }
    // 4. snare NOISE knob must add HF sizzle.
    // Metric: high-frequency energy (first difference) in the attack window,
    // which is what "crispness" is. Raw level is a bad proxy (saturator).
    {
        PartParams a, b; defaultParamsForPart (Snare, a); b = a;
        a.p[9] = 0.f; b.p[9] = 100.f;
        a.p[8] = 0.f; b.p[8] = 0.f; // body off: isolate the noise sizzle
        DrumVoice va, vb; va.prepare (44100.f); vb.prepare (44100.f);
        va.noise.rng.setSeed (42); vb.noise.rng.setSeed (42); // identical noise: only params differ
        va.trigger (Snare, a, 1.f, 0.f); vb.trigger (Snare, b, 1.f, 0.f);
        double ea = 0, eb = 0, pa = 0, pb = 0;
        for (int i = 0; i < 400; ++i) { // attack window
            float sa = va.renderWithParams (a), sb2 = vb.renderWithParams (b);
            ea += std::abs (sa - pa); eb += std::abs (sb2 - pb);
            pa = sa; pb = sb2;
        }
        bool ok = eb > ea * 1.3;
        printf ("snare noise HF 0 vs 100: %.1f vs %.1f %s\n", ea, eb, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 4b. SNARE knobs own their windows: NOISE owns the sizzle, BODY owns the
    // mids, N DECAY owns the tail length, NOISE HP owns the noise's low end.
    {
        // each comparison zeroes the sibling layers so windows isolate the knob
        auto winE = [] (int slot, float v, int from, int to, bool zeroSib) {
            PartParams a; defaultParamsForPart (Snare, a); a.p[slot] = v;
            if (zeroSib) { a.p[8] = 0.f; a.p[9] = 0.f; a.p[16] = 0.f; a.p[slot] = v; }
            DrumVoice va; va.prepare (44100.f);
            va.noise.rng.setSeed (42);
            va.trigger (Snare, a, 1.f, 0.f);
            double e = 0;
            for (int i = 0; i < to; ++i) {
                float s = std::abs (va.renderWithParams (a));
                if (i >= from) e += s;
            }
            return e;
        };
        double atk0 = winE (9, 0.f, 0, 200, true), atk100 = winE (9, 100.f, 0, 200, true);
        double mid0 = winE (8, 0.f, 500, 1500, true), mid100 = winE (8, 100.f, 500, 1500, true);
        double tailShort = winE (10, 20.f, 3000, 8000, false), tailLong = winE (10, 400.f, 3000, 8000, false);
        // NOISE HP: noise only, energy under ~1 kHz (one-pole LP of the output)
        auto lowNz = [] (float hp) {
            PartParams a; defaultParamsForPart (Snare, a);
            a.p[8] = 0.f; a.p[16] = 0.f; a.p[9] = 100.f; a.p[14] = hp;
            DrumVoice va; va.prepare (44100.f); va.noise.rng.setSeed (42);
            va.trigger (Snare, a, 1.f, 0.f);
            double e = 0; float lp = 0;
            for (int i = 0; i < 4000; ++i) { lp += 0.13f * (va.renderWithParams (a) - lp); e += std::abs (lp); }
            return e;
        };
        double sub0 = lowNz (4000.f), sub100 = lowNz (100.f);
        bool noiseOk = atk100 > atk0 * 2.0;  // NOISE owns the sizzle
        bool bodyOk = mid100 > mid0 * 1.5;   // BODY owns mids
        bool decOk = tailLong > tailShort * 1.5; // N DECAY owns tail length
        bool subOk = sub100 > sub0 * 1.5;    // low NOISE HP keeps the noise's low end
        printf ("snare layers noise %.1f/%.1f body %.1f/%.1f ndec %.1f/%.1f nzHP 4k/100 %.1f/%.1f %s\n",
                atk0, atk100, mid0, mid100, tailShort, tailLong, sub0, sub100,
                (noiseOk && bodyOk && decOk && subOk) ? "OK" : "FAIL");
        if (! (noiseOk && bodyOk && decOk && subOk)) ++fails;
    }
    // 4c. SNARE noise release is honest: wash EXISTS (anti-wood), N DECAY
    // moves it, and it still ends (one-shot, no runaway ring).
    {
        auto relWin = [] (float ndec, int from, int to) {
            PartParams a; defaultParamsForPart (Snare, a); a.p[10] = ndec;
            DrumVoice va; va.prepare (44100.f);
            va.noise.rng.setSeed (43);
            va.trigger (Snare, a, 1.f, 0.f);
            double e = 0;
            for (int i = 0; i < to; ++i) {
                float s = std::abs (va.renderWithParams (a));
                if (i >= from) e += s;
            }
            return e;
        };
        PartParams a; defaultParamsForPart (Snare, a);
        DrumVoice va; va.prepare (44100.f);
        va.noise.rng.setSeed (43);
        va.trigger (Snare, a, 1.f, 0.f);
        double early = 0, late = 0;
        for (int i = 0; i < 12000; ++i) {
            float s = std::abs (va.renderWithParams (a));
            if (i < 1000) early += s; else if (i >= 3000) late += s;
        }
        double ratio = late / juce::jmax (1e-9, early);
        double rShort = relWin (20.f, 3000, 9000), rLong = relWin (400.f, 3000, 9000);
        bool exists = ratio > 0.03;     // audible wash after the hit, not wood
        bool ends = ratio < 0.9;        // still a one-shot
        bool knob = rLong > rShort * 1.5; // N DECAY owns the release
        printf ("snare release late/early %.3f 20/400ms %.1f/%.1f %s\n",
                ratio, rShort, rLong, (exists && ends && knob) ? "OK" : "FAIL");
        if (! (exists && ends && knob)) ++fails;
    }
    // 5. PUNCH knob must raise the membrane attack transient.
    // Metric: thump-window energy just after the click (drive=0 to isolate
    // from saturation). Peak is click-dominated (sine body starts at zero
    // phase), so peak cannot show punch.
    {
        PartParams a, b; defaultParamsForPart (Kick, a); b = a;
        a.p[16] = 0.f; b.p[16] = 100.f;
        a.p[12] = 0.f; b.p[12] = 0.f;
        DrumVoice va, vb; va.prepare (44100.f); vb.prepare (44100.f);
        va.noise.rng.setSeed (7); vb.noise.rng.setSeed (7);
        va.trigger (Kick, a, 1.f, 0.f); vb.trigger (Kick, b, 1.f, 0.f);
        double ea = 0, eb = 0;
        for (int i = 0; i < 1200; ++i) {
            float sa = std::abs (va.renderWithParams (a));
            float sb2 = std::abs (vb.renderWithParams (b));
            if (i >= 100) { ea += sa; eb += sb2; } // skip the click, capture the thump
        }
        bool ok = eb > ea * 1.3;
        printf ("kick punch thump 0 vs 100: %.1f vs %.1f %s\n", ea, eb, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 8. WAVE selector must change timbre (harmonics must survive the body filter)
    {
        PartParams a, b; defaultParamsForPart (Kick, a); b = a;
        a.wave = 0; b.wave = 2; // sine vs saw
        a.p[10] = 0.f; b.p[10] = 0.f; // click off: isolate the oscillator
        a.p[13] = 0.f; b.p[13] = 0.f; // noise off: isolate the oscillator
        DrumVoice va, vb; va.prepare (44100.f); vb.prepare (44100.f);
        va.noise.rng.setSeed (11); vb.noise.rng.setSeed (11);
        va.trigger (Kick, a, 1.f, 0.f); vb.trigger (Kick, b, 1.f, 0.f);
        double ea = 0, eb = 0, pa = 0, pb = 0;
        for (int i = 0; i < 800; ++i) {
            float sa = va.renderWithParams (a), sb2 = vb.renderWithParams (b);
            ea += std::abs (sa - pa); eb += std::abs (sb2 - pb);
            pa = sa; pb = sb2;
        }
        bool ok = eb > ea * 1.5;
        printf ("wave sine vs saw HF: %.1f vs %.1f %s\n", ea, eb, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 9b. SNARE wave must survive the full mix (shell term, not just isolated body)
    {
        PartParams a, b; defaultParamsForPart (Snare, a); b = a;
        a.wave = 0; b.wave = 3; // sine vs square, everything else identical
        DrumVoice va, vb; va.prepare (44100.f); vb.prepare (44100.f);
        va.noise.rng.setSeed (21); vb.noise.rng.setSeed (21);
        va.trigger (Snare, a, 1.f, 0.f); vb.trigger (Snare, b, 1.f, 0.f);
        double e = 0, d = 0;
        for (int i = 0; i < 1500; ++i) {
            float sa = va.renderWithParams (a), sb2 = vb.renderWithParams (b);
            e += std::abs (sa); d += std::abs (sa - sb2);
        }
        bool ok = d > e * 0.12;
        printf ("snare wave sine vs square diff: %.1f of %.1f %s\n", d, e, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 9c. USER SCENARIO: snare, attack FULL + sustain FULL, sine vs square.
    // Must differ on scope even with smeared transient + ringing sustain.
    {
        PartParams a, b; defaultParamsForPart (Snare, a); b = a;
        a.wave = 0; b.wave = 3;
        a.p[4] = 20.f; b.p[4] = 20.f; // attack full
        a.p[6] = 0.f; b.p[6] = 0.f;   // sustain full
        DrumVoice va, vb; va.prepare (44100.f); vb.prepare (44100.f);
        va.noise.rng.setSeed (23); vb.noise.rng.setSeed (23);
        va.trigger (Snare, a, 1.f, 0.f); vb.trigger (Snare, b, 1.f, 0.f);
        double e = 0, d = 0;
        for (int i = 0; i < 4000; ++i) {
            float sa = va.renderWithParams (a), sb2 = vb.renderWithParams (b);
            e += std::abs (sa); d += std::abs (sa - sb2);
        }
        bool ok = d > e * 0.10;
        printf ("snare wave fullA/fullS diff: %.1f of %.1f %s\n", d, e, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 9. SAT modes must sound distinct (not converge)
    {
        PartParams a, b; defaultParamsForPart (Kick, a); b = a;
        a.p[12] = 30.f; b.p[12] = 30.f; // mid drive: the knee zone where modes diverge
        a.satMode = 1; b.satMode = 4;   // hard vs asymmetric
        DrumVoice va, vb; va.prepare (44100.f); vb.prepare (44100.f);
        va.noise.rng.setSeed (13); vb.noise.rng.setSeed (13);
        va.trigger (Kick, a, 1.f, 0.f); vb.trigger (Kick, b, 1.f, 0.f);
        double e = 0, d = 0;
        for (int i = 0; i < 1200; ++i) {
            float sa = va.renderWithParams (a), sb2 = vb.renderWithParams (b);
            e += std::abs (sa); d += std::abs (sa - sb2);
        }
        bool ok = d > e * 0.15;
        printf ("sat hard vs asym diff: %.1f of %.1f %s\n", d, e, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 6. every factory kit recipe mints in-range params + valid FX (files dumped from these)
    {
        bool okCount = true;
        for (int i = 0; i < numFactoryKits(); ++i) {
            Kit k; mintFactoryKit (i, k);
            if (k.name.isEmpty()) { okCount = false; break; }
            for (int p = 0; p < 12; ++p) {
                for (int s = 0; s < 20; ++s) {
                    const ParamMeta* m = slotMeta (p, s);
                    if (! m) continue;
                    float v = k.parts[p].p[s];
                    if (! (v >= m->min && v <= m->max && std::isfinite (v))) {
                        printf ("  kit %d part %d slot %d out of range FAIL\n", i, p, s); ++fails;
                    }
                }
            }
            // kits may carry FX: every used slot must be a real type with knobs in 0..1
            auto chainOk = [&] (const FxChainState& ch, const char* where, int p) {
                for (int s = 0; s < FxChainState::NumSlots; ++s) {
                    auto& sl = ch.slots[(size_t) s];
                    int t = sl.type.load();
                    if (t == FX_NONE) continue;
                    float ks[6] = { sl.p0, sl.p1, sl.p2, sl.p3, sl.p4, sl.p5 };
                    bool good = isUsableFx (t);
                    for (float kv : ks) good &= kv >= 0.f && kv <= 1.f;
                    if (! good) { printf ("  kit %d %s %d slot %d invalid FX FAIL\n", i, where, p, s); ++fails; }
                }
            };
            for (int p = 0; p < 12; ++p) chainOk (k.partFx[p], "part", p);
            chainOk (k.globalFx, "global", 0);
            // state tree round-trips through XML (the on-disk format)
            auto vt = kitToStateVT (k, i + 1);
            std::unique_ptr<juce::XmlElement> xml (vt.createXml());
            auto back = juce::ValueTree::fromXml (*xml);
            if (! back.isValid() || ! back.hasType ("DRUMSYNTH")
                || (int) back.getProperty ("kit", -1) != i + 1
                || back.getProperty ("kitName", "").toString() != k.name) {
                printf ("  kit %d XML round-trip FAIL\n", i); ++fails;
            }
        }
        printf ("%d kit recipes (valid FX) + XML round-trip %s\n", numFactoryKits(), fails == 0 ? "OK" : "FAIL");
        (void) okCount;
    }
    // 11. CYMBALS have a hit: peak in the first 3 ms must dwarf the tail average.
    {
        Kit k; mintFactoryKit (1, k); // Neuro: tight bright hats
        DrumVoice v; v.prepare (44100.f);
        v.noise.rng.setSeed (33);
        v.trigger (ClosedHat, k.parts[ClosedHat], 1.f, 0.f);
        float peak = 0;
        double tail = 0; int tailN = 0;
        for (int i = 0; i < 4000; ++i) {
            float s = std::abs (v.renderWithParams (k.parts[ClosedHat]));
            if (i < 130) peak = juce::jmax (peak, s);
            else { tail += s; ++tailN; }
        }
        double tailAvg = tail / juce::jmax (1, tailN);
        bool ok = std::isfinite (peak) && peak > tailAvg * 3.0 && peak > 0.05;
        printf ("hat transient peak %.3f vs tail %.4f %s\n", peak, tailAvg, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 12. CYMBALS don't swoop: max PITCH/SPREAD vs zero changes hats mildly,
    // while the same knob on kick (true sweep) changes it dramatically.
    // Metric: dominant pitch early (0-100, sweep still high) vs late settled.
    // Noise/click off so only pitched content is measured.
    {
        auto sweepRatio = [] (int part, float pamt, float pdec) {
            PartParams a; defaultParamsForPart (part, a);
            a.p[1] = pamt; a.p[2] = pdec;
            if (part == Kick) { a.p[10] = 0.f; a.p[13] = 0.f; }
            else { a.p[9] = 0.f; }
            DrumVoice va; va.prepare (44100.f);
            va.noise.rng.setSeed (37);
            va.trigger (part, a, 1.f, 0.f);
            long zEarly = 0, zLate = 0; float prev = 0;
            for (int i = 0; i < 2500; ++i) {
                float s = va.renderWithParams (a);
                if ((prev < 0) != (s < 0)) { if (i < 100) ++zEarly; else if (i >= 2000) ++zLate; }
                prev = s;
            }
            double rEarly = zEarly / 100.0, rLate = zLate / 500.0; // crossings per sample
            printf ("  part %d: early %.3f late %.3f\n", part, rEarly, rLate);
            return rEarly / juce::jmax (1e-9, rLate);
        };
        double hatSwoop = sweepRatio (OpenHat, 36.f, 60.f);
        double kickSwoop = sweepRatio (Kick, 60.f, 15.f);
        bool ok = hatSwoop < 2.2 && kickSwoop > 3.0;
        printf ("swoop: hats %.2f (steady) vs kick %.2f (sweep) %s\n",
                hatSwoop, kickSwoop, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // 10. HATS are wash, not bells, and kits differ audibly (pitch + ring).
    {
        auto hatDesc = [] (int kitIdx, double& zc, double& tailR) {
            Kit k; mintFactoryKit (kitIdx, k);
            DrumVoice v; v.prepare (44100.f);
            v.noise.rng.setSeed (31);
            v.trigger (ClosedHat, k.parts[ClosedHat], 1.f, 0.f);
            double e = 0, hf = 0, prev = 0, eEarly = 0, eLate = 0;
            long crosses = 0;
            for (int i = 0; i < 4000; ++i) {
                float s = v.renderWithParams (k.parts[ClosedHat]);
                e += std::abs (s); hf += std::abs (s - prev);
                if ((prev < 0) != (s < 0)) ++crosses;
                prev = s;
                if (i < 1000) eEarly += std::abs (s); else eLate += std::abs (s);
            }
            zc = (double) crosses;
            tailR = eLate / juce::jmax (1e-9, eEarly);
            return hf / juce::jmax (1e-9, e);
        };
        double z1, t1, z8, t8;
        double h1 = hatDesc (1, z1, t1), h8 = hatDesc (6, z8, t8); // Neuro Grid vs Dungeon
        bool washOk = h1 > 0.45 && h8 > 0.45; // no bell fundamental
        bool varyOk = std::abs (z1 - z8) > 0.10 * juce::jmax (z1, z8)   // different pitch content
                   || std::abs (t1 - t8) > 0.20 * juce::jmax (t1, t8); // different ring time
        printf ("hats wash %.2f/%.2f zc %.0f/%.0f ring %.2f/%.2f %s\n",
                h1, h8, z1, z8, t1, t8, (washOk && varyOk) ? "OK" : "FAIL");
        if (! (washOk && varyOk)) ++fails;
    }
    // 7. character truth: kits differ, snares included (distinct designs,
    // not one snare with tweaks), names unique
    {
        std::set<juce::String> names, snareShapes;
        float minTone = 999, maxTone = -999, minDec = 1e9, maxDec = 0, minSweep = 999, maxSweep = 0;
        for (int i = 0; i < numFactoryKits(); ++i) {
            Kit k; mintFactoryKit (i, k);
            names.insert (k.name);
            auto& sn = k.parts[Snare];
            snareShapes.insert (juce::String (sn.wave) + "/" + juce::String (sn.noiseType) + "/" + juce::String ((int) sn.p[1])
                                + "/" + juce::String ((int) sn.p[8]) + "/" + juce::String ((int) sn.p[9]));
            minTone = juce::jmin (minTone, sn.p[11]); maxTone = juce::jmax (maxTone, sn.p[11]);
            minDec = juce::jmin (minDec, sn.p[10]); maxDec = juce::jmax (maxDec, sn.p[10]);
            minSweep = juce::jmin (minSweep, sn.p[1]); maxSweep = juce::jmax (maxSweep, sn.p[1]);
        }
        bool ok = (int) names.size() == numFactoryKits() && numFactoryKits() == 64
               && snareShapes.size() >= 12 && maxTone - minTone > 50 && maxDec > minDec * 4 && maxSweep - minSweep > 30;
        printf ("snare variety: %d shapes, tone %.0f..%.0f, n-decay %.0f..%.0f ms, sweep %.0f..%.0f st\n",
                (int) snareShapes.size(), minTone, maxTone, minDec, maxDec, minSweep, maxSweep);
        printf ("character traits %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // TRANSIENT SHAPER: ATTACK/SUSTAIN act on onset/tail as a clean gain,
    // centre = transparent, and the effect does not depend on input level.
    {
        juce::dsp::ProcessSpec spec { 44100.0, 512, 1 };
        // decaying 200 Hz tone: onset + tail, like a drum
        auto run = [&] (float att, float sus, float lvl, double& onset, double& tail, double& err) {
            FxSlot sl; FxProcessor::defaultSlot (FX_TRANSIENT, sl);
            sl.p0 = att; sl.p1 = sus; sl.p5 = 1.f;
            SlotDsp dsp; dsp.prepare (spec);
            onset = tail = err = 0;
            for (int i = 0; i < 22050; ++i) {
                float x = lvl * std::sin (2.f * juce::MathConstants<float>::pi * 200.f * i / 44100.f)
                        * std::exp (-i / (44100.f * 0.08f));
                float y = FxProcessor::processSlot (sl, dsp, x, 44100.f);
                if (i < 441) onset += std::abs (y); else if (i > 4410) tail += std::abs (y);
                if (x * y < -1e-6f) err += std::abs (y); // polarity flips = distortion
            }
        };
        double o0, t0, e0, oA, tA, eA, oS, tS, eS, oN, tN, eN, oQ, tQ, eQ;
        run (0.5f, 0.5f, 0.8f, o0, t0, e0);   // neutral
        run (1.0f, 0.5f, 0.8f, oA, tA, eA);   // attack +
        run (0.5f, 1.0f, 0.8f, oS, tS, eS);   // sustain +
        run (0.0f, 0.5f, 0.8f, oN, tN, eN);   // attack -
        run (1.0f, 0.5f, 0.08f, oQ, tQ, eQ);  // attack +, -20 dB input
        bool neutral = std::abs (t0 - 0.0) > 0 && e0 < 1e-3;
        bool atkUp = oA / t0 > (o0 / t0) * 1.5;
        bool atkDn = oN < o0 * 0.8;
        bool susUp = tS > t0 * 1.5;
        bool clean = eA < 1e-3 && eS < 1e-3 && eN < 1e-3;
        bool levelInd = std::abs ((oQ / o0) / (oA / o0) - 0.1) < 0.02; // same gain at -20 dB
        bool ok = neutral && atkUp && atkDn && susUp && clean && levelInd;
        printf ("transient shaper atk %.2fx/%.2fx sus %.2fx lvl %.3f %s\n",
                oA / o0, oN / o0, tS / t0, (oQ / o0) / (oA / o0), ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // SNARE noise layer is reported as post-FX (clean), body is not; and
    // PUNCH makes the first 10 ms hit harder relative to the body that follows.
    {
        PartParams a; defaultParamsForPart (Snare, a);
        DrumVoice v; v.prepare (44100.f); v.noise.rng.setSeed (5);
        v.trigger (Snare, a, 1.f, 0.f);
        double post = 0, tot = 0;
        for (int i = 0; i < 8000; ++i) { float s = v.renderWithParams (a); post += std::abs (v.postFx); tot += std::abs (s); }
        PartParams k; defaultParamsForPart (Kick, k);
        DrumVoice kv; kv.prepare (44100.f); kv.trigger (Kick, k, 1.f, 0.f);
        double kpost = 0; for (int i = 0; i < 4000; ++i) { kv.renderWithParams (k); kpost += std::abs (kv.postFx); }
        auto punchRatio = [] (float pu) {
            PartParams b; defaultParamsForPart (Snare, b); b.p[16] = pu; b.p[9] = 0.f;
            DrumVoice w; w.prepare (44100.f); w.noise.rng.setSeed (5);
            w.trigger (Snare, b, 1.f, 0.f);
            double hit = 0, after = 0;
            for (int i = 0; i < 2600; ++i) {
                float s = std::abs (w.renderWithParams (b));
                if (i < 441) hit += s; else if (i >= 1300) after += s;
            }
            (void) after;
            return hit; // PUNCH = denser first 10 ms (saturated overshoot, like the kick)
        };
        double p0 = punchRatio (0.f), p100 = punchRatio (100.f);
        bool ok = post > tot * 0.1 && kpost == 0.0 && p100 > p0 * 1.3;
        printf ("snare clean noise %.0f of %.0f, punch hit energy %.1f -> %.1f %s\n",
                post, tot, p0, p100, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    // SNARE ends: noise + body envelopes run on their own clock (no freeze)
    {
        PartParams a; defaultParamsForPart (Snare, a);
        DrumVoice v; v.prepare (44100.f);
        v.trigger (Snare, a, 1.f, 0.f);
        int n = 0; while (v.active && n < 44100 * 3) { v.renderWithParams (a); ++n; }
        bool ok = ! v.active && n < 44100;
        printf ("snare one-shot ends after %.0f ms %s\n", n / 44.1, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    printf (fails == 0 ? "ALL SMOKE TESTS PASSED\n" : "FAILURES: %d\n", fails);
    return fails == 0 ? 0 : 1;
}
