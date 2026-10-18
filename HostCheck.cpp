// VST3 host check (dev only, never shipped): loads the BUILT .vst3 through
// JUCE's VST3 hosting, like a DAW would: buses, params, audio, state.
//   HostCheck <path/to/Pastel Drum Synth.vst3>
#include <JuceHeader.h>
#include <cstdio>

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { printf ("usage: HostCheck <plugin.vst3>\n"); return 1; }
    int fails = 0;
    auto check = [&] (bool ok, const juce::String& what) { printf ("%s  %s\n", ok ? "  ok " : "FAIL ", what.toRawUTF8()); if (! ok) ++fails; };
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile (descs, juce::File (argv[1]).getFullPathName());
    check (descs.size() == 1, "VST3 scanned: " + juce::String (descs.size()) + " plugin(s)");
    if (descs.isEmpty()) return 1;
    auto& d = *descs[0];
    check (d.isInstrument, "registered as instrument: " + d.name + " / " + d.category);
    juce::String err;
    auto inst = fmt.createInstanceFromDescription (d, 48000.0, 512, err);
    check (inst != nullptr, "instantiated " + err);
    if (! inst) return 1;
    check (inst->getBusCount (false) == 13, "output buses: " + juce::String (inst->getBusCount (false)));
    check (inst->getParameters().size() >= 12 * 50 + 1, "parameters: " + juce::String (inst->getParameters().size()));
    check (inst->enableAllBuses(), "all 13 stereo buses enabled");
    inst->prepareToPlay (48000.0, 512);
    int nch = inst->getTotalNumOutputChannels();
    check (nch == 26, "26 output channels");
    juce::AudioBuffer<float> buf (nch, 512);
    float mainPeak = 0.f;
    for (int b = 0; b < 40; ++b) {
        juce::MidiBuffer mb;
        if (b == 1) mb.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 120), 0);
        buf.clear();
        inst->processBlock (buf, mb);
        mainPeak = juce::jmax (mainPeak, buf.getMagnitude (0, 0, 512), buf.getMagnitude (1, 0, 512));
    }
    check (mainPeak > 0.05f, "note 36 plays the default factory kit on Main (peak " + juce::String (mainPeak, 3) + ")");
    juce::MemoryBlock st;
    inst->getStateInformation (st);
    check (st.getSize() > 1000, "state saved (" + juce::String ((int) st.getSize()) + " bytes)");
    inst->setStateInformation (st.getData(), (int) st.getSize());
    check (true, "state restored");
    std::unique_ptr<juce::AudioProcessorEditor> ed (inst->createEditorIfNeeded());
    check (ed != nullptr && ed->getWidth() == 1280, "editor opens at 1280x800");
    ed.reset();
    inst->releaseResources();
    inst.reset();
    printf (fails == 0 ? "HOST CHECK PASSED\n" : "%d HOST CHECK(S) FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
