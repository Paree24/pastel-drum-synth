// UI snapshot tool (dev only, never shipped): opens the real editor
// offscreen, clicks through views/tabs and writes PNGs for inspection.
//   UiShot <outDir>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>

using namespace mds;

static void pump() { juce::MessageManager::getInstance()->runDispatchLoopUntil (60); }

// click every visible TextButton whose text matches (private members are
// reachable through the component tree)
static int click (juce::Component& root, const juce::String& text, int nth = -1) {
    // collect first: a click can rebuild the very children being walked
    std::vector<juce::Component::SafePointer<juce::Button>> found;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c) {
        if (! c.isVisible()) return; // offscreen editor: isShowing() is always false
        if (auto* b = dynamic_cast<juce::Button*> (&c))
            if (b->getButtonText() == text) found.push_back (b);
        for (auto* ch : c.getChildren()) walk (*ch);
    };
    walk (root);
    int hit = 0;
    for (int i = 0; i < (int) found.size(); ++i) {
        auto* b = found[(size_t) i].getComponent();
        if (b == nullptr || (nth >= 0 && i != nth)) continue;
        if (b->getClickingTogglesState()) b->setToggleState (true, juce::dontSendNotification);
        if (b->onClick) b->onClick();
        ++hit;
    }
    pump();
    return hit;
}

static void shot (juce::Component& c, const juce::File& f) {
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    printf ("wrote %s (%dx%d)\n", f.getFileName().toRawUTF8(), img.getWidth(), img.getHeight());
}

int main (int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    juce::File out = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile ("uishot");
    out.createDirectory();
    DrumProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    auto& e = *ed;
    e.setVisible (true);
    pump();
    printf ("editor %dx%d\n", e.getWidth(), e.getHeight());

    shot (e, out.getChildFile ("01_kick_synth.png"));
    proc.selectedPart = Snare;
    proc.setLocked (Snare, 8, true);                       // BODY
    proc.setLocked (Snare, DrumProcessor::LockWave, true); // WAVE label
    proc.setLocked (Snare, DrumProcessor::LockFx, true);   // FX chain
    click (e, "SYNTH");
    // right-click path: toggle the TUNE knob's lock through its slider callback
    std::function<void (juce::Component&)> rc = [&] (juce::Component& c) {
        if (auto* k = dynamic_cast<Knob*> (&c)) if (k->lab.getText() == "TUNE" && k->s.onRightClick) { k->s.onRightClick(); return; }
        for (auto* ch : c.getChildren()) rc (*ch);
    };
    rc (e);
    printf ("TUNE locked via right-click: %d\n", (int) proc.isLocked (Snare, 0));
    shot (e, out.getChildFile ("02_snare_synth.png"));
    click (e, "AMP");    shot (e, out.getChildFile ("03_snare_amp.png"));
    click (e, "PITCH");  shot (e, out.getChildFile ("04_snare_pitch.png"));
    click (e, "FX SEND"); shot (e, out.getChildFile ("05_snare_send.png"));
    proc.selectedPart = ClosedHat;
    click (e, "SYNTH");
    shot (e, out.getChildFile ("06_chh_synth.png"));
    proc.selectedPart = Metallic;
    click (e, "SYNTH");
    shot (e, out.getChildFile ("06b_metal_synth.png"));
    click (e, "PITCH");
    shot (e, out.getChildFile ("06d_metal_pitch.png"));
    proc.selectedPart = Sfx;
    click (e, "SYNTH");
    shot (e, out.getChildFile ("06c_sfx_synth.png"));

    // snare part FX with a reverb selected
    proc.selectedPart = Snare;
    FxProcessor::clearChain (proc.parts[Snare].fx);
    FxProcessor::defaultSlot (FX_REVERB, proc.parts[Snare].fx.slots[0]);
    FxProcessor::defaultSlot (FX_DIST, proc.parts[Snare].fx.slots[1]);
    click (e, "PART FX");
    if (auto* v = dynamic_cast<DrumEditor&> (e).partFxViewForTest()) { v->selected = 0; v->refresh(); }
    pump();
    shot (e, out.getChildFile ("07_snare_partfx_reverb.png"));
    if (auto* v = dynamic_cast<DrumEditor&> (e).partFxViewForTest()) { v->selected = 1; v->refresh(); }
    pump();
    shot (e, out.getChildFile ("08_snare_partfx_dist.png"));
    click (e, "GLOBAL FX");
    shot (e, out.getChildFile ("09_global_fx.png"));

    // preset browser: the header button shows the kit name
    click (e, "SYNTH");
    int opened = click (e, proc.kitName);
    printf ("browser button clicks: %d (kit '%s')\n", opened, proc.kitName.toRawUTF8());
    shot (e, out.getChildFile ("10_browser.png"));
    click (e, "SAVE"); // inline: no window, button arms/saves in place
    shot (e, out.getChildFile ("11_browser_after_save.png"));
    ed.reset();
    return 0;
}
