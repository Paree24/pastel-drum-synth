#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mds {

juce::AudioProcessor::BusesProperties DrumProcessor::makeBuses() {
    auto b = BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < NumAuxOuts; ++i)
        b = b.withOutput ("Out " + juce::String (i + 2), juce::AudioChannelSet::stereo(), false);
    return b;
}

bool DrumProcessor::isBusesLayoutSupported (const BusesLayout& l) const {
    auto main = l.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo() && main != juce::AudioChannelSet::mono()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i) {
        const auto& s = l.outputBuses.getReference (i);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

DrumProcessor::~DrumProcessor() { stopTimer(); }

DrumProcessor::DrumProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", makeLayout()) {
    for (int p = 0; p < NumParts; ++p) {
        defaultParamsForPart (p, parts[p].params);
        parts[p].midiNote = partMidiNote (p);
        parts[p].level = 0.8f; parts[p].pan = 0.5f;
        FxProcessor::clearChain (parts[p].fx); // no effects by default; user adds them
    }
    FxProcessor::clearChain (globalFx);
    loadKit (1);
    startTimerHz (10);
}

juce::AudioProcessorValueTreeState::ParameterLayout DrumProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int p = 0; p < NumParts; ++p) {
        const ParamMeta* m = metaForPart (p);
        for (int s = 0; s < 16; ++s) {
            auto id = synthParamID (p, s);
            auto nm = juce::String (partName (p)) + " " + m[s].label;
            juce::NormalisableRange<float> range (m[s].min, m[s].max);
            if (juce::String (m[s].fmt) == "Hz") range.setSkewForCentre (std::sqrt (m[s].min * m[s].max)); // log feel
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (id, 1), nm, range, m[s].def));
        }
        // advanced slots 16..19 (per-part extras: PUNCH/BOOST/NZ>FX, SFX LFO...)
        for (int s = 16; s < 20; ++s)
            if (auto* am = advMetaForPart (p, s)) {
                juce::NormalisableRange<float> range (am->min, am->max);
                if (juce::String (am->fmt) == "Hz") range.setSkewForCentre (std::sqrt (am->min * am->max));
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID (synthParamID (p, s), 1),
                    juce::String (partName (p)) + " " + am->label, range, am->def));
            }
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ("p" + juce::String (p) + "_wave", 1),
            juce::String (partName (p)) + " Wave",
            juce::StringArray ({ "Sine","Tri","Saw","Square","FM" }), 0));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ("p" + juce::String (p) + "_noise", 1),
            juce::String (partName (p)) + " Noise",
            juce::StringArray ({ "White","Pink","Violet","Metallic","Digital","Crackle","Tape","Resonant" }),
            p <= 5 ? 2 : 0));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("mix" + juce::String (p) + "_lvl", 1),
            juce::String (partName (p)) + " Level",
            juce::NormalisableRange<float> (0.f, 1.2f), 0.8f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("mix" + juce::String (p) + "_pan", 1),
            juce::String (partName (p)) + " Pan",
            juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID ("tune" + juce::String (p), 1),
            juce::String (partName (p)) + " Tune",
            juce::NormalisableRange<float> (-24.f, 24.f), 0.f));
        // engine MODEL + its 16 generic 0..1 knobs (meaning depends on the model) + REVERSE
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID ("p" + juce::String (p) + "_model", 1), juce::String (partName (p)) + " Model", modelNames (p), 0));
        for (int k = 0; k < NumModelParams; ++k)
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (modelParamID (p, k), 1), juce::String (partName (p)) + " Model " + juce::String (k + 1),
                juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID ("p" + juce::String (p) + "_rev", 1), juce::String (partName (p)) + " Reverse", false));
    }
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID ("master", 1), "Master",
        juce::NormalisableRange<float> (0.f, 1.2f), 0.8f));
    return layout;
}

void DrumProcessor::prepareToPlay (double sr, int block) {
    sampleRate = sr;
    tailLenSamples = (int) (sr * 4.0); // 4 s tail budget for delay/reverb ring-out
    spec.sampleRate = sr; spec.maximumBlockSize = (juce::uint32) block; spec.numChannels = 2;
    for (int p = 0; p < NumParts; ++p) {
        for (auto& v : parts[p].voices) v.prepare ((float) sr);
        FxProcessor::prepareAll (parts[p].fxDsp, FxChainState::NumSlots, spec);
    }
    FxProcessor::prepareAll (globalDsp, FxChainState::NumSlots, spec);
    FxProcessor::prepareAll (globalDspR, FxChainState::NumSlots, spec);
    sendRevDsp.prepare (spec); sendDlyDsp.prepare (spec);
    pullApvtsToParts();
}

void DrumProcessor::pullApvtsToParts() {
    for (int p = 0; p < NumParts; ++p) {
        for (int s = 0; s < 16; ++s)
            if (auto* par = apvts.getRawParameterValue (synthParamID (p, s)))
                parts[p].params.p[s] = par->load();
        for (int s = 16; s < 20; ++s)
            if (advMetaForPart (p, s))
                if (auto* par = apvts.getRawParameterValue (synthParamID (p, s)))
                    parts[p].params.p[s] = par->load();
        // lock-free reads of choice params. NOTE: choice raw values are
        // DENORMALISED indices (not 0..1) - do not rescale them.
        if (auto* w = apvts.getRawParameterValue ("p" + juce::String (p) + "_wave"))
            parts[p].params.wave = juce::jlimit (0, 4, (int) std::round (w->load()));
        if (auto* n = apvts.getRawParameterValue ("p" + juce::String (p) + "_noise"))
            parts[p].params.noiseType = juce::jlimit (0, 7, (int) std::round (n->load()));
        parts[p].params.satMode = 0; // fixed Soft: DRIVE knob + DISTORTION carry the grit
        if (auto* l = apvts.getRawParameterValue ("mix" + juce::String (p) + "_lvl")) parts[p].level = l->load();
        if (auto* pn = apvts.getRawParameterValue ("mix" + juce::String (p) + "_pan")) parts[p].pan = pn->load();
        readPartFromApvts (p, parts[p].params);
    }
    if (auto* m = apvts.getRawParameterValue ("master")) masterGain = m->load();
}

void DrumProcessor::readPartFromApvts (int p, PartParams& out) const {
    if (auto* md = apvts.getRawParameterValue ("p" + juce::String (p) + "_model"))
        out.model = juce::jlimit (0, numModels (p) - 1, (int) std::round (md->load()));
    for (int k = 0; k < NumModelParams; ++k)
        if (auto* mp = apvts.getRawParameterValue (modelParamID (p, k))) out.m[k] = mp->load();
    if (auto* rv = apvts.getRawParameterValue ("p" + juce::String (p) + "_rev")) out.reverse = rv->load() > 0.5f;
}

void DrumProcessor::triggerPart (int part, float vel) {
    // choke: hats share group
    int g = chokeGroup[part];
    // CHOKE (hats): 0 = lets the other hat ring, 100 = cuts it in 5 ms
    float chokeAmt = (part == ClosedHat || part == OpenHat) ? parts[part].params.p[14] / 100.f : 1.f;
    if (g != 0 && chokeAmt > 0.005f)
        for (int p = 0; p < NumParts; ++p) if (p != part && chokeGroup[p] == g)
            for (auto& v : parts[p].voices) if (v.active) v.chokeNow (80.f - 75.f * chokeAmt);
    float tuneOff = 0.f;
    if (auto* t = apvts.getRawParameterValue ("tune" + juce::String (part))) tuneOff = t->load();
    // find free voice
    DrumVoice* tgt = nullptr;
    for (auto& v : parts[part].voices) if (! v.active) { tgt = &v; break; }
    if (! tgt) tgt = &parts[part].voices[0]; // steal oldest-ish
    if (parts[part].params.reverse) {
        RevBuf::Ptr b;
        { const juce::SpinLock::ScopedLockType sl (parts[part].revLock); b = parts[part].revCur; }
        if (b != nullptr) { tgt->startReverse (part, b, 0.3f + 0.7f * juce::jlimit (0.f, 1.f, vel)); return; }
    }
    tgt->trigger (part, parts[part].params, vel, tuneOff);
}

void DrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    pullApvtsToParts(); // cheap pointer reads; no allocation
    if (int a = auditionReq.exchange (-1); a >= 0 && a < NumParts) triggerPart (a, 0.9f);

    for (const auto md : midi) {
        auto msg = md.getMessage();
        if (msg.isNoteOn()) {
            float vel = msg.getFloatVelocity();
            int note = msg.getNoteNumber();
            for (int p = 0; p < NumParts; ++p)
                if (parts[p].midiNote == note && ! parts[p].mute) triggerPart (p, vel);
        } else if (msg.isNoteOff()) {
            int note = msg.getNoteNumber();
            for (int p = 0; p < NumParts; ++p)
                if (parts[p].midiNote == note)
                    for (auto& v : parts[p].voices) v.noteOff();
        }
    }

    int N = buffer.getNumSamples();
    // multi-out: Main + enabled aux pairs; a part routed to a disabled bus plays on Main
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, N);
    float* auxL[NumAuxOuts + 1] = {};
    float* auxR[NumAuxOuts + 1] = {};
    for (int b = 1; b < juce::jmin (getBusCount (false), NumAuxOuts + 1); ++b) {
        auto* bus = getBus (false, b);
        if (bus == nullptr || ! bus->isEnabled()) continue;
        auto bb = getBusBuffer (buffer, false, b);
        if (bb.getNumChannels() == 0) continue;
        auxL[b] = bb.getWritePointer (0);
        auxR[b] = bb.getNumChannels() > 1 ? bb.getWritePointer (1) : nullptr;
    }
    int route[NumParts];
    for (int p = 0; p < NumParts; ++p) {
        int o = parts[p].output.load();
        route[p] = (o > 0 && o <= NumAuxOuts && auxL[o] != nullptr) ? o : 0;
    }
    const float auxGain = masterGain.load();
    auto mainBuf = getBusBuffer (buffer, false, 0);
    auto* L = mainBuf.getWritePointer (0);
    auto* R = mainBuf.getNumChannels() > 1 ? mainBuf.getWritePointer (1) : nullptr;
    float sr = (float) sampleRate;

    for (int i = 0; i < N; ++i) {
        float mixL = 0.f, mixR = 0.f, revIn = 0.f, dlyIn = 0.f;
        bool anySolo = false;
        for (int p = 0; p < NumParts; ++p) if (parts[p].solo) { anySolo = true; break; }
        for (int p = 0; p < NumParts; ++p) {
            auto& pe = parts[p];
            if (pe.mute || (anySolo && ! pe.solo)) { pe.tail = 0; continue; }
            bool va = false;
            float sig = 0.f, post = 0.f; // post: layers that skip the part FX (snare noise)
            for (auto& v : pe.voices) if (v.active) {
                va = true;
                float s = v.renderWithParams (pe.params);
                sig += s - v.postFx; post += v.postFx;
            }
            if (va) pe.tail = tailLenSamples;
            // NOTE: the chain keeps running on tail after voices end so that
            // delay/reverb ring out instead of being gated by the amp envelope.
            if (! va && pe.tail <= 0) { pe.meter *= 0.9995f; continue; }
            {
                float m = sig;
                if (pe.fx.enabled.load())
                    for (int k = 0; k < FxChainState::NumSlots; ++k) {
                        int idx = pe.fx.order[(size_t) k];
                        m = FxProcessor::processSlot (pe.fx.slots[(size_t) idx], pe.fxDsp[idx], m, sr);
                    }
                m += post; // clean layer joins after the part chain
                // capture post-FX waveform for scope/analyzer (freezes when tail ends)
                pe.scopeBuf[(size_t) (pe.scopePos.load() % 2048)] = m;
                ++pe.scopePos;
                float pan = juce::jlimit (0.f, 1.f, pe.pan);
                float gl = std::cos (pan * juce::MathConstants<float>::halfPi);
                float gr = std::sin (pan * juce::MathConstants<float>::halfPi);
                if (route[p] == 0) {
                    mixL += m * gl * pe.level;
                    mixR += m * gr * pe.level;
                } else {
                    float ol = m * gl * pe.level * auxGain, orr = m * gr * pe.level * auxGain;
                    if (! std::isfinite (ol) || ! std::isfinite (orr)) ol = orr = 0.f;
                    if (auxR[route[p]]) { auxL[route[p]][i] += ol; auxR[route[p]][i] += orr; }
                    else auxL[route[p]][i] += (ol + orr) * 0.5f;
                }
                revIn += m * pe.level * pe.sendRev.load();
                dlyIn += m * pe.level * pe.sendDly.load();
                if (va) pe.meter = juce::jmax (pe.meter * 0.9995f, juce::jmin (1.f, std::abs (m * pe.level)));
                else { pe.meter *= 0.9995f; --pe.tail; }
            }
        }
        bool anySound = false;
        for (int p = 0; p < NumParts; ++p)
            if (parts[p].tail > 0) { anySound = true; break; }
        // (voices set tail above; anySound covers sounding + ringing parts)
        // shared send returns ring out on their own tail after the last voice
        if (anySound) sendTail = tailLenSamples;
        if (anySound || sendTail > 0) {
            // stereo returns: decorrelated L/R reverb, ping-pong delay
            float dL = 0.f, dR = 0.f;
            FxProcessor::sendDelayStereo (sendDlyDsp, dlyIn, dlyTime.load(), dlyFb.load(), sr, dL, dR);
            float rL = 0.f, rR = 0.f;
            FxProcessor::sendReverbStereo (sendRevDsp, revIn, revDecay.load(), revDamp.load(), rL, rR);
            mixL += rL + dL;
            mixR += rR + dR;
            if (! anySound) --sendTail;
        }
        float gL = mixL, gR = mixR;
        // global chain runs per channel (own DSP state for L and R): pans survive
        if (anySound) globalTail = tailLenSamples;
        if ((anySound || globalTail > 0) && globalFx.enabled.load()) {
            for (int k = 0; k < FxChainState::NumSlots; ++k) {
                int idx = globalFx.order[(size_t) k];
                gL = FxProcessor::processSlot (globalFx.slots[(size_t) idx], globalDsp[idx], gL, sr);
                gR = FxProcessor::processSlot (globalFx.slots[(size_t) idx], globalDspR[idx], gR, sr);
            }
            if (! anySound) --globalTail;
        }
        float mg = masterGain.load();
        if (! std::isfinite (mg)) { mg = 0.8f; masterGain.store (mg); }
        gL *= mg; gR *= mg;
        // safety limiter (limEnv guarded: a stuck inf/NaN would mute forever)
        if (! std::isfinite (limEnv)) limEnv = 0.f;
        if (limiterOn) {
            float peak = juce::jmax (std::abs (gL), std::abs (gR));
            if (! std::isfinite (peak)) peak = 0.f;
            limEnv = juce::jmax (peak, limEnv * 0.9995f);
            float thr = 1.f;
            if (limEnv > thr) { float g = thr / limEnv; gL *= g; gR *= g; }
            gL = std::tanh (gL * 1.05f) * 0.95f;
            gR = std::tanh (gR * 1.05f) * 0.95f;
        }
        // final safety: never emit non-finite samples (a NaN stream reads as "muted")
        if (! std::isfinite (gL)) gL = 0.f;
        if (! std::isfinite (gR)) gR = 0.f;
        if (R) { L[i] = gL; R[i] = gR; }
        else L[i] = (gL + gR) * 0.5f; // mono bus: fold down
        outLevel = juce::jmax (outLevel.load() * 0.9995f, juce::jmin (1.f, (std::abs (gL) + std::abs (gR)) * 0.5f));
    }
}

// ---- kits / state ----
const juce::String DrumProcessor::getProgramName (int i) { return presetName (i); }

void DrumProcessor::applyKitToApvts() {
    for (int p = 0; p < NumParts; ++p) {
        for (int s = 0; s < 16; ++s)
            if (auto* par = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (synthParamID (p, s))))
                par->setValueNotifyingHost (par->convertTo0to1 (parts[p].params.p[s]));
        for (int s = 16; s < 20; ++s)
            if (advMetaForPart (p, s))
                if (auto* par = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (synthParamID (p, s))))
                    par->setValueNotifyingHost (par->convertTo0to1 (parts[p].params.p[s]));
        if (auto* w = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("p" + juce::String (p) + "_wave")))
            w->setValueNotifyingHost (w->convertTo0to1 (parts[p].params.wave));
        if (auto* n = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("p" + juce::String (p) + "_noise")))
            n->setValueNotifyingHost (n->convertTo0to1 (parts[p].params.noiseType));
        if (auto* l = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("mix" + juce::String (p) + "_lvl")))
            l->setValueNotifyingHost (l->convertTo0to1 (parts[p].level));
        if (auto* pn = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("mix" + juce::String (p) + "_pan")))
            pn->setValueNotifyingHost (pn->convertTo0to1 (parts[p].pan));
        if (auto* t = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("tune" + juce::String (p))))
            t->setValueNotifyingHost (t->convertTo0to1 (0.f));
        if (auto* md = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("p" + juce::String (p) + "_model")))
            md->setValueNotifyingHost (md->convertTo0to1 ((float) juce::jlimit (0, numModels (p) - 1, parts[p].params.model)));
        for (int k = 0; k < NumModelParams; ++k)
            if (auto* mp = apvts.getParameter (modelParamID (p, k))) mp->setValueNotifyingHost (juce::jlimit (0.f, 1.f, parts[p].params.m[k]));
        if (auto* rv = apvts.getParameter ("p" + juce::String (p) + "_rev")) rv->setValueNotifyingHost (parts[p].params.reverse ? 1.f : 0.f);
    }
    if (auto* m = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("master")))
        m->setValueNotifyingHost (m->convertTo0to1 (masterGain.load()));
}

void DrumProcessor::loadKit (int idx) {
    pushUndo();
    if (applyPreset (*this, idx)) return; // currentKit/kitName set by applyPreset
    initKitNoUndo();
}

void DrumProcessor::initKit() {
    pushUndo();
    initKitNoUndo();
}

void DrumProcessor::initKitNoUndo() {
    // blank slate: default voices, empty FX chains (no preset involved)
    currentKit = -1;
    kitName = "Init";
    for (int p = 0; p < NumParts; ++p) {
        defaultParamsForPart (p, parts[p].params);
        parts[p].level = 0.8f; parts[p].pan = 0.5f;
        parts[p].mute = false; parts[p].solo = false; // no stuck solos on a blank slate
        parts[p].sendRev = 0.f; parts[p].sendDly = 0.f;
        parts[p].midiNote = partMidiNote (p);
        parts[p].output = 0;
        FxProcessor::clearChain (parts[p].fx);
    }
    FxProcessor::clearChain (globalFx);
    masterGain = 0.8f;
    applyKitToApvts();
    pullApvtsToParts();
    ++stateVersion;
}

juce::ValueTree DrumProcessor::fullStateVT() {
    juce::ValueTree v ("DRUMSYNTH");
    v.setProperty ("kit", currentKit, nullptr);
    v.setProperty ("kitName", kitName, nullptr);
    v.setProperty ("master", masterGain.load(), nullptr);
    v.setProperty ("limiter", limiterOn.load(), nullptr);
    v.setProperty ("selected", selectedPart.load(), nullptr);
    v.setProperty ("revDecay", revDecay.load(), nullptr); v.setProperty ("revDamp", revDamp.load(), nullptr);
    v.setProperty ("dlyTime", dlyTime.load(), nullptr);   v.setProperty ("dlyFb", dlyFb.load(), nullptr);
    for (int p = 0; p < NumParts; ++p) {
        juce::ValueTree pv = parts[p].params.toVT();
        pv.setProperty ("id", p, nullptr);
        pv.setProperty ("level", parts[p].level, nullptr);
        pv.setProperty ("pan", parts[p].pan, nullptr);
        pv.setProperty ("mute", parts[p].mute, nullptr);
        pv.setProperty ("solo", parts[p].solo, nullptr);
        pv.setProperty ("midi", parts[p].midiNote, nullptr);
        pv.setProperty ("sendRev", parts[p].sendRev.load(), nullptr);
        pv.setProperty ("sendDly", parts[p].sendDly.load(), nullptr);
        if (parts[p].output.load() != 0) pv.setProperty ("out", parts[p].output.load(), nullptr);
        pv.addChild (parts[p].fx.toVT(), -1, nullptr);
        v.addChild (pv, -1, nullptr);
    }
    v.addChild (globalFx.toVT(), -1, nullptr);
    return v;
}

void DrumProcessor::loadFullStateVT (const juce::ValueTree& v) {
    if (! v.isValid() || ! v.hasType ("DRUMSYNTH")) return;
    pushUndo();
    currentKit = (int) v.getProperty ("kit", 0);
    kitName = v.getProperty ("kitName", "Kit").toString();
    masterGain = (float) v.getProperty ("master", 0.8f);
    limiterOn = (bool) v.getProperty ("limiter", true);
    revDecay = (float) v.getProperty ("revDecay", 0.5f); revDamp = (float) v.getProperty ("revDamp", 0.5f);
    dlyTime = (float) v.getProperty ("dlyTime", 0.35f);  dlyFb = (float) v.getProperty ("dlyFb", 0.35f);
    selectedPart = (int) v.getProperty ("selected", 0);
    for (int p = 0; p < NumParts && p < v.getNumChildren() - 1; ++p) {
        auto pv = v.getChild (p);
        parts[p].params.fromVT (pv);
        parts[p].level = (float) pv.getProperty ("level", 0.8f);
        parts[p].pan = (float) pv.getProperty ("pan", 0.5f);
        parts[p].mute = (bool) pv.getProperty ("mute", false);
        parts[p].solo = (bool) pv.getProperty ("solo", false);
        parts[p].sendRev = (float) pv.getProperty ("sendRev", 0.f);
        parts[p].sendDly = (float) pv.getProperty ("sendDly", 0.f);
        parts[p].output = juce::jlimit (0, NumAuxOuts, (int) pv.getProperty ("out", 0));
        parts[p].midiNote = partMidiNote (p); // fixed map (stored "midi" is informational: older files used C4)
        if (pv.getNumChildren() > 0) {
            parts[p].fx.fromVT (pv.getChild (0));
            FxProcessor::normaliseChainOrder (parts[p].fx); // heal duplicated indices
        }
    }
    if (v.getNumChildren() > NumParts) {
        globalFx.fromVT (v.getChild (NumParts));
        FxProcessor::normaliseChainOrder (globalFx);
    }
    applyKitToApvts();
    pullApvtsToParts();
    updateReverseCaches();   // reversed parts play backwards from the very first hit after a load
    ++stateVersion;
}

void DrumProcessor::getStateInformation (juce::MemoryBlock& dest) {
    juce::MemoryOutputStream mos (dest, true);
    // APVTS + full state
    auto ap = apvts.copyState();
    juce::ValueTree root ("ROOT");
    root.addChild (ap, -1, nullptr);
    root.addChild (fullStateVT(), -1, nullptr);
    juce::ValueTree lk ("LOCKS"); // randomize locks: session-only
    for (int p = 0; p < NumParts; ++p) {
        juce::String bits;
        for (int k = 0; k < NumLocks; ++k) bits << (locks[p][k] ? "1" : "0");
        lk.setProperty ("p" + juce::String (p), bits, nullptr);
    }
    root.addChild (lk, -1, nullptr);
    root.writeToStream (mos);
}
void DrumProcessor::setStateInformation (const void* data, int size) {
    juce::MemoryInputStream mis (data, (size_t) size, false);
    auto root = juce::ValueTree::readFromStream (mis);
    if (! root.isValid()) return;
    auto ap = root.getChildWithName ("APVTS");
    // APVTS state tag is "PARAMS"
    for (int i = 0; i < root.getNumChildren(); ++i) {
        auto c = root.getChild (i);
        if (c.hasType ("PARAMS")) apvts.replaceState (c);
        if (c.hasType ("DRUMSYNTH")) loadFullStateVT (c);
        if (c.hasType ("LOCKS"))
            for (int p = 0; p < NumParts; ++p) {
                auto bits = c.getProperty ("p" + juce::String (p)).toString();
                for (int k = 0; k < NumLocks; ++k) locks[p][k] = k < bits.length() && bits[k] == '1';
            }
    }
    pullApvtsToParts();
}

// ---- randomize / mutate ----
void DrumProcessor::pushUndo() {
    undoStack.push_back (fullStateVT());
    if (undoStack.size() > 30) undoStack.erase (undoStack.begin());
}
void DrumProcessor::undo() {
    if (undoStack.empty()) return;
    auto v = undoStack.back(); undoStack.pop_back();
    // restore without pushing again
    currentKit = (int) v.getProperty ("kit", 0);
    kitName = v.getProperty ("kitName", "Kit").toString();
    masterGain = (float) v.getProperty ("master", 0.8f);
    limiterOn = (bool) v.getProperty ("limiter", true);
    revDecay = (float) v.getProperty ("revDecay", 0.5f); revDamp = (float) v.getProperty ("revDamp", 0.5f);
    dlyTime = (float) v.getProperty ("dlyTime", 0.35f);  dlyFb = (float) v.getProperty ("dlyFb", 0.35f);
    for (int p = 0; p < NumParts && p < v.getNumChildren() - 1; ++p) {
        auto pv = v.getChild (p);
        parts[p].params.fromVT (pv);
        parts[p].level = (float) pv.getProperty ("level", 0.8f);
        parts[p].pan = (float) pv.getProperty ("pan", 0.5f);
        parts[p].mute = (bool) pv.getProperty ("mute", false);
        parts[p].solo = (bool) pv.getProperty ("solo", false);
        parts[p].sendRev = (float) pv.getProperty ("sendRev", 0.f);
        parts[p].sendDly = (float) pv.getProperty ("sendDly", 0.f);
        parts[p].output = juce::jlimit (0, NumAuxOuts, (int) pv.getProperty ("out", 0));
        if (pv.getNumChildren() > 0) {
            parts[p].fx.fromVT (pv.getChild (0));
            FxProcessor::normaliseChainOrder (parts[p].fx); // heal duplicated indices
        }
    }
    if (v.getNumChildren() > NumParts) {
        globalFx.fromVT (v.getChild (NumParts));
        FxProcessor::normaliseChainOrder (globalFx);
    }
    applyKitToApvts(); pullApvtsToParts();
    ++stateVersion;
}

// randomize everything EXCEPT envelope slots (user requirement)
void DrumProcessor::randomizePart (int part, float intensity) {
    pushUndo();
    for (int s = 0; s < 20; ++s) {
        const ParamMeta* m = slotMeta (part, s);
        if (! m) continue;
        if (isEnvelopeSlot (part, s) || isLocked (part, s)) continue; // skip envelope + locked
        float span = m->max - m->min;
        float centre = m->def;
        float wild = centre + (liveRng.nextFloat() * 2.f - 1.f) * span * 0.5f * intensity;
        float keep = parts[part].params.p[s];
        parts[part].params.p[s] = juce::jlimit (m->min, m->max, keep * (1.f - intensity) + wild * intensity);
    }
    if (! isLocked (part, LockNoise) && liveRng.nextFloat() < 0.6f * intensity + 0.15f)
        parts[part].params.noiseType = liveRng.nextInt (8);
    if (! isLocked (part, LockWave) && liveRng.nextFloat() < 0.4f * intensity)
        parts[part].params.wave = liveRng.nextInt (5);
    if (liveRng.nextFloat() < 0.4f * intensity)
        parts[part].params.satMode = liveRng.nextInt (5);
    // randomize fx knob params (keep order/types; skip empty + bypassed + locked chain)
    for (int k = 0; k < FxChainState::NumSlots && ! isLocked (part, LockFx); ++k) {
        auto& sl = parts[part].fx.slots[k];
        if (sl.bypass) continue;
        if (sl.type.load() == FX_NONE) continue;
        auto j = [&] (std::atomic<float>& a) {
            float v = a.load();
            a = juce::jlimit (0.f, 1.f, v + (liveRng.nextFloat() * 2.f - 1.f) * 0.35f * intensity);
        };
        j (sl.p0); j (sl.p1); j (sl.p2); j (sl.p3); j (sl.p4);
    }
    randomizeModel (part, intensity);
    applyKitToApvts(); pullApvtsToParts();
    ++stateVersion;
    kitName = "Randomized " + juce::String (partName (part));
}
// Build a fresh random FX chain for one part from a per-part pool, with
// knob ranges kept musical (no 100% fold-back on hats, no reverb on kick).
// Slot types are atomics: safe to rewrite while the audio thread runs.
void DrumProcessor::randomizePartFx (int part, float intensity) {
    auto& ch = parts[part].fx;
    auto r = [this] (float a, float b) { return a + liveRng.nextFloat() * (b - a); };
    std::vector<int> pool;
    std::vector<int> distAlgos;
    switch (part) {
        case Kick:  pool = { FX_EQ, FX_COMP, FX_TRANSIENT, FX_DIST, FX_FILTER }; distAlgos = { 0, 1, 4 }; break;
        case Snare: pool = { FX_TRANSIENT, FX_COMP, FX_DIST, FX_EQ, FX_REVERB, FX_FILTER }; distAlgos = { 0, 2, 4, 6 }; break;
        case ClosedHat: case OpenHat: case Ride: case Crash:
                    pool = { FX_EQ, FX_TRANSIENT, FX_DIST, FX_FILTER, FX_DELAY, FX_REVERB }; distAlgos = { 0, 5 }; break;
        default:    pool = { FX_REVERB, FX_DELAY, FX_DIST, FX_FILTER, FX_COMP, FX_EQ }; distAlgos = { 0, 2, 3, 5, 6 }; break;
    }
    int maxFx = 1 + (int) std::round (2.f * intensity);
    int n = liveRng.nextInt (maxFx + 1);
    for (int s = 0; s < FxChainState::NumSlots; ++s) {
        auto& sl = ch.slots[(size_t) s];
        if (s >= n) { sl.type.store (FX_NONE); sl.bypass.store (false); continue; }
        int type = pool[(size_t) liveRng.nextInt ((int) pool.size())];
        FxProcessor::defaultSlot (type, sl);
        switch (type) {
            case FX_EQ:        sl.setParams (r (.35f,.65f), r (.35f,.65f), r (.35f,.65f), r (.35f,.65f), .5f, 1.f); break;
            case FX_COMP:      sl.setParams (r (.45f,.75f), r (.1f,.35f), r (.1f,.4f), r (.2f,.5f), r (.3f,.6f), r (.6f,1.f)); break;
            case FX_TRANSIENT: sl.setParams (r (.5f,.85f), r (.35f,.65f), r (.2f,.6f), r (.4f,.7f), .5f, 1.f); break;
            case FX_DIST: {
                int a = distAlgos[(size_t) liveRng.nextInt ((int) distAlgos.size())];
                sl.setParams (r (.05f,.3f), r (.35f,.7f), (a + .5f) / 8.f, r (.4f,.5f), 0.f, r (.25f,.6f));
                break;
            }
            // reverb/delay inserts are fully wet at MIX 1: keep the blend low
            case FX_REVERB:    sl.setParams (r (.2f,.6f), r (.3f,.9f), r (.3f,.8f), r (0.f,.3f), r (.3f,.7f), r (.04f,.12f)); break;
            case FX_DELAY:     sl.setParams (r (.25f,.5f), r (.15f,.4f), r (.3f,.6f), r (.1f,.4f), r (0.f,.3f), r (.05f,.15f)); break;
            case FX_FILTER: {
                // kick: LP; cymbals: HP; others: BP. Cutoff kept out of the extremes.
                bool cym = part == ClosedHat || part == OpenHat || part == Ride || part == Crash;
                float mode = part == Kick ? 0.1f : (cym ? 0.3f : 0.6f);
                float cut = part == Kick ? r (.7f,.9f) : (cym ? r (.55f,.75f) : r (.45f,.75f));
                sl.setParams (cut, r (.05f,.3f), mode, liveRng.nextBool() ? 0.f : 1.f, r (0.f,.2f), 1.f);
                break;
            }
            default: break;
        }
    }
}

void DrumProcessor::randomizeKit (float intensity) {
    pushUndo();
    for (int p = 0; p < NumParts; ++p) {
        for (int s = 0; s < 20; ++s) {
            const ParamMeta* m = slotMeta (p, s);
            if (! m) continue;
            if (isEnvelopeSlot (p, s) || isLocked (p, s)) continue;
            float span = m->max - m->min;
            float wild = m->def + (liveRng.nextFloat() * 2.f - 1.f) * span * 0.5f * intensity;
            parts[p].params.p[s] = juce::jlimit (m->min, m->max, wild);
        }
        if (! isLocked (p, LockNoise) && liveRng.nextFloat() < 0.5f) parts[p].params.noiseType = liveRng.nextInt (8);
        if (! isLocked (p, LockFx)) randomizePartFx (p, intensity);
        bool roomy = p != Kick && p != Sfx;
        if (! isLocked (p, LockSendRev))
            parts[p].sendRev = roomy && liveRng.nextFloat() < 0.5f ? liveRng.nextFloat() * 0.3f * intensity : 0.f;
        if (! isLocked (p, LockSendDly))
            parts[p].sendDly = p != Kick && liveRng.nextFloat() < 0.25f ? liveRng.nextFloat() * 0.25f * intensity : 0.f;
    }
    for (int p = 0; p < NumParts; ++p) randomizeModel (p, intensity);
    applyKitToApvts(); pullApvtsToParts();
    ++stateVersion;
    kitName = "Randomized Kit";
}
void DrumProcessor::mutatePart (int part, float amount) {
    pushUndo();
    for (int s = 0; s < 20; ++s) {
        const ParamMeta* m = slotMeta (part, s);
        if (! m || isLocked (part, s)) continue;
        float span = m->max - m->min;
        float radius = span * amount * (isEnvelopeSlot (part, s) ? 0.3f : 1.f) * 0.25f; // mild; env moves even less
        float v = parts[part].params.p[s] + (liveRng.nextFloat() * 2.f - 1.f) * radius;
        parts[part].params.p[s] = juce::jlimit (m->min, m->max, v);
    }
    // occasionally drift noise type for flavour (small chance)
    if (! isLocked (part, LockNoise) && liveRng.nextFloat() < 0.12f) parts[part].params.noiseType = liveRng.nextInt (8);
    for (int k = 0; k < FxChainState::NumSlots && ! isLocked (part, LockFx); ++k) {
        auto& sl = parts[part].fx.slots[k];
        if (sl.bypass) continue;
        auto j = [&] (std::atomic<float>& a) {
            a = juce::jlimit (0.f, 1.f, a.load() + (liveRng.nextFloat() * 2.f - 1.f) * 0.08f * (amount * 4.f));
        };
        j (sl.p0); j (sl.p1); j (sl.p2);
    }
    mutateModel (part, amount);   // knobs only: MUTATE never switches the model
    applyKitToApvts(); pullApvtsToParts();
    ++stateVersion;
    kitName = "Mutated " + juce::String (partName (part));
}
void DrumProcessor::mutateKit (float amount) {
    pushUndo();
    for (int p = 0; p < NumParts; ++p) {
        for (int s = 0; s < 20; ++s) {
            const ParamMeta* m = slotMeta (p, s);
            if (! m || isLocked (p, s)) continue;
            float span = m->max - m->min;
            float radius = span * amount * 0.25f;
            parts[p].params.p[s] = juce::jlimit (m->min, m->max, parts[p].params.p[s] + (liveRng.nextFloat() * 2.f - 1.f) * radius);
        }
    }
    for (int p = 0; p < NumParts; ++p) mutateModel (p, amount);
    applyKitToApvts(); pullApvtsToParts();
    ++stateVersion;
    kitName = "Mutated Kit";
}

// ---- engine models ----
void DrumProcessor::setPartModel (int part, int model) {
    part = juce::jlimit (0, NumParts - 1, part);
    pushUndo();
    auto& pp = parts[part].params;
    pp.model = juce::jlimit (0, numModels (part) - 1, model);
    modelDefaults01 (part, pp.model, pp.m);
    if (auto* md = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("p" + juce::String (part) + "_model")))
        md->setValueNotifyingHost (md->convertTo0to1 ((float) pp.model));
    for (int k = 0; k < NumModelParams; ++k)
        if (auto* mp = apvts.getParameter (modelParamID (part, k))) mp->setValueNotifyingHost (pp.m[k]);
    pullApvtsToParts();
    ++stateVersion;
}

// RANDOMIZE may switch the engine model (unless locked); knobs move around the model's defaults.
// Envelope knobs and VEL>AMP (dynamics) are never randomized; switching model loads their defaults.
void DrumProcessor::randomizeModel (int part, float intensity) {
    if (isLocked (part, LockModel)) return;
    auto& pp = parts[part].params;
    if (liveRng.nextFloat() < 0.35f * intensity + 0.1f) {
        int model = liveRng.nextInt (numModels (part));
        if (model != pp.model) { pp.model = model; modelDefaults01 (part, model, pp.m); }
    }
    if (auto* md = modelDef (part, pp.model))
        for (int k = 0; k < md->nparams; ++k) {
            if (modelParamIsEnvelope (md->p[k])) continue;
            float def = modelUncook (md->p[k], md->p[k].def);
            float wild = def + (liveRng.nextFloat() * 2.f - 1.f) * 0.35f * intensity;
            pp.m[k] = juce::jlimit (0.f, 1.f, pp.m[k] * (1.f - intensity) + wild * intensity);
        }
}

// MUTATE never changes the model, only nudges its knobs
void DrumProcessor::mutateModel (int part, float amount) {
    auto& pp = parts[part].params;
    if (pp.model <= 0 || isLocked (part, LockModel)) return;
    if (auto* md = modelDef (part, pp.model))
        for (int k = 0; k < md->nparams; ++k)
            if (! modelParamIsEnvelope (md->p[k])) pp.m[k] = juce::jlimit (0.f, 1.f, pp.m[k] + (liveRng.nextFloat() * 2.f - 1.f) * amount * 0.25f);
}

// ---- REVERSE ----
// The part's sound (whatever engine) is rendered once at full velocity, trimmed, faded and
// reversed on the message thread; hits then play that buffer. Re-rendered when any setting
// that shapes the sound changes. The pool keeps every buffer alive until no voice holds it,
// so the audio thread never frees memory.
void DrumProcessor::updateReverseCaches() {
    const double sr = sampleRate;
    for (int p = 0; p < NumParts; ++p) {
        auto& pe = parts[p];
        PartParams pp;
        defaultParamsForPart (p, pp);
        for (int s = 0; s < 20; ++s)
            if (auto* par = apvts.getRawParameterValue (synthParamID (p, s))) pp.p[s] = par->load();
        if (auto* w = apvts.getRawParameterValue ("p" + juce::String (p) + "_wave")) pp.wave = juce::jlimit (0, 4, (int) std::round (w->load()));
        if (auto* nz = apvts.getRawParameterValue ("p" + juce::String (p) + "_noise")) pp.noiseType = juce::jlimit (0, 7, (int) std::round (nz->load()));
        readPartFromApvts (p, pp);
        if (! pp.reverse) {
            if (pe.revCur != nullptr) { const juce::SpinLock::ScopedLockType sl (pe.revLock); pe.revCur = nullptr; }
            pe.revSig = 0;
            continue;
        }
        float tuneOff = 0.f;
        if (auto* t = apvts.getRawParameterValue ("tune" + juce::String (p))) tuneOff = t->load();
        juce::uint64 h = 1469598103934665603ull;
        auto mixIn = [&h] (const void* d, size_t nBytes) {
            auto* b = static_cast<const unsigned char*> (d);
            for (size_t i = 0; i < nBytes; ++i) { h ^= b[i]; h *= 1099511628211ull; }
        };
        mixIn (pp.p, sizeof (pp.p)); mixIn (&pp.wave, sizeof (int)); mixIn (&pp.noiseType, sizeof (int));
        mixIn (&pp.model, sizeof (int)); mixIn (pp.m, sizeof (pp.m)); mixIn (&tuneOff, sizeof (float)); mixIn (&sr, sizeof (double));
        if (h == pe.revSig && pe.revCur != nullptr) continue;
        auto v = std::make_unique<DrumVoice>();
        v->prepare ((float) sr);
        v->noise.rng.setSeed (7777 + p);
        pp.reverse = false;
        v->trigger (p, pp, 1.0f, tuneOff);
        RevBuf::Ptr buf = new RevBuf();
        auto& data = buf->data;
        const int maxLen = (int) (sr * 4.0);
        data.reserve ((size_t) maxLen);
        for (int i = 0; i < maxLen && v->active; ++i) data.push_back (v->renderWithParams (pp));
        float pk = 0.f;
        for (float x : data) pk = std::max (pk, std::abs (x));
        int end = (int) data.size();
        while (end > 0 && std::abs (data[(size_t) end - 1]) < pk * 1.0e-3f) --end;
        data.resize ((size_t) std::max (1, end));
        int fade = std::min ((int) data.size(), (int) (0.01 * sr));
        for (int i = 0; i < fade; ++i) data[data.size() - 1 - (size_t) i] *= (float) i / (float) fade;   // soft start once reversed
        std::reverse (data.begin(), data.end());
        revPool.add (buf.get());
        { const juce::SpinLock::ScopedLockType sl (pe.revLock); pe.revCur = buf; }
        pe.revSig = h;
    }
    for (int i = revPool.size(); --i >= 0;)
        if (revPool.getObjectPointerUnchecked (i)->getReferenceCount() == 1) revPool.remove (i);
}

} // namespace mds

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new mds::DrumProcessor(); }
