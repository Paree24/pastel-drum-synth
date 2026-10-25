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

DrumProcessor::~DrumProcessor() {
    removeListener (&gestureUndo);
    stopTimer();
}

DrumProcessor::DrumProcessor()
    : juce::AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "PARAMS", makeLayout()) {
    cacheParamPointers();
    prepareLimiter();   // usable before the host's first prepareToPlay
    for (int p = 0; p < NumParts; ++p) {
        defaultParamsForPart (p, parts[p].params);
        parts[p].midiNote = partMidiNote (p);
        parts[p].level = 0.8f; parts[p].pan = 0.5f;
        parts[p].choke = defaultChoke (p);
        FxProcessor::clearChain (parts[p].fx); // no effects by default; user adds them
        defaultMacros (p);
    }
    FxProcessor::clearChain (globalFx);
    loadKit (1);
    undoStack.clear();   // the startup kit is not an undo step
    midiLearn.onPresetStep = [this] (int delta) {
        // same walk as the header's < > buttons: factory order, wrapping
        auto& all = getFactoryPresets();
        if (all.empty()) return;
        int pos = 0;
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].index == currentKit) { pos = (int) i; break; }
        const int n = (int) all.size();
        setCurrentProgram (all[(size_t) (((pos + delta) % n + n) % n)].index);
    };
    gestureUndo.startValue.assign ((size_t) getParameters().size(), -1.f);
    addListener (&gestureUndo);
    startTimerHz (10);
}

void DrumProcessor::cacheParamPointers() {
    for (int p = 0; p < NumParts; ++p) {
        for (int s = 0; s < 20; ++s) {
            slotRaw[p][s] = apvts.getRawParameterValue (synthParamID (p, s));   // nullptr: unused adv slot
            if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (synthParamID (p, s))))
                slotRange[p][s] = pf->range;
        }
        waveRaw[p] = apvts.getRawParameterValue ("p" + juce::String (p) + "_wave");
        noiseRaw[p] = apvts.getRawParameterValue ("p" + juce::String (p) + "_noise");
        lvlRaw[p] = apvts.getRawParameterValue ("mix" + juce::String (p) + "_lvl");
        panRaw[p] = apvts.getRawParameterValue ("mix" + juce::String (p) + "_pan");
        tuneRaw[p] = apvts.getRawParameterValue (tuneParamID (p));
        modelRaw[p] = apvts.getRawParameterValue ("p" + juce::String (p) + "_model");
        for (int k = 0; k < NumModelParams; ++k) mpRaw[p][k] = apvts.getRawParameterValue (modelParamID (p, k));
        revRaw[p] = apvts.getRawParameterValue ("p" + juce::String (p) + "_rev");
        sendRaw[p][0] = apvts.getRawParameterValue (sendParamID (p, false));
        sendRaw[p][1] = apvts.getRawParameterValue (sendParamID (p, true));
        for (int k = 0; k < NumMacros; ++k) macroRaw[p][k] = apvts.getRawParameterValue (macroParamID (p, k));
        jassert (waveRaw[p] && noiseRaw[p] && lvlRaw[p] && panRaw[p] && tuneRaw[p] && modelRaw[p] && revRaw[p]
                 && sendRaw[p][0] && sendRaw[p][1] && macroRaw[p][NumMacros - 1]);
    }
    masterRaw = apvts.getRawParameterValue ("master");
}

void DrumProcessor::prepareLimiter() {
    limiter.prepare (sampleRate);
    auxDelayLen = limiter.latency();
    auxDelay.assign ((size_t) (NumAuxOuts * 2 * auxDelayLen), 0.f);
    auxDelayPos = 0;
    setLatencySamples (limiter.latency());
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
    // added later (kept after "master" so older parameter orders are unchanged):
    // per-part FX SEND levels and the 4 macros
    for (int p = 0; p < NumParts; ++p) {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (sendParamID (p, false), 1), juce::String (partName (p)) + " Send Rev",
            juce::NormalisableRange<float> (0.f, 1.f), 0.f));
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID (sendParamID (p, true), 1), juce::String (partName (p)) + " Send Dly",
            juce::NormalisableRange<float> (0.f, 1.f), 0.f));
        for (int k = 0; k < NumMacros; ++k)
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID (macroParamID (p, k), 1), juce::String (partName (p)) + " Macro " + juce::String (k + 1),
                juce::NormalisableRange<float> (0.f, 1.f), 0.5f));
    }
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
    // the send delay also runs tempo-synced: room for a 1/2 note at slow tempos
    sendDlyDsp.delayBuf.setSize (2, (int) (sr * FxProcessor::sendDelayMaxSec) + 64, false, true, false);
    sendDlyDsp.delayBuf.clear(); sendDlyDsp.delayPos = 0;
    prepareLimiter();
    pullApvtsToParts();
}

static const juce::NormalisableRange<float> levelRange (0.f, 1.2f), unitRange (0.f, 1.f), tuneRange (-24.f, 24.f);
static inline float applyMacro (const juce::NormalisableRange<float>& r, float v, float off) {
    return off != 0.f ? r.convertFrom0to1 (juce::jlimit (0.f, 1.f, r.convertTo0to1 (v) + off)) : v;
}

bool DrumProcessor::macroOffsets (int p, float* off) const {
    bool any = false;
    std::fill (off, off + NumMacroTargets, 0.f);
    for (int k = 0; k < NumMacros; ++k) {
        float bip = (macroRaw[p][k]->load() - 0.5f) * 2.f; // centre = neutral
        if (std::abs (bip) < 1.0e-4f) continue;
        for (auto& t : parts[p].macros[k].t) {
            int prm = t.param.load();
            if (prm < 0 || prm >= NumMacroTargets) continue;
            off[prm] += t.depth.load() * bip;
            any = true;
        }
    }
    return any;
}

// audio + message thread: cached pointer reads, no allocation
void DrumProcessor::pullApvtsToParts() {
    for (int p = 0; p < NumParts; ++p) {
        auto& pe = parts[p];
        float off[NumMacroTargets];
        macroOffsets (p, off);
        for (int s = 0; s < 20; ++s)
            if (slotRaw[p][s]) pe.params.p[s] = applyMacro (slotRange[p][s], slotRaw[p][s]->load(), off[s]);
        // lock-free reads of choice params. NOTE: choice raw values are
        // DENORMALISED indices (not 0..1) - do not rescale them.
        pe.params.wave = juce::jlimit (0, 4, (int) std::round (waveRaw[p]->load()));
        pe.params.noiseType = juce::jlimit (0, 7, (int) std::round (noiseRaw[p]->load()));
        pe.params.satMode = 0; // fixed Soft: DRIVE knob + DISTORTION carry the grit
        pe.level = applyMacro (levelRange, lvlRaw[p]->load(), off[MT_Level]);
        pe.pan = applyMacro (unitRange, panRaw[p]->load(), off[MT_Pan]);
        pe.tune = applyMacro (tuneRange, tuneRaw[p]->load(), off[MT_Tune]);
        pe.sendRev.store (applyMacro (unitRange, sendRaw[p][0]->load(), off[MT_SendRev]), std::memory_order_relaxed);
        pe.sendDly.store (applyMacro (unitRange, sendRaw[p][1]->load(), off[MT_SendDly]), std::memory_order_relaxed);
        readPartFromApvts (p, pe.params);
    }
    if (masterRaw) masterGain = masterRaw->load();
}

void DrumProcessor::readPartFromApvts (int p, PartParams& out) const {
    float off[NumMacroTargets];
    macroOffsets (p, off);
    out.model = juce::jlimit (0, numModels (p) - 1, (int) std::round (modelRaw[p]->load()));
    for (int k = 0; k < NumModelParams; ++k) out.m[k] = applyMacro (unitRange, mpRaw[p][k]->load(), off[MT_Model0 + k]);
    out.reverse = revRaw[p]->load() > 0.5f;
}

void DrumProcessor::triggerPart (int part, float vel) {
    // choke: parts in the same group cut each other
    int g = parts[part].choke.load();
    // CHOKE (hats): 0 = lets the other hat ring, 100 = cuts it in 5 ms
    float chokeAmt = (part == ClosedHat || part == OpenHat) ? parts[part].params.p[14] / 100.f : 1.f;
    if (g != 0 && chokeAmt > 0.005f)
        for (int p = 0; p < NumParts; ++p) if (p != part && parts[p].choke.load() == g)
            for (auto& v : parts[p].voices) if (v.active) v.chokeNow (80.f - 75.f * chokeAmt);
    float tuneOff = parts[part].tune;
    // find free voice
    DrumVoice* tgt = nullptr;
    for (auto& v : parts[part].voices) if (! v.active) { tgt = &v; break; }
    if (! tgt) tgt = &parts[part].voices[0]; // steal oldest-ish
    if (parts[part].params.reverse) {
        RevBuf::Ptr b;
        { const juce::SpinLock::ScopedLockType sl (parts[part].revLock); b = parts[part].revCur; }
        if (b != nullptr) {
            const auto& pp = parts[part].params;
            const auto* md = pp.model > 0 ? modelDef (part, pp.model) : nullptr;
            const float amt = md != nullptr ? modelCook (md->p[md->nparams - 1], pp.m[md->nparams - 1]) * 0.01f : pp.p[15] / 100.f;
            tgt->startReverse (part, b, velGain (vel, amt), vel, amt);
            return;
        }
    }
    tgt->trigger (part, parts[part].params, vel, tuneOff);
}

void DrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midiLearn.process (midi); // learned CCs land before this block renders
    pullApvtsToParts(); // cheap pointer reads; no allocation
    if (int a = auditionReq.exchange (-1); a >= 0 && a < NumParts) triggerPart (a, 0.9f);

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
    bool anyAux = false;
    for (int o = 1; o <= NumAuxOuts; ++o) anyAux = anyAux || auxL[o] != nullptr;
    if (auxDelay.size() < (size_t) (NumAuxOuts * 2 * auxDelayLen)) anyAux = false; // not prepared
    const float auxGain = masterGain.load();
    auto mainBuf = getBusBuffer (buffer, false, 0);
    auto* L = mainBuf.getWritePointer (0);
    auto* R = mainBuf.getNumChannels() > 1 ? mainBuf.getWritePointer (1) : nullptr;
    float sr = (float) sampleRate;

    // block rate: solo state (muted / soloed-out parts keep running, silently,
    // so their voices and FX tails carry on instead of freezing)
    bool anySolo = false;
    for (auto& pe : parts) if (pe.solo.load()) { anySolo = true; break; }
    bool silent[NumParts];
    float sRev[NumParts], sDly[NumParts];
    for (int p = 0; p < NumParts; ++p) {
        silent[p] = parts[p].mute.load() || (anySolo && ! parts[p].solo.load());
        sRev[p] = parts[p].sendRev.load (std::memory_order_relaxed);
        sDly[p] = parts[p].sendDly.load (std::memory_order_relaxed);
    }
    const double bpm = TempoSync::hostBpm (getPlayHead(), hostBpm.load());
    hostBpm = (float) bpm;
    const int dsync = dlySync.load();
    const float dlySec = dsync >= 0 ? (float) TempoSync::divisionSeconds (dsync, bpm) : FxProcessor::sendDelaySeconds (dlyTime.load());
    const bool limitOn = limiterOn.load();
    auto it = midi.cbegin();
    const auto end = midi.cend();

    for (int i = 0; i < N; ++i) {
        // sample-accurate MIDI
        for (; it != end && (*it).samplePosition <= i; ++it) {
            const auto msg = (*it).getMessage();
            if (msg.isNoteOn()) {
                float vel = msg.getFloatVelocity();
                int note = msg.getNoteNumber();
                for (int p = 0; p < NumParts; ++p)
                    if (parts[p].midiNote == note && ! parts[p].mute.load()) triggerPart (p, vel);
            } else if (msg.isNoteOff()) {
                int note = msg.getNoteNumber();
                for (int p = 0; p < NumParts; ++p)
                    if (parts[p].midiNote == note)
                        for (auto& v : parts[p].voices) v.noteOff();
            } else if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
                for (auto& pe : parts) for (auto& v : pe.voices) if (v.active) v.chokeNow (5.f);
            }
        }

        float mixL = 0.f, mixR = 0.f, revIn = 0.f, dlyIn = 0.f;
        float accL[NumAuxOuts + 1] = {}, accR[NumAuxOuts + 1] = {};
        for (int p = 0; p < NumParts; ++p) {
            auto& pe = parts[p];
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
            float meter = pe.meter.load (std::memory_order_relaxed);
            if (! va && pe.tail <= 0) { pe.meter.store (meter * 0.9995f, std::memory_order_relaxed); continue; }
            float m = sig;
            if (pe.fx.enabled.load())
                for (int k = 0; k < FxChainState::NumSlots; ++k) {
                    int idx = pe.fx.order[(size_t) k];
                    m = FxProcessor::processSlot (pe.fx.slots[(size_t) idx], pe.fxDsp[idx], m, sr);
                }
            m += post; // clean layer joins after the part chain
            // capture post-FX waveform for scope/analyzer (freezes when tail ends)
            pe.scope.push (m);
            if (! va) --pe.tail;
            if (silent[p]) { pe.meter.store (meter * 0.9995f, std::memory_order_relaxed); continue; }
            float pan = juce::jlimit (0.f, 1.f, pe.pan);
            float gl = std::cos (pan * juce::MathConstants<float>::halfPi);
            float gr = std::sin (pan * juce::MathConstants<float>::halfPi);
            if (route[p] == 0) {
                mixL += m * gl * pe.level;
                mixR += m * gr * pe.level;
            } else {
                float ol = m * gl * pe.level * auxGain, orr = m * gr * pe.level * auxGain;
                if (! std::isfinite (ol) || ! std::isfinite (orr)) ol = orr = 0.f;
                accL[route[p]] += ol; accR[route[p]] += orr;
            }
            revIn += m * pe.level * sRev[p];
            dlyIn += m * pe.level * sDly[p];
            pe.meter.store (va ? juce::jmax (meter * 0.9995f, juce::jmin (1.f, std::abs (m * pe.level))) : meter * 0.9995f,
                            std::memory_order_relaxed);
        }
        if (anyAux) {   // aux buses: the limiter's lookahead as a plain delay, so every bus lines up with Main
            for (int o = 1; o <= NumAuxOuts; ++o) {
                if (auxL[o] == nullptr) continue;
                float* dl = auxDelay.data() + (size_t) ((o - 1) * 2 * auxDelayLen);
                float* dr = dl + auxDelayLen;
                const float yl = dl[auxDelayPos], yr = dr[auxDelayPos];
                dl[auxDelayPos] = accL[o]; dr[auxDelayPos] = accR[o];
                if (auxR[o]) { auxL[o][i] += yl; auxR[o][i] += yr; }
                else auxL[o][i] += (yl + yr) * 0.5f;
            }
            auxDelayPos = (auxDelayPos + 1) % auxDelayLen;
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
            FxProcessor::sendDelayStereo (sendDlyDsp, dlyIn, dlySec, dlyFb.load(), sr, dL, dR);
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
        // final safety: never feed / emit non-finite samples (a NaN stream reads as "muted")
        if (! std::isfinite (gL)) gL = 0.f;
        if (! std::isfinite (gR)) gR = 0.f;
        // lookahead peak limiter (off: same lookahead delay, no gain change)
        limiter.process (&gL, &gR, 1, LimitCeiling, limitOn);
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
        // TUNE + sends: set by their own setters (never forced here)
        if (auto* md = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("p" + juce::String (p) + "_model")))
            md->setValueNotifyingHost (md->convertTo0to1 ((float) juce::jlimit (0, numModels (p) - 1, parts[p].params.model)));
        for (int k = 0; k < NumModelParams; ++k)
            if (auto* mp = apvts.getParameter (modelParamID (p, k))) mp->setValueNotifyingHost (juce::jlimit (0.f, 1.f, parts[p].params.m[k]));
        if (auto* rv = apvts.getParameter ("p" + juce::String (p) + "_rev")) rv->setValueNotifyingHost (parts[p].params.reverse ? 1.f : 0.f);
    }
    if (auto* m = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter ("master")))
        m->setValueNotifyingHost (m->convertTo0to1 (masterGain.load()));
}

void DrumProcessor::setSend (int part, bool dly, float v) {
    v = juce::jlimit (0.f, 1.f, v);
    if (auto* q = apvts.getParameter (sendParamID (part, dly))) q->setValueNotifyingHost (v);
    (dly ? parts[part].sendDly : parts[part].sendRev).store (v);   // mirror (re-read every block)
}
float DrumProcessor::getSend (int part, bool dly) const { return sendRaw[part][dly ? 1 : 0]->load(); }

void DrumProcessor::setTune (int part, float semis) {
    if (auto* t = dynamic_cast<juce::AudioParameterFloat*> (apvts.getParameter (tuneParamID (part))))
        t->setValueNotifyingHost (t->convertTo0to1 (juce::jlimit (-24.f, 24.f, semis)));
}
float DrumProcessor::getTune (int part) const { return tuneRaw[part]->load(); }

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
        setSend (p, false, 0.f); setSend (p, true, 0.f);
        setTune (p, 0.f);
        parts[p].midiNote = partMidiNote (p);
        parts[p].output = 0;
        parts[p].choke = defaultChoke (p);
        FxProcessor::clearChain (parts[p].fx);
        defaultMacros (p);
    }
    FxProcessor::clearChain (globalFx);
    masterGain = 0.8f;
    dlySync = -1;
    applyKitToApvts();
    pullApvtsToParts();
    ++stateVersion;
}

juce::String DrumProcessor::macroTargetKey (int t) {
    if (t >= 0 && t < 20) return "s" + juce::String (t);
    if (t >= MT_Model0 && t < MT_Model0 + NumModelParams) return "m" + juce::String (t - MT_Model0);
    switch (t) {
        case MT_Level: return "level"; case MT_Pan: return "pan";
        case MT_SendRev: return "srev"; case MT_SendDly: return "sdly"; case MT_Tune: return "tune";
        default: return {};
    }
}
int DrumProcessor::macroTargetFromKey (const juce::String& k) {
    for (int t = 0; t < NumMacroTargets; ++t) if (macroTargetKey (t) == k) return t;
    return -1;
}

juce::ValueTree DrumProcessor::partToVT (int p) {
    juce::ValueTree pv = parts[p].params.toVT();
    pv.setProperty ("id", p, nullptr);
    pv.setProperty ("level", lvlRaw[p]->load(), nullptr);   // un-modulated values
    pv.setProperty ("pan", panRaw[p]->load(), nullptr);
    pv.setProperty ("mute", parts[p].mute.load(), nullptr);
    pv.setProperty ("solo", parts[p].solo.load(), nullptr);
    pv.setProperty ("midi", parts[p].midiNote, nullptr);
    pv.setProperty ("sendRev", getSend (p, false), nullptr);
    pv.setProperty ("sendDly", getSend (p, true), nullptr);
    pv.setProperty ("tune", getTune (p), nullptr);
    pv.setProperty ("choke", parts[p].choke.load(), nullptr);
    if (parts[p].output.load() != 0) pv.setProperty ("out", parts[p].output.load(), nullptr);
    pv.addChild (parts[p].fx.toVT(), -1, nullptr);   // child 0: the FX chain (older readers expect it first)
    for (int k = 0; k < NumMacros; ++k) {
        juce::ValueTree m ("MACRO");
        m.setProperty ("k", k, nullptr);
        m.setProperty ("name", parts[p].macros[k].name, nullptr);
        m.setProperty ("value", macroRaw[p][k]->load(), nullptr);
        juce::StringArray tg;
        for (auto& t : parts[p].macros[k].t)
            if (t.param.load() >= 0) tg.add (macroTargetKey (t.param.load()) + ":" + juce::String (t.depth.load(), 4));
        m.setProperty ("targets", tg.joinIntoString (","), nullptr);
        pv.addChild (m, -1, nullptr);
    }
    return pv;
}

void DrumProcessor::partFromVT (int p, const juce::ValueTree& pv) {
    parts[p].params.fromVT (pv);
    parts[p].level = (float) pv.getProperty ("level", 0.8f);
    parts[p].pan = (float) pv.getProperty ("pan", 0.5f);
    parts[p].mute = (bool) pv.getProperty ("mute", false);
    parts[p].solo = (bool) pv.getProperty ("solo", false);
    setSend (p, false, (float) pv.getProperty ("sendRev", 0.f));
    setSend (p, true, (float) pv.getProperty ("sendDly", 0.f));
    setTune (p, (float) pv.getProperty ("tune", 0.f));   // older kit files: 0 (setStateInformation keeps a project's own)
    parts[p].choke = juce::jlimit (0, NumChokeGroups, (int) pv.getProperty ("choke", defaultChoke (p)));
    parts[p].output = juce::jlimit (0, NumAuxOuts, (int) pv.getProperty ("out", 0));
    parts[p].midiNote = partMidiNote (p); // fixed map (stored "midi" is informational: older files used C4)
    if (pv.getNumChildren() > 0 && ! pv.getChild (0).hasType ("MACRO")) {
        parts[p].fx.fromVT (pv.getChild (0));
        FxProcessor::normaliseChainOrder (parts[p].fx); // heal duplicated indices
    }
    bool sawMacro = false;
    for (auto c : pv) {
        if (! c.hasType ("MACRO")) continue;
        int k = (int) c.getProperty ("k", -1);
        if (k < 0 || k >= NumMacros) continue;
        if (! sawMacro) { // file has macros: start from a clean slate
            for (auto& m : parts[p].macros) for (auto& t : m.t) { t.param = -1; t.depth = 0.f; }
            sawMacro = true;
        }
        parts[p].macros[k].name = c.getProperty ("name", "MACRO " + juce::String (k + 1)).toString();
        if (auto* mp = apvts.getParameter (macroParamID (p, k))) mp->setValueNotifyingHost ((float) c.getProperty ("value", 0.5f));
        for (auto& tok : juce::StringArray::fromTokens (c.getProperty ("targets").toString(), ",", "")) {
            int prm = macroTargetFromKey (tok.upToFirstOccurrenceOf (":", false, false));
            if (prm >= 0) setMacroTarget (p, k, prm, tok.fromFirstOccurrenceOf (":", false, false).getFloatValue());
        }
    }
    if (! sawMacro) defaultMacros (p);
}

juce::ValueTree DrumProcessor::fullStateVT() {
    juce::ValueTree v ("DRUMSYNTH");
    v.setProperty ("kit", currentKit, nullptr);
    v.setProperty ("kitName", kitName, nullptr);
    v.setProperty ("master", masterRaw ? masterRaw->load() : masterGain.load(), nullptr);
    v.setProperty ("limiter", limiterOn.load(), nullptr);
    v.setProperty ("selected", selectedPart.load(), nullptr);
    v.setProperty ("revDecay", revDecay.load(), nullptr); v.setProperty ("revDamp", revDamp.load(), nullptr);
    v.setProperty ("dlyTime", dlyTime.load(), nullptr);   v.setProperty ("dlyFb", dlyFb.load(), nullptr);
    v.setProperty ("dlySync", dlySync.load(), nullptr);
    for (int p = 0; p < NumParts; ++p) v.addChild (partToVT (p), -1, nullptr);
    v.addChild (globalFx.toVT(), -1, nullptr);
    return v;
}

void DrumProcessor::applyStateVT (const juce::ValueTree& v) {
    currentKit = (int) v.getProperty ("kit", 0);
    kitName = v.getProperty ("kitName", "Kit").toString();
    masterGain = (float) v.getProperty ("master", 0.8f);
    limiterOn = (bool) v.getProperty ("limiter", true);
    revDecay = (float) v.getProperty ("revDecay", 0.5f); revDamp = (float) v.getProperty ("revDamp", 0.5f);
    dlyTime = (float) v.getProperty ("dlyTime", 0.35f);  dlyFb = (float) v.getProperty ("dlyFb", 0.35f);
    dlySync = juce::jlimit (-1, TempoSync::NumDivisions - 1, (int) v.getProperty ("dlySync", -1)); // older files: free time
    selectedPart = juce::jlimit (0, NumParts - 1, (int) v.getProperty ("selected", 0));
    for (int p = 0; p < NumParts && p < v.getNumChildren() - 1; ++p)
        partFromVT (p, v.getChild (p));
    if (v.getNumChildren() > NumParts) {
        globalFx.fromVT (v.getChild (NumParts));
        FxProcessor::normaliseChainOrder (globalFx);
    }
    applyKitToApvts();
    pullApvtsToParts();
    updateReverseCaches();   // reversed parts play backwards from the very first hit after a load
    ++stateVersion;
}

void DrumProcessor::loadFullStateVT (const juce::ValueTree& v, bool undoable) {
    if (! v.isValid() || ! v.hasType ("DRUMSYNTH")) return;
    if (undoable) pushUndo();
    applyStateVT (v);
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
    // APVTS state tag is "PARAMS"
    for (int i = 0; i < root.getNumChildren(); ++i) {
        auto c = root.getChild (i);
        if (c.hasType ("PARAMS")) apvts.replaceState (c);
        if (c.hasType ("DRUMSYNTH")) {
            // projects saved before TUNE was part of the kit state: keep the
            // project's TUNE (restored with PARAMS above) instead of zeroing it
            auto st = c.createCopy();
            for (int p = 0; p < NumParts && p < st.getNumChildren() - 1; ++p) {
                auto pv = st.getChild (p);
                if (! pv.hasProperty ("tune")) pv.setProperty ("tune", getTune (p), nullptr);
            }
            loadFullStateVT (st, false);   // a project load is not an undo step
        }
        if (c.hasType ("LOCKS"))
            for (int p = 0; p < NumParts; ++p) {
                auto bits = c.getProperty ("p" + juce::String (p)).toString();
                for (int k = 0; k < NumLocks; ++k) locks[p][k] = k < bits.length() && bits[k] == '1';
            }
    }
    pullApvtsToParts();
}

// ---- undo / redo ----
void DrumProcessor::pushUndoEntry (juce::ValueTree entry) {
    undoStack.push_back (std::move (entry));
    if (undoStack.size() > MaxUndo) undoStack.erase (undoStack.begin());
    redoStack.clear();   // a new edit forks history
}
void DrumProcessor::pushUndo() { pushUndoEntry (fullStateVT()); }

juce::ValueTree DrumProcessor::paramEntry (int index) const {
    juce::ValueTree e ("PARAM");
    e.setProperty ("index", index, nullptr);
    if (auto* prm = getParameters()[index]) e.setProperty ("value", prm->getValue(), nullptr);
    return e;
}
void DrumProcessor::applyParamEntry (const juce::ValueTree& e) {
    auto* prm = getParameters()[(int) e.getProperty ("index", -1)];
    if (prm == nullptr) return;
    const juce::ScopedValueSetter<bool> svs (gestureUndo.applying, true);
    prm->beginChangeGesture();   // hosts writing automation see a normal edit
    prm->setValueNotifyingHost ((float) e.getProperty ("value", prm->getValue()));
    prm->endChangeGesture();
    pullApvtsToParts();
    ++stateVersion;
}

void DrumProcessor::undo() {
    if (undoStack.empty()) return;
    auto v = undoStack.back(); undoStack.pop_back();
    if (v.hasType ("PARAM")) { redoStack.push_back (paramEntry ((int) v.getProperty ("index"))); applyParamEntry (v); return; }
    redoStack.push_back (fullStateVT());
    applyStateVT (v);
}
void DrumProcessor::redo() {
    if (redoStack.empty()) return;
    auto v = redoStack.back(); redoStack.pop_back();
    if (v.hasType ("PARAM")) { undoStack.push_back (paramEntry ((int) v.getProperty ("index"))); applyParamEntry (v); return; }
    undoStack.push_back (fullStateVT());
    applyStateVT (v);
}

// gestures arrive on the message thread (they come from a UI, never from host automation)
void DrumProcessor::GestureUndo::audioProcessorParameterChangeGestureBegin (juce::AudioProcessor*, int index) {
    if (applying || ! juce::MessageManager::existsAndIsCurrentThread() || index < 0 || index >= (int) startValue.size()) return;
    if (auto* prm = proc.getParameters()[index]) startValue[(size_t) index] = prm->getValue();
}
void DrumProcessor::GestureUndo::audioProcessorParameterChangeGestureEnd (juce::AudioProcessor*, int index) {
    if (applying || ! juce::MessageManager::existsAndIsCurrentThread() || index < 0 || index >= (int) startValue.size()) return;
    const float was = startValue[(size_t) index];
    startValue[(size_t) index] = -1.f;
    auto* prm = proc.getParameters()[index];
    if (was < 0.f || prm == nullptr || std::abs (prm->getValue() - was) < 1.0e-6f) return; // a click without a change
    auto e = proc.paramEntry (index);
    e.setProperty ("value", was, nullptr);
    proc.pushUndoEntry (e);
}

// ---- part clipboard ----
void DrumProcessor::pastePart (int part) {
    if (! partClipboard.isValid()) return;
    pushUndo();
    auto v = partClipboard.createCopy();
    const int src = (int) v.getProperty ("id", part);
    if (src != part) {
        // another part type: its synth slots and models mean other things here,
        // so this part keeps its own voice knobs and a model it lacks falls back to CLASSIC
        for (int s = 0; s < 20; ++s) v.setProperty ("p" + juce::String (s), parts[part].params.p[s], nullptr);
        const int model = (int) v.getProperty ("model", 0);
        auto a = modelNames (src), b = modelNames (part);
        if (model > 0 && (model >= a.size() || model >= b.size() || a[model] != b[model])) {
            v.setProperty ("model", 0, nullptr);
            for (int k = 0; k < NumModelParams; ++k) v.removeProperty ("m" + juce::String (k), nullptr);
        }
    }
    v.setProperty ("mute", parts[part].mute.load(), nullptr);   // pasting a sound, not mixer switches
    v.setProperty ("solo", parts[part].solo.load(), nullptr);
    partFromVT (part, v);
    applyKitToApvts(); pullApvtsToParts();
    updateReverseCaches();
    ++stateVersion;
}

void DrumProcessor::clearPart (int part) {
    pushUndo();
    defaultParamsForPart (part, parts[part].params);
    juce::ValueTree blank = parts[part].params.toVT();
    blank.setProperty ("mute", parts[part].mute.load(), nullptr);
    blank.setProperty ("solo", parts[part].solo.load(), nullptr);
    FxProcessor::clearChain (parts[part].fx);
    partFromVT (part, blank);   // default level / pan / sends / tune / choke / macros
    applyKitToApvts(); pullApvtsToParts();
    updateReverseCaches();
    ++stateVersion;
}

// ---- macros ----
void DrumProcessor::setMacroTarget (int part, int k, int target, float depth) {
    if (target < 0 || target >= NumMacroTargets) return;
    auto& m = parts[part].macros[k];
    for (auto& t : m.t)
        if (t.param.load() == target) {
            if (std::abs (depth) < 1.0e-4f) t.param = -1;
            else t.depth = depth;
            ++stateVersion;
            return;
        }
    if (std::abs (depth) < 1.0e-4f) return;
    for (auto& t : m.t)
        if (t.param.load() < 0) { t.depth = depth; t.param = target; ++stateVersion; return; }
}

float DrumProcessor::macroDepth (int part, int k, int target) const {
    for (auto& t : parts[part].macros[k].t) if (t.param.load() == target) return t.depth.load();
    return 0.f;
}

bool DrumProcessor::macroTargetValid (int part, int t) const {
    if (t >= 0 && t < 20) return slotMeta (part, t) != nullptr;
    if (t >= MT_Model0 && t < MT_Model0 + NumModelParams) {
        auto* md = modelDef (part, parts[part].params.model);
        return md != nullptr && t - MT_Model0 < md->nparams;
    }
    return t >= MT_Level && t < NumMacroTargets;
}

juce::String DrumProcessor::macroTargetLabel (int part, int t) const {
    if (t >= 0 && t < 20) { if (auto* m = slotMeta (part, t)) return m->label; return "SLOT " + juce::String (t + 1); }
    if (t >= MT_Model0 && t < MT_Model0 + NumModelParams) {
        if (auto* md = modelDef (part, parts[part].params.model))
            if (t - MT_Model0 < md->nparams) return md->p[t - MT_Model0].label;
        return "MODEL " + juce::String (t - MT_Model0 + 1);
    }
    switch (t) {
        case MT_Level: return "LEVEL"; case MT_Pan: return "PAN";
        case MT_SendRev: return "SEND REV"; case MT_SendDly: return "SEND DLY"; case MT_Tune: return "TUNE";
        default: return {};
    }
}

// the default target set of a part (for its engine model): { macro, target, depth }
static std::vector<std::array<float, 3>> defaultMacroTargets (int part, int model) {
    std::vector<std::array<float, 3>> out;
    out.push_back ({ 0.f, (float) DrumProcessor::MT_Tune, 0.25f });   // +-12 st
    out.push_back ({ 1.f, 5.f, 0.5f });                               // DECAY (slot 5 on every CLASSIC part)
    for (int s = 8; s < 20; ++s)
        if (auto* m = slotMeta (part, s))
            if (juce::String (m->label).startsWithIgnoreCase ("DRIVE")) { out.push_back ({ 2.f, (float) s, 0.35f }); break; }
    if (auto* md = modelDef (part, model))
        for (int k = 0; k < md->nparams; ++k) {
            juce::String l (md->p[k].label);
            if (l == "DECAY" || l == "TAIL DECAY" || l == "BODY DECAY") out.push_back ({ 1.f, (float) (DrumProcessor::MT_Model0 + k), 0.5f });
            if (l == "DRIVE") out.push_back ({ 2.f, (float) (DrumProcessor::MT_Model0 + k), 0.35f });
        }
    out.push_back ({ 3.f, (float) DrumProcessor::MT_SendRev, 0.5f });
    out.push_back ({ 3.f, (float) DrumProcessor::MT_SendDly, 0.25f });
    return out;
}

void DrumProcessor::defaultMacros (int part) {
    // same four on every part, centre = neutral. LENGTH / COLOR reach the CLASSIC
    // voice's DECAY / DRIVE slots and the current engine model's own DECAY / DRIVE knobs
    static const char* names[NumMacros] = { "PITCH", "LENGTH", "COLOR", "SPACE" };
    auto& pe = parts[part];
    for (int k = 0; k < NumMacros; ++k) {
        pe.macros[k].name = names[k];
        for (auto& t : pe.macros[k].t) { t.param = -1; t.depth = 0.f; }
        if (auto* mp = apvts.getParameter (macroParamID (part, k))) mp->setValueNotifyingHost (0.5f);
    }
    for (auto& d : defaultMacroTargets (part, pe.params.model)) setMacroTarget (part, (int) d[0], (int) d[1], d[2]);
}

// ---- randomize / mutate ----
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
            setSend (p, false, roomy && liveRng.nextFloat() < 0.5f ? liveRng.nextFloat() * 0.3f * intensity : 0.f);
        if (! isLocked (p, LockSendDly))
            setSend (p, true, p != Kick && liveRng.nextFloat() < 0.25f ? liveRng.nextFloat() * 0.25f * intensity : 0.f);
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
    int newModel = juce::jlimit (0, numModels (part) - 1, model);
    if (newModel != pp.model)   // knob locks belong to the old model's knobs
        for (int k = 0; k < NumModelParams; ++k) setLocked (part, modelKnobLock (k), false);
    // untouched default macros follow the model (its own DECAY / DRIVE knobs)
    bool stockMacros = true;
    {
        auto def = defaultMacroTargets (part, pp.model);
        int n = 0;
        for (int k = 0; k < NumMacros; ++k) for (auto& t : parts[part].macros[k].t) if (t.param.load() >= 0) ++n;
        stockMacros = n == (int) def.size();
        for (auto& d : def) stockMacros = stockMacros && std::abs (macroDepth (part, (int) d[0], (int) d[1]) - d[2]) < 1.0e-4f;
    }
    if (stockMacros && newModel != pp.model) {
        for (auto& d : defaultMacroTargets (part, pp.model)) setMacroTarget (part, (int) d[0], (int) d[1], 0.f);
        for (auto& d : defaultMacroTargets (part, newModel)) setMacroTarget (part, (int) d[0], (int) d[1], d[2]);
    }
    pp.model = newModel;
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
    auto& pp = parts[part].params;
    // a locked knob keeps its model too (knob k means something else in another model)
    if (! isLocked (part, LockModel) && ! anyModelKnobLocked (part) && liveRng.nextFloat() < 0.35f * intensity + 0.1f) {
        int model = liveRng.nextInt (numModels (part));
        if (model != pp.model) { pp.model = model; modelDefaults01 (part, model, pp.m); }
    }
    if (auto* md = modelDef (part, pp.model))
        for (int k = 0; k < md->nparams; ++k) {
            if (modelParamIsEnvelope (md->p[k]) || isLocked (part, modelKnobLock (k))) continue;
            float def = modelUncook (md->p[k], md->p[k].def);
            float wild = def + (liveRng.nextFloat() * 2.f - 1.f) * 0.35f * intensity;
            pp.m[k] = juce::jlimit (0.f, 1.f, pp.m[k] * (1.f - intensity) + wild * intensity);
        }
}

// MUTATE never changes the model, only nudges its knobs
void DrumProcessor::mutateModel (int part, float amount) {
    auto& pp = parts[part].params;
    if (pp.model <= 0) return;
    if (auto* md = modelDef (part, pp.model))
        for (int k = 0; k < md->nparams; ++k)
            if (! modelParamIsEnvelope (md->p[k]) && ! isLocked (part, modelKnobLock (k))) pp.m[k] = juce::jlimit (0.f, 1.f, pp.m[k] + (liveRng.nextFloat() * 2.f - 1.f) * amount * 0.25f);
}

// ---- UI thumbnails ----
std::vector<float> DrumProcessor::renderThumbnail (int p, int buckets, juce::uint64& sig) {
    if (p < 0 || p >= NumParts || buckets <= 0) return {};
    float off[NumMacroTargets];
    macroOffsets (p, off);
    PartParams pp;
    defaultParamsForPart (p, pp);
    for (int s = 0; s < 20; ++s)
        if (slotRaw[p][s]) pp.p[s] = applyMacro (slotRange[p][s], slotRaw[p][s]->load(), off[s]);
    pp.wave = juce::jlimit (0, 4, (int) std::round (waveRaw[p]->load()));
    pp.noiseType = juce::jlimit (0, 7, (int) std::round (noiseRaw[p]->load()));
    readPartFromApvts (p, pp);
    const float tuneOff = applyMacro (tuneRange, tuneRaw[p]->load(), off[MT_Tune]);
    const bool reversed = pp.reverse;
    juce::uint64 h = 1469598103934665603ull ^ (juce::uint64) buckets;
    auto mixIn = [&h] (const void* d, size_t nBytes) {
        auto* b = static_cast<const unsigned char*> (d);
        for (size_t i = 0; i < nBytes; ++i) { h ^= b[i]; h *= 1099511628211ull; }
    };
    mixIn (pp.p, sizeof (pp.p)); mixIn (&pp.wave, sizeof (int)); mixIn (&pp.noiseType, sizeof (int));
    mixIn (&pp.model, sizeof (int)); mixIn (pp.m, sizeof (pp.m)); mixIn (&tuneOff, sizeof (float)); mixIn (&reversed, sizeof (bool));
    if (h == sig) return {};
    sig = h;
    constexpr float sr = 16000.f;
    auto v = std::make_unique<DrumVoice>();
    v->prepare (sr);
    v->noise.rng.setSeed (7777 + p);
    pp.reverse = false;
    v->trigger (p, pp, 1.0f, tuneOff);
    std::vector<float> data;
    const int maxLen = (int) (sr * 1.5f);
    data.reserve ((size_t) maxLen);
    for (int i = 0; i < maxLen && v->active; ++i) data.push_back (v->renderWithParams (pp));
    if (reversed) std::reverse (data.begin(), data.end());
    std::vector<float> out ((size_t) buckets, 0.f);
    if (data.empty()) return out;
    // fit the sound's own length (trimmed where it falls below -54 dB of its peak)
    float pk = 0.f;
    for (float x : data) pk = std::max (pk, std::abs (x));
    size_t len = data.size();
    if (! reversed) while (len > 1 && std::abs (data[len - 1]) < pk * 0.002f) --len;
    len = std::max (len, (size_t) (0.05f * sr));
    pk = 0.f;
    for (int b = 0; b < buckets; ++b) {
        const size_t i0 = (size_t) ((double) b / buckets * (double) len), i1 = (size_t) ((double) (b + 1) / buckets * (double) len);
        float m = 0.f;
        for (size_t i = i0; i < i1 && i < data.size(); ++i) m = std::max (m, std::abs (data[i]));
        out[(size_t) b] = m; pk = std::max (pk, m);
    }
    if (pk > 0.f) for (auto& x : out) x /= pk;
    return out;
}

// ---- REVERSE ----
// The part's sound (whatever engine) is rendered once at full velocity, trimmed, faded and
// reversed on the message thread; hits then play that buffer. Re-rendered when any setting
// that shapes the sound changes. The pool keeps every buffer alive until no voice holds it,
// so the audio thread never frees memory.
void DrumProcessor::updateReverseCaches (bool force) {
    const double sr = sampleRate;
    const double now = juce::Time::getMillisecondCounterHiRes();
    constexpr double SettleMs = 150.0, MaxWaitMs = 500.0;   // knob drags: re-render on pauses, at most every 0.5 s
    for (int p = 0; p < NumParts; ++p) {
        auto& pe = parts[p];
        float off[NumMacroTargets];
        macroOffsets (p, off);
        PartParams pp;
        defaultParamsForPart (p, pp);
        for (int s = 0; s < 20; ++s)
            if (slotRaw[p][s]) pp.p[s] = applyMacro (slotRange[p][s], slotRaw[p][s]->load(), off[s]);
        pp.wave = juce::jlimit (0, 4, (int) std::round (waveRaw[p]->load()));
        pp.noiseType = juce::jlimit (0, 7, (int) std::round (noiseRaw[p]->load()));
        readPartFromApvts (p, pp);
        if (! pp.reverse) {
            if (pe.revCur != nullptr) { const juce::SpinLock::ScopedLockType sl (pe.revLock); pe.revCur = nullptr; }
            pe.revSig = 0; revPendSig[p] = 0;
            continue;
        }
        float tuneOff = applyMacro (tuneRange, tuneRaw[p]->load(), off[MT_Tune]);
        juce::uint64 h = 1469598103934665603ull;
        auto mixIn = [&h] (const void* d, size_t nBytes) {
            auto* b = static_cast<const unsigned char*> (d);
            for (size_t i = 0; i < nBytes; ++i) { h ^= b[i]; h *= 1099511628211ull; }
        };
        mixIn (pp.p, sizeof (pp.p)); mixIn (&pp.wave, sizeof (int)); mixIn (&pp.noiseType, sizeof (int));
        mixIn (&pp.model, sizeof (int)); mixIn (pp.m, sizeof (pp.m)); mixIn (&tuneOff, sizeof (float)); mixIn (&sr, sizeof (double));
        if (h == pe.revSig && pe.revCur != nullptr) continue;
        // throttle: while the knobs of a reversed part move, every timer tick has a
        // new signature. Wait until it holds still for SettleMs (or MaxWaitMs passed).
        if (! force && pe.revCur != nullptr) {
            if (h != revPendSig[p]) {
                if (revPendSig[p] == 0) revPendFirst[p] = now;
                revPendSig[p] = h; revPendSince[p] = now;
                if (now - revPendFirst[p] < MaxWaitMs) continue;
            } else if (now - revPendSince[p] < SettleMs && now - revPendFirst[p] < MaxWaitMs) continue;
        }
        revPendSig[p] = 0;
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
