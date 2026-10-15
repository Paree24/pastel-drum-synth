// End-to-end UI-path test: drive real APVTS parameters exactly like the UI
// does (synchronous setValue, no async), pull to engine, render, compare.
// Proves selector plumbing, not just DSP in isolation.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <set>

using namespace mds;

static void flushMessages() {
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
}

static double renderKickHF (DrumProcessor& proc, int waveSel) {
    auto* w = proc.apvts.getParameter ("p0_wave");
    w->setValueNotifyingHost (w->convertTo0to1 ((float) waveSel)); // exactly what the UI does
    flushMessages();
    proc.pullApvtsToParts();
    auto& pe = proc.parts[Kick];
    if (pe.params.wave != waveSel) {
        printf ("  WAVE PLUMBING BROKEN: selected %d engine has %d\n", waveSel, pe.params.wave);
        return -1.0;
    }
    // plumbing under test = the WAVE value the engine received; the rest is
    // the default kick so the loaded factory kit's tuning/drive can't mask it
    PartParams pr; defaultParamsForPart (Kick, pr);
    pr.wave = pe.params.wave;
    pr.p[10] = 0.f; pr.p[13] = 0.f; // click + noise off: isolate oscillator
    DrumVoice v; v.prepare (44100.f);
    v.noise.rng.setSeed (11);
    v.trigger (Kick, pr, 1.f, 0.f);
    double e = 0, prev = 0;
    for (int i = 0; i < 800; ++i) {
        float s = v.renderWithParams (pr);
        e += std::abs (s - prev); prev = s;
    }
    return e;
}

// render through processBlock (all buses), optionally hitting one part by MIDI first
static juce::AudioBuffer<float> renderProc (DrumProcessor& proc, int part, int nSamples, float vel = 1.f) {
    juce::AudioBuffer<float> out (proc.getTotalNumOutputChannels(), nSamples);
    out.clear();
    int pos = 0; bool hit = part < 0;
    while (pos < nSamples) {
        int n = juce::jmin (512, nSamples - pos);
        juce::AudioBuffer<float> blk (proc.getTotalNumOutputChannels(), n);
        blk.clear();
        juce::MidiBuffer mb;
        if (! hit) { mb.addEvent (juce::MidiMessage::noteOn (10, proc.parts[part].midiNote, vel), 0); hit = true; }
        proc.processBlock (blk, mb);
        for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom (c, pos, blk, c, 0, n);
        pos += n;
    }
    return out;
}
static float chanPeak (const juce::AudioBuffer<float>& b, int ch0, int ch1, bool* finite = nullptr) {
    float pk = 0.f;
    for (int c = ch0; c <= ch1 && c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i) {
            float v = b.getSample (c, i);
            if (! std::isfinite (v) && finite) *finite = false;
            pk = std::max (pk, std::abs (v));
        }
    return pk;
}
static void silenceProc (DrumProcessor& proc) {
    for (auto& pe : proc.parts) for (auto& v : pe.voices) v.active = false;
    renderProc (proc, -1, 44100 * 5);   // let FX / send tails die
}

int main() {
    juce::ScopedJuceInitialiser_GUI juceInit; // required before any MessageManager/editor use (segfaults on macOS without it)
    setvbuf (stdout, nullptr, _IONBF, 0); // unbuffered: CI shows progress even if we crash later
    int fails = 0;
    DrumProcessor proc;
    proc.prepareToPlay (44100.0, 512);

    // float param plumbing sanity (pitch amount follows UI value)
    {
        auto* p = proc.apvts.getParameter ("p1_1"); // snare P DROP (0..25 %)
        p->setValueNotifyingHost (p->convertTo0to1 (20.f));
        flushMessages();
        proc.pullApvtsToParts();
        float got = proc.parts[Snare].params.p[1];
        bool ok = std::abs (got - 20.f) < 0.5f;
        printf ("e2e float param p1_1 -> %.2f %s\n", got, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    double sine = renderKickHF (proc, 0);
    double saw = renderKickHF (proc, 2);
    if (sine < 0 || saw < 0) { ++fails; }
    else {
        bool ok = saw > sine * 1.5;
        printf ("e2e UI wave sine vs saw: %.1f vs %.1f %s\n", sine, saw, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    // 3c. PHANTOM REGRESSION: old 6-child chain files must load with ZERO active FX
    {
        juce::ValueTree root ("DRUMSYNTH");
        root.setProperty ("kit", 1, nullptr); root.setProperty ("kitName", "Old", nullptr);
        root.setProperty ("master", 0.8f, nullptr); root.setProperty ("limiter", true, nullptr);
        root.setProperty ("selected", 0, nullptr);
        for (int p = 0; p < 12; ++p) {
            juce::ValueTree pv ("PART");
            for (int s = 0; s < 20; ++s) pv.setProperty ("p" + juce::String (s), 0.0, nullptr);
            pv.setProperty ("wave", 0, nullptr); pv.setProperty ("noiseType", 0, nullptr); pv.setProperty ("satMode", 0, nullptr);
            pv.setProperty ("level", 0.8, nullptr); pv.setProperty ("pan", 0.5, nullptr);
            pv.setProperty ("mute", false, nullptr); pv.setProperty ("solo", false, nullptr);
            pv.setProperty ("midi", 60 + p, nullptr);
            juce::ValueTree ch ("CHAIN"); // OLD format: only 6 children!
            for (int s = 0; s < 6; ++s) {
                juce::ValueTree fx ("FX");
                fx.setProperty ("type", 99, nullptr);
                fx.setProperty ("p0", 0.5, nullptr); fx.setProperty ("p1", 0.5, nullptr);
                fx.setProperty ("p2", 0.5, nullptr); fx.setProperty ("p3", 0.5, nullptr);
                fx.setProperty ("p4", 0.5, nullptr); fx.setProperty ("p5", 1.0, nullptr);
                fx.setProperty ("bypass", false, nullptr);
                ch.addChild (fx, -1, nullptr);
            }
            ch.setProperty ("order", "0,1,2,3,4,5", nullptr);
            ch.setProperty ("enabled", true, nullptr);
            pv.addChild (ch, -1, nullptr);
            root.addChild (pv, -1, nullptr);
        }
        juce::ValueTree g ("CHAIN");
        for (int s = 0; s < 6; ++s) {
            juce::ValueTree fx ("FX"); fx.setProperty ("type", 99, nullptr);
            fx.setProperty ("p0", 0.5, nullptr); fx.setProperty ("p1", 0.5, nullptr);
            fx.setProperty ("p2", 0.5, nullptr); fx.setProperty ("p3", 0.5, nullptr);
            fx.setProperty ("p4", 0.5, nullptr); fx.setProperty ("p5", 1.0, nullptr);
            fx.setProperty ("bypass", false, nullptr);
            g.addChild (fx, -1, nullptr);
        }
        g.setProperty ("order", "0,1,2,3,4,5", nullptr);
        g.setProperty ("enabled", true, nullptr);
        root.addChild (g, -1, nullptr);
        proc.loadFullStateVT (root);
        bool ok = true;
        for (int p = 0; p < 12; ++p)
            if (FxProcessor::numActive (proc.parts[p].fx) != 0) { ok = false; break; }
        if (FxProcessor::numActive (proc.globalFx) != 0) ok = false;
        printf ("e2e old-format chains load empty %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    // noise selector plumbing sanity (re-verify after state load)
    {
        auto* n = proc.apvts.getParameter ("p1_noise");
        n->setValueNotifyingHost (n->convertTo0to1 (3.f));
        flushMessages();
        proc.pullApvtsToParts();
        bool ok = proc.parts[Snare].params.noiseType == 3;
        printf ("e2e noise selector -> %d %s\n", proc.parts[Snare].params.noiseType, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    // UI-LOGIC: add ONE effect headlessly (same ops as the + ADD menu),
    // then verify slot counts, chip mapping, drag-reorder and type edits.
    {
        DrumEditor ed (proc); // headless: logic only, no peer
        auto* view = ed.partFxViewForTest();
        auto& ch = proc.parts[ed.selectedPartForTest()].fx;
        auto addFx = [&] (int type) {
            int free = -1;
            for (int i = 0; i < FxChainState::NumSlots; ++i)
                if (ch.slots[(size_t) i].type.load() == FX_NONE) { free = i; break; }
            if (free < 0) return -1;
            FxProcessor::defaultSlot (type, ch.slots[(size_t) free]);
            view->selectSlot (free);
            return free;
        };
        int s0 = addFx (FX_EQ);
        bool ok = (s0 >= 0) && (FxProcessor::numActive (ch) == 1) && (view->chips.size() == 1);
        printf ("e2e add-one-effect single active slot %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
        // add two more, then drag first chip down (light, like mouseDrag)
        int s1 = addFx (FX_DIST), s2 = addFx (FX_DELAY);
        ok = (FxProcessor::numActive (ch) == 3) && (view->chips.size() == 3);
        view->moveSlot (s0, +1);
        flushMessages();
        // mapping must survive the drag: every chip's slot must be active,
        // distinct, and clicking it must select exactly that slot's type
        {
            std::vector<int> seen;
            for (auto& c : view->chips) {
                view->selectSlot (c->slot);
                seen.push_back (c->slot);
                int t = ch.slots[(size_t) c->slot].type.load();
                if (t == FX_NONE || view->selected != c->slot) ok = false;
            }
            std::sort (seen.begin(), seen.end());
            if (seen != std::vector<int> ({ s0, s1, s2 })) ok = false;
        }
        // type edit via the same path as the detail typeBox must hit ONE slot
        {
            int before = FxProcessor::numActive (ch);
            view->selectSlot (s1);
            ch.slots[(size_t) s1].type.store (FX_COMP);
            view->refresh();
            int nEQ = 0;
            for (int i = 0; i < FxChainState::NumSlots; ++i)
                if (ch.slots[(size_t) i].type.load() == FX_EQ) ++nEQ;
            if (FxProcessor::numActive (ch) != before || nEQ != 1) ok = false;
        }
        printf ("e2e chip mapping survives drag + edits %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    // UI-LOGIC on INIT state: one ADD, real knob turn, audio still passes.
    // (The exact user flow: fresh INIT, + ADD EQ, turn LOW to 0.)
    {
        proc.initKit();
        flushMessages();
        bool chainsEmpty = true;
        for (int p = 0; p < 12; ++p)
            if (FxProcessor::numActive (proc.parts[p].fx) != 0) chainsEmpty = false;
        if (FxProcessor::numActive (proc.globalFx) != 0) chainsEmpty = false;
        printf ("e2e INIT chains empty %s\n", chainsEmpty ? "OK" : "FAIL");
        if (! chainsEmpty) ++fails;

        DrumEditor ed (proc);
        auto* view = ed.partFxViewForTest();
        auto& ch = proc.parts[ed.selectedPartForTest()].fx;
        int free = -1;
        for (int i = 0; i < FxChainState::NumSlots; ++i)
            if (ch.slots[(size_t) i].type.load() == FX_NONE) { free = i; break; }
        FxProcessor::defaultSlot (FX_EQ, ch.slots[(size_t) free]);
        view->selectSlot (free);
        bool oneChip = (FxProcessor::numActive (ch) == 1) && (view->chips.size() == 1);
        printf ("e2e INIT + ADD EQ single chip %s\n", oneChip ? "OK" : "FAIL");
        if (! oneChip) ++fails;

        // turn the LOW knob via the REAL slider object to minimum
        view->knobs[0]->s.setValue (0.0, juce::sendNotification);
        flushMessages();
        // exactly one atomic may have changed: slot[free].p0 == 0, rest 0.5
        bool isolated = (ch.slots[(size_t) free].p0.load() == 0.f);
        for (int i = 0; i < FxChainState::NumSlots && isolated; ++i) {
            auto& sl = ch.slots[(size_t) i];
            if (i == free) {
                if (sl.p1.load() != 0.5f || sl.p2.load() != 0.5f || sl.p3.load() != 0.5f
                    || sl.p4.load() != 0.5f || sl.p5.load() != 1.f) isolated = false;
            } else if (sl.type.load() != FX_NONE) isolated = false;
        }
        for (int p = 0; p < 12 && isolated; ++p) {
            if (p == ed.selectedPartForTest()) continue;
            if (FxProcessor::numActive (proc.parts[p].fx) != 0) isolated = false;
        }
        printf ("e2e knob write isolated to one slot %s\n", isolated ? "OK" : "FAIL");
        if (! isolated) ++fails;

        // audio through that chain: 2 kHz content must survive a cut LOW band
        {
            auto& dsp = proc.parts[ed.selectedPartForTest()].fxDsp[0];
            (void) dsp;
            double e = 0;
            for (int i = 0; i < 4000; ++i) {
                float x = 0.5f * std::sin (2.f * juce::MathConstants<float>::pi * 110.f * i / 44100.f)
                        + 0.5f * std::sin (2.f * juce::MathConstants<float>::pi * 2000.f * i / 44100.f);
                float L = x, R = x;
                FxProcessor::processChain (ch, proc.parts[ed.selectedPartForTest()].fxDsp, L, R, 44100.f);
                if (! std::isfinite (L)) { e = -1; break; }
                e += std::abs (L);
            }
            bool ok = e > 600.0; // highs pass: not muted
            printf ("e2e chain audio passes with LOW cut %.0f %s\n", e, ok ? "OK" : "FAIL");
            if (! ok) ++fails;
        }
    }

    // CHAOS: 3000 mixed UI ops with invariants after every op, then a MIDI
    // storm through processBlock. Catches state corruption no unit test sees.
    {
        DrumEditor ed (proc);
        juce::Random rng (12345);
        auto orderOk = [&] (FxChainState& ch) {
            bool seen[10] = {};
            for (int k = 0; k < 10; ++k) {
                int s = ch.order[(size_t) k];
                if (s < 0 || s >= 10 || seen[s]) return false;
                seen[s] = true;
            }
            return true;
        };
        auto activeSet = [&] (FxChainState& ch) {
            std::vector<int> v;
            bool seen[10] = {};
            for (int k = 0; k < 10; ++k) {
                int s = ch.order[(size_t) k];
                if (s < 0 || s >= 10 || seen[s]) continue;
                seen[s] = true;
                if (ch.slots[(size_t) s].type.load() != FX_NONE) v.push_back (s);
            }
            return v;
        };
        bool ok = true;
        for (int step = 0; step < 3000 && ok; ++step) {
            int op = rng.nextInt (12);
            int p = rng.nextInt (12);
            proc.selectedPart = p;
            auto* view = ed.partFxViewForTest();
            auto& ch = proc.parts[p].fx;
            switch (op) {
                case 0: { // ADD random type (menu body)
                    int free = -1;
                    for (int i = 0; i < 10; ++i)
                        if (ch.slots[(size_t) i].type.load() == FX_NONE) { free = i; break; }
                    if (free >= 0) {
                        FxProcessor::defaultSlot (rng.nextInt (8), ch.slots[(size_t) free]);
                        view->selectSlot (free);
                    }
                    break;
                }
                case 1: { // DEL selected
                    auto act = activeSet (ch);
                    if (! act.empty()) {
                        int s = act[(size_t) rng.nextInt ((int) act.size())];
                        FxProcessor::defaultSlot (FX_NONE, ch.slots[(size_t) s]);
                        view->selectSlot (s);
                    }
                    break;
                }
                case 2: { // drag-reorder (light) or button reorder
                    auto act = activeSet (ch);
                    if (act.size() > 1) {
                        int s = act[(size_t) rng.nextInt ((int) act.size())];
                        view->moveSlot (s, rng.nextInt (2) ? 1 : -1);
                    }
                    break;
                }
                case 3: { // turn a detail knob via the REAL slider
                    if (! view->knobs.empty() && view->selected >= 0)
                        view->knobs[(size_t) rng.nextInt (6)]->s.setValue (rng.nextFloat(), juce::sendNotification);
                    break;
                }
                case 4: { // retune selected slot type
                    auto act = activeSet (ch);
                    if (! act.empty()) {
                        int s = act[(size_t) rng.nextInt ((int) act.size())];
                        ch.slots[(size_t) s].type.store (rng.nextInt (8));
                        view->selectSlot (s);
                    }
                    break;
                }
                case 5: { // bypass / power toggles
                    auto act = activeSet (ch);
                    if (! act.empty() && rng.nextInt (2))
                        ch.slots[(size_t) act[(size_t) rng.nextInt ((int) act.size())]].bypass.store (rng.nextInt (2));
                    else if (rng.nextInt (2)) ch.enabled.store (rng.nextInt (2));
                    else proc.globalFx.enabled.store (rng.nextInt (2));
                    break;
                }
                case 6: { // S/M toggles (engine-side; UI resync covered by refresh path)
                    int q = rng.nextInt (12);
                    if (rng.nextInt (2)) proc.parts[q].solo = ! proc.parts[q].solo;
                    else proc.parts[q].mute = ! proc.parts[q].mute;
                    break;
                }
                case 7: { // kit ops
                    int k = rng.nextInt (4);
                    if (k == 0) proc.setCurrentProgram (1 + rng.nextInt (6));
                    else if (k == 1) proc.initKit();
                    else if (k == 2) proc.undo();
                    else proc.loadKit (1 + rng.nextInt (6));
                    break;
                }
                case 8: { // randomize / mutate
                    if (rng.nextInt (2)) proc.randomizePart (p, rng.nextFloat());
                    else proc.mutatePart (p, rng.nextFloat() * 0.5f);
                    break;
                }
                case 9: { // synth param via APVTS (like automation)
                    auto* par = proc.apvts.getParameter ("p" + juce::String (p) + "_" + juce::String (rng.nextInt (16)));
                    if (par) par->setValueNotifyingHost (rng.nextFloat());
                    break;
                }
                case 10: { // global chain ADD/DEL
                    auto& g = proc.globalFx;
                    if (rng.nextInt (2)) {
                        for (int i = 0; i < 10; ++i)
                            if (g.slots[(size_t) i].type.load() == FX_NONE) {
                                FxProcessor::defaultSlot (rng.nextInt (8), g.slots[(size_t) i]);
                                break;
                            }
                    } else {
                        auto act = activeSet (g);
                        if (! act.empty())
                            FxProcessor::defaultSlot (FX_NONE, g.slots[(size_t) act[0]]);
                    }
                    break;
                }
                default: { // switch selected part (view rebind)
                    proc.selectedPart = rng.nextInt (12);
                    break;
                }
            }
            ed.timerCallback(); // production sync path
            flushMessages();
            // INVARIANTS
            for (int q = 0; q < 12 && ok; ++q)
                if (! orderOk (proc.parts[q].fx)) {
                    juce::String o;
                    for (int k = 0; k < 10; ++k) o += juce::String (proc.parts[q].fx.order[(size_t) k]) + " ";
                    printf ("  chaos step %d op %d part %d: order corrupt [%s]\n", step, op, q, o.toRawUTF8());
                    ok = false;
                }
            if (ok && ! orderOk (proc.globalFx)) { printf ("  chaos step %d: global order corrupt\n", step); ok = false; }
            if (ok) {
                // view chips == engine active set for the selected part
                auto act = activeSet (proc.parts[ed.selectedPartForTest()].fx);
                auto& chips = ed.partFxViewForTest()->chips;
                if (act.size() != chips.size()) { printf ("  chaos step %d op %d: chip count %d vs active %d\n", step, op, (int) chips.size(), (int) act.size()); ok = false; }
                else {
                    for (auto& c : chips) {
                        int s = c->slot;
                        if (s < 0 || s >= 10 || proc.parts[ed.selectedPartForTest()].fx.slots[(size_t) s].type.load() == FX_NONE) {
                            printf ("  chaos step %d op %d: stale chip\n", step, op); ok = false; break;
                        }
                    }
                }
            }
            if (ok && step % 100 == 0) {
                // audio must stay finite through a MIDI storm (mixer cleared first)
                for (int q = 0; q < 12; ++q) { proc.parts[q].mute = false; proc.parts[q].solo = false; }
                juce::MidiBuffer midi;
                for (int q = 0; q < 12; ++q) midi.addEvent (juce::MidiMessage::noteOn (1, partMidiNote (q), 0.9f), q * 10);
                juce::AudioBuffer<float> buf (2, 512);
                double e = 0;
                for (int b = 0; b < 60; ++b) {
                    proc.processBlock (buf, midi);
                    midi.clear();
                    for (int i = 0; i < buf.getNumSamples(); ++i) {
                        float l = buf.getSample (0, i), r = buf.getSample (1, i);
                        if (! std::isfinite (l) || ! std::isfinite (r)) { ok = false; break; }
                        e += std::abs (l) + std::abs (r);
                    }
                    if (! ok) break;
                }
                if (ok && e < 1.0) { printf ("  chaos step %d: storm silent (%.1f)\n", step, e); ok = false; }
            }
        }
        printf ("e2e chaos 3000 ops %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    // FX SEND: a snare with SEND REV leaves a reverb tail after the voice is
    // gone; SEND 0 leaves silence. Saved + reloaded with the kit state.
    {
        auto tailAfterHit = [&] (float send) {
            proc.initKitNoUndo();
            for (auto& pe : proc.parts) for (auto& v : pe.voices) v.active = false; // earlier tests may leave held notes
            proc.parts[Snare].sendRev = 0.f;
            juce::AudioBuffer<float> b (2, 512);
            for (int blk = 0; blk < 450; ++blk) { b.clear(); juce::MidiBuffer none; proc.processBlock (b, none); } // flush tails
            proc.parts[Snare].sendRev = send;
            proc.revDecay = 0.8f;
            double tail = 0;
            for (int blk = 0; blk < 70; ++blk) { // ~0.8 s
                b.clear();
                juce::MidiBuffer mb;
                if (blk == 0) mb.addEvent (juce::MidiMessage::noteOn (1, partMidiNote (Snare), 0.9f), 0);
                proc.processBlock (b, mb);
                if (blk >= 26 && blk < 70) for (int i = 0; i < 512; ++i) tail += std::abs (b.getSample (0, i)); // 0.3..0.8 s: dry snare is gone
            }
            return tail;
        };
        double dry = tailAfterHit (0.f), wet = tailAfterHit (0.6f);
        proc.parts[Snare].sendRev = 0.42f;
        auto vt = proc.fullStateVT();
        proc.parts[Snare].sendRev = 0.f;
        proc.loadFullStateVT (vt);
        bool saved = std::abs (proc.parts[Snare].sendRev.load() - 0.42f) < 1e-4f;
        bool ok = wet > dry * 5.0 + 0.5 && saved;
        printf ("e2e send reverb tail dry %.2f wet %.2f saved %d %s\n", dry, wet, (int) saved, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    // RANDOMIZE LOCKS: locked knobs / WAVE / send / FX chain survive every
    // randomize + mutate path; unlocked ones move; locks survive a kit change
    // and a session save/restore
    {
        proc.loadKit (1);
        for (int p = 0; p < NumParts; ++p) for (int k = 0; k < DrumProcessor::NumLocks; ++k) proc.setLocked (p, k, false);
        int lockedSlots[] = { 0, 8, 9, 16 }; // snare TUNE, BODY, NOISE, PUNCH
        for (int s : lockedSlots) proc.setLocked (Snare, s, true);
        proc.setLocked (Snare, DrumProcessor::LockWave, true);
        proc.setLocked (Snare, DrumProcessor::LockSendRev, true);
        proc.setLocked (Snare, DrumProcessor::LockFx, true);
        proc.parts[Snare].sendRev = 0.37f;
        auto before = proc.parts[Snare].params;
        auto fxBefore = proc.parts[Snare].fx.toVT();
        bool held = true, moved = false;
        for (int round = 0; round < 25; ++round) {
            switch (round % 4) {
                case 0: proc.randomizeKit (1.f); break;
                case 1: proc.randomizePart (Snare, 1.f); break;
                case 2: proc.mutatePart (Snare, 0.5f); break;
                default: proc.mutateKit (0.5f); break;
            }
            flushMessages();
            auto& now = proc.parts[Snare].params;
            for (int s : lockedSlots) held &= std::abs (now.p[s] - before.p[s]) < 1e-4f;
            held &= now.wave == before.wave;
            held &= std::abs (proc.parts[Snare].sendRev.load() - 0.37f) < 1e-6f;
            held &= proc.parts[Snare].fx.toVT().isEquivalentTo (fxBefore);
            moved |= std::abs (now.p[11] - before.p[11]) > 1e-3f; // TONE is unlocked
        }
        // locks survive a kit change and a session round-trip
        proc.loadKit (5);
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        for (int k = 0; k < DrumProcessor::NumLocks; ++k) proc.setLocked (Snare, k, false);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        bool persisted = proc.isLocked (Snare, 8) && proc.isLocked (Snare, DrumProcessor::LockFx)
                      && proc.isLocked (Snare, DrumProcessor::LockWave) && ! proc.isLocked (Snare, 11);
        bool ok = held && moved && persisted;
        printf ("e2e randomize locks held %d unlocked-moved %d persisted %d %s\n", (int) held, (int) moved, (int) persisted, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
        for (int p = 0; p < NumParts; ++p) for (int k = 0; k < DrumProcessor::NumLocks; ++k) proc.setLocked (p, k, false);
    }

    // ===== engine MODELS / REVERSE / MULTI-OUT =====
    {
        proc.initKitNoUndo();
        for (int p = 0; p < NumParts; ++p) proc.parts[p].fx.enabled = false;
        proc.parts[0].sendRev = proc.parts[0].sendDly = 0.f;
        // every model of every part: finite, audible, and roughly the level of CLASSIC
        int bad = 0, total = 0;
        for (int p = 0; p < NumParts; ++p) {
            proc.parts[p].sendRev = 0.f; proc.parts[p].sendDly = 0.f;
            float classic = 0.f;
            for (int m = 0; m < numModels (p); ++m) {
                proc.setPartModel (p, m);
                flushMessages(); silenceProc (proc);
                bool fin = true;
                auto b = renderProc (proc, p, 44100 * 2);
                float pk = chanPeak (b, 0, 1, &fin);
                if (m == 0) classic = pk;
                bool ok = fin && pk > 1e-3f && pk < 8.f && (m == 0 || (pk > classic * 0.25f && pk < classic * 4.f));
                if (! ok) { ++bad; printf ("  model %s/%s peak %.3f (classic %.3f) finite %d\n", partName (p), modelNames (p)[m].toRawUTF8(), pk, classic, (int) fin); }
                ++total;
            }
            proc.setPartModel (p, 0);
        }
        printf ("e2e models render (%d models) bad %d %s\n", total, bad, bad == 0 ? "OK" : "FAIL");
        if (bad) ++fails;
    }
    {
        // TRIGGER button path (audition request handled on the audio thread)
        silenceProc (proc);
        proc.setPartModel (Snare, 2);
        proc.auditionReq = Snare;
        auto b = renderProc (proc, -1, 22050);
        bool ok = chanPeak (b, 0, 1) > 1e-3f && proc.auditionReq.load() == -1;
        printf ("e2e audition request plays %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    {
        // RANDOMIZE may switch the model; MUTATE never does; neither moves envelope knobs / VEL>AMP; LockModel holds all
        proc.setPartModel (Snare, 1);
        std::set<int> seen;
        bool envHeld = true;
        for (int i = 0; i < 40; ++i) {
            int before = proc.parts[Snare].params.model; float bm[NumModelParams];
            std::copy (proc.parts[Snare].params.m, proc.parts[Snare].params.m + NumModelParams, bm);
            proc.randomizePart (Snare, 1.f); flushMessages();
            auto& pp = proc.parts[Snare].params;
            seen.insert (pp.model);
            if (auto* md = modelDef (Snare, pp.model)) {
                float def[NumModelParams]; modelDefaults01 (Snare, pp.model, def);
                for (int k = 0; k < md->nparams; ++k)
                    if (modelParamIsEnvelope (md->p[k]))
                        envHeld &= std::abs (pp.m[k] - (pp.model == before ? bm[k] : def[k])) < 1e-5f;
            }
        }
        proc.setPartModel (Snare, 3);
        bool mutKeeps = true, mutMoves = false;
        for (int i = 0; i < 30; ++i) {
            float bm[NumModelParams]; std::copy (proc.parts[Snare].params.m, proc.parts[Snare].params.m + NumModelParams, bm);
            if (i % 2) proc.mutatePart (Snare, 0.5f); else proc.mutateKit (0.5f);
            flushMessages();
            auto& pp = proc.parts[Snare].params;
            mutKeeps &= pp.model == 3;
            auto* md = modelDef (Snare, 3);
            for (int k = 0; k < md->nparams; ++k) {
                if (modelParamIsEnvelope (md->p[k])) envHeld &= std::abs (pp.m[k] - bm[k]) < 1e-5f;
                else mutMoves |= std::abs (pp.m[k] - bm[k]) > 1e-4f;
            }
        }
        proc.setLocked (Snare, DrumProcessor::LockModel, true);
        auto locked = proc.parts[Snare].params;
        bool lockHeld = true;
        for (int i = 0; i < 20; ++i) {
            if (i % 2) proc.randomizeKit (1.f); else proc.randomizePart (Snare, 1.f);
            proc.mutatePart (Snare, 0.5f); flushMessages();
            lockHeld &= proc.parts[Snare].params.model == locked.model;
            for (int k = 0; k < NumModelParams; ++k) lockHeld &= std::abs (proc.parts[Snare].params.m[k] - locked.m[k]) < 1e-5f;
        }
        proc.setLocked (Snare, DrumProcessor::LockModel, false);
        bool ok = seen.size() >= 3 && envHeld && mutKeeps && mutMoves && lockHeld;
        printf ("e2e model randomize: models seen %d env/vel held %d mutate keeps model %d moves knobs %d lock %d %s\n",
                (int) seen.size(), (int) envHeld, (int) mutKeeps, (int) mutMoves, (int) lockHeld, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    {
        // state round-trip: model, knobs, reverse, output routing; INIT resets them
        proc.setPartModel (Tom1, 2);
        if (auto* q = proc.apvts.getParameter (DrumProcessor::modelParamID (Tom1, 1))) q->setValueNotifyingHost (0.77f);
        if (auto* q = proc.apvts.getParameter ("p6_rev")) q->setValueNotifyingHost (1.f);
        proc.parts[Tom1].output = 4;
        flushMessages(); proc.pullApvtsToParts();
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        proc.initKitNoUndo(); flushMessages(); proc.pullApvtsToParts();
        bool cleared = proc.parts[Tom1].params.model == 0 && ! proc.parts[Tom1].params.reverse && proc.parts[Tom1].output.load() == 0;
        proc.setStateInformation (mb.getData(), (int) mb.getSize()); flushMessages(); proc.pullApvtsToParts();
        auto& pp = proc.parts[Tom1].params;
        bool back = pp.model == 2 && std::abs (pp.m[1] - 0.77f) < 1e-3f && pp.reverse && proc.parts[Tom1].output.load() == 4;
        // kit preset round-trip too (toVT/fromVT)
        auto vt = pp.toVT(); PartParams q2; q2.fromVT (vt);
        bool kitRt = q2.model == 2 && std::abs (q2.m[1] - 0.77f) < 1e-3f && q2.reverse;
        bool ok = cleared && back && kitRt;
        printf ("e2e model/reverse/out state: init clears %d restore %d kit vt %d %s\n", (int) cleared, (int) back, (int) kitRt, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
        proc.initKitNoUndo(); flushMessages();
    }
    {
        // REVERSE: the energy moves to the end (works for CLASSIC and a model)
        bool ok = true;
        for (int model : { 0, 1 }) {
            proc.setPartModel (Snare, model);
            if (auto* q = proc.apvts.getParameter ("p1_rev")) q->setValueNotifyingHost (0.f);
            flushMessages(); silenceProc (proc);
            auto fw = renderProc (proc, Snare, 44100);
            if (auto* q = proc.apvts.getParameter ("p1_rev")) q->setValueNotifyingHost (1.f);
            flushMessages(); proc.pullApvtsToParts(); proc.updateReverseCaches(); silenceProc (proc);
            auto rv = renderProc (proc, Snare, 44100);
            auto centroid = [] (const juce::AudioBuffer<float>& b) {
                double e = 0, et = 0;
                for (int i = 0; i < b.getNumSamples(); ++i) { double v = b.getSample (0, i); e += v * v; et += v * v * i; }
                return e > 0 ? et / e / 44100.0 : -1.0;
            };
            int pkF = 0, pkR = 0; float mf = 0, mr = 0;
            for (int i = 0; i < 44100; ++i) {
                if (std::abs (fw.getSample (0, i)) > mf) { mf = std::abs (fw.getSample (0, i)); pkF = i; }
                if (std::abs (rv.getSample (0, i)) > mr) { mr = std::abs (rv.getSample (0, i)); pkR = i; }
            }
            double cf = centroid (fw), cr = centroid (rv);
            bool good = mr > 1e-3f && pkR > pkF + 2000 && cr > cf;
            printf ("  reverse model %d: peak %d -> %d samples, centroid %.3f -> %.3f s\n", model, pkF, pkR, cf, cr);
            ok &= good;
        }
        if (auto* q = proc.apvts.getParameter ("p1_rev")) q->setValueNotifyingHost (0.f);
        proc.setPartModel (Snare, 0); flushMessages();
        printf ("e2e reverse plays backwards %s\n", ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    {
        // MULTI-OUT: kick on OUT 3 lands on that bus only; with the bus disabled it falls back to MAIN
        auto layout = proc.getBusesLayout();
        for (int i = 1; i < layout.outputBuses.size(); ++i) layout.outputBuses.getReference (i) = juce::AudioChannelSet::stereo();
        proc.releaseResources();
        bool set = proc.setBusesLayout (layout);
        proc.prepareToPlay (44100.0, 512);
        proc.parts[Kick].output = 2;
        silenceProc (proc);
        auto b = renderProc (proc, Kick, 22050);
        float mainPk = chanPeak (b, 0, 1), auxPk = chanPeak (b, 4, 5), otherPk = chanPeak (b, 2, 3) + chanPeak (b, 6, b.getNumChannels() - 1);
        bool routed = set && b.getNumChannels() == 2 * (1 + DrumProcessor::NumAuxOuts) && auxPk > 1e-3f && mainPk < 1e-5f && otherPk < 1e-5f;
        auto layout2 = proc.getBusesLayout();
        for (int i = 1; i < layout2.outputBuses.size(); ++i) layout2.outputBuses.getReference (i) = juce::AudioChannelSet::disabled();
        proc.releaseResources();
        proc.setBusesLayout (layout2);
        proc.prepareToPlay (44100.0, 512);
        silenceProc (proc);
        auto b2 = renderProc (proc, Kick, 22050);
        bool fallback = b2.getNumChannels() == 2 && chanPeak (b2, 0, 1) > 1e-3f;
        proc.parts[Kick].output = 0;
        bool ok = routed && fallback;
        printf ("e2e multi-out: aux %.3f main %.6f others %.6f routed %d, disabled-bus fallback %d %s\n",
                auxPk, mainPk, otherPk, (int) routed, (int) fallback, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }
    {
        // model kits load from their data files: models + REVERSE restored, reversed parts play backwards on the first hit
        // read the repo's freshly minted data files, never an installed copy
        auto repo = juce::File (__FILE__).getParentDirectory();
        auto shadow = juce::File::createTempFile ("factory_shadow"); shadow.createDirectory();
        setFactoryDirsForTest (repo.getChildFile ("Factory"), shadow);
        rescanFactoryPresets();
        proc.loadKit (74);
        flushMessages();
        bool loaded = proc.kitName == "Mirror Room" && proc.parts[Snare].params.model == 1 && proc.parts[Crash].params.model == 1
                   && proc.parts[Crash].params.reverse && ! proc.parts[Kick].params.reverse;
        auto b = renderProc (proc, Crash, 44100 * 3);
        int pk = 0; float mx = 0;
        for (int i = 0; i < b.getNumSamples(); ++i) if (std::abs (b.getSample (0, i)) > mx) { mx = std::abs (b.getSample (0, i)); pk = i; }
        bool firstHitReversed = mx > 1e-3f && pk > 44100;   // a forward crash peaks in the first ms
        int modelParts = 0;
        for (int k = 65; k <= numFactoryKits(); ++k) {
            proc.loadKit (k);
            for (int p = 0; p < NumParts; ++p) modelParts += proc.parts[p].params.model > 0 ? 1 : 0;
        }
        bool ok = loaded && firstHitReversed && numFactoryKits() == 80 && modelParts > 100;
        printf ("e2e model kits: Mirror Room loaded %d, first reversed crash peaks at %.2f s, %d model parts in kits 65-80 %s\n",
                (int) loaded, pk / 44100.0, modelParts, ok ? "OK" : "FAIL");
        if (! ok) ++fails;
    }

    printf (fails == 0 ? "ALL E2E TESTS PASSED\n" : "FAILURES: %d\n", fails);
    return fails == 0 ? 0 : 1;
}
