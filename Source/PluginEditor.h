#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Visuals.h"

namespace mds {

class PresetBrowser;

struct PastelLNF : juce::LookAndFeel_V4 {
    PastelLNF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Colour accent = juce::Colour (0xfff49ac1);
};

// slider that hands right-clicks to a callback (lock toggle) instead of dragging
struct LockSlider : juce::Slider {
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; } juce::Slider::mouseDown (e); }
    void mouseDrag (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseDrag (e); }
    void mouseUp (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e); }
};

// label that toggles a lock on right-click (WAVE / NOISE selectors)
struct LockLabel : juce::Label {
    using juce::Label::Label;
    bool locked = false;
    std::function<void()> onRightClick;
    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu() && onRightClick) onRightClick(); }
    void paint (juce::Graphics&) override;
};

void drawPadlock (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c);

// compact labelled knob (right-click = randomize lock when lockable)
struct Knob : juce::Component {
    Knob (const juce::String& label, juce::Colour ac);
    void attach (juce::AudioProcessorValueTreeState& ap, const juce::String& id);
    void setManual (float v, std::function<void(float)> cb, std::function<juce::String(float)> fmt = {});
    void setValue01 (float v01, bool notify = true);
    void resized() override;
    LockSlider s; juce::Label lab, val;
    bool locked = false; // shown as a padlock; set by the owner
    void paintOverChildren (juce::Graphics&) override;
    juce::Colour accent;
    std::function<void(float)> onChange;
    std::function<juce::String(float)> fmtr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

class DrumEditor;

// the four macros of the selected part (MACROS tab)
struct MacroPanel : juce::Component {
    explicit MacroPanel (DrumEditor& e);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
    void showTargetMenu (int k);
    DrumEditor& ed;
    std::unique_ptr<Knob> knobs[DrumProcessor::NumMacros];
    juce::Label names[DrumProcessor::NumMacros];
    juce::Rectangle<int> targetRects[DrumProcessor::NumMacros];
};

// 12-pad selector card
struct PartCard : juce::Component {
    PartCard (int idx, DrumProcessor& pr);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    int idx; DrumProcessor& proc;
    Knob tune, level;
    juce::TextButton solo, mute;
};

class DrumEditor : public juce::AudioProcessorEditor, public juce::Timer {
public:
    // fx chain view: chip strip (slots) + roomy tabbed detail editor
    struct FxChainView : juce::Component {
        FxChainView (DrumEditor& ed, bool global);
        void refresh(); // rebind after part/kit/type/selection change
        void resized() override;
        float getParam (int slot, int k) const;
        void setParam (int slot, int k, float v);
        void selectSlot (int slot);
        void moveSlot (int slot, int dir); // reorder active chip (< > buttons)
        FxChainState& chain();
        int selected = 0;
        struct Chip : juce::TextButton {
            Chip (FxChainView& v, int slot);
            FxChainView& view; int slot;
        };
        DrumEditor& ed; bool isGlobal;
        juce::ComboBox typeBox;
        juce::TextButton bypassB { "ON" }, addB { "+ ADD" }, delB { "DEL" }, upB { "<" }, dnB { ">" };
        juce::Label emptyLab { {}, "EMPTY - click + ADD to insert an effect" };
        std::vector<std::unique_ptr<Knob>> knobs;
        std::vector<std::unique_ptr<Chip>> chips;
    };

    DrumEditor (DrumProcessor&);
    ~DrumEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    // headless UI-logic test hooks (no windows needed)
    FxChainView* partFxViewForTest() { return partFxView.get(); }
    int selectedPartForTest() const { return proc.selectedPart.load(); }

    DrumProcessor& proc;
    int sel() const { return proc.selectedPart.load(); }
    void refreshAll() { rebuildSynthPanel(); refreshFxRows(); }

private:
    PastelLNF lnf;
    juce::TooltipWindow tooltips { this, 700 };   // child of the editor: setTooltip() texts show up in every host

    // header
    juce::TextButton presetBtn { "Init" };
    juce::TextButton prevB { "<" }, nextB { ">" }, randPartB { "RANDOMIZE" }, mutPartB { "MUTATE" },
                     randKitB { "KIT RND" }, undoB { "UNDO" }, redoB { "REDO" }, initB { "INIT" };
    std::unique_ptr<PresetBrowser> browser;
    juce::ComboBox scopeBox; // Part / Kit for rand/mut
    juce::Slider masterDial; juce::Label masterLab, presetLab, kitLab;
    juce::Slider masterFader { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox }; // before the attachments: they must die first
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterDialAtt, masterFaderAtt;
    juce::TextButton limitB { "LIMITER" };

    // part cards
    juce::Component cardRow;
    std::vector<std::unique_ptr<PartCard>> cards;

    // scope + analyzer (follow selected part)
    ScopeSource selectedScope() const;
    ScopeView scopeView { [this] { return selectedScope(); } };
    SpectrumView analyzerView { [this] { return selectedScope(); } };
    MeterComp meterComp { [this] { return proc.outLevel.load(); } };
    // main view tabs: synth engine | per-part FX | global FX
    juce::TextButton viewSynth { "SYNTH" }, viewPartFx { "PART FX" }, viewGlobFx { "GLOBAL FX" };
    int mainView = 0;
    juce::Rectangle<int> footerRect;
    // synth panel
    juce::Component synthPanel, fxPanel, globalPanel, outPanel, footPanel;
    juce::TextButton tabSynth { "SYNTH" }, tabAmp { "AMP" }, tabPitch { "PITCH" }, tabSend { "FX SEND" }, tabMacro { "MACROS" };
    std::unique_ptr<MacroPanel> macroPanel;
    int tab = 0;
    juce::Label partTitle;
    std::vector<std::unique_ptr<Knob>> synthKnobs;
    juce::ComboBox waveBox, noiseBox;
    LockLabel waveLab { {}, "WAVE" }, noiseLab { {}, "NOISE" };
    juce::Label lockHint { {}, "RIGHT-CLICK = LOCK" };
    juce::TextButton trigB { "TRIGGER" };
    // engine model / reverse / output routing for the selected part
    juce::ComboBox modelBox, outBox, chokeBox;
    LockLabel modelLab { {}, "MODEL" };
    juce::Label outLab { {}, "OUT" }, chokeLab { {}, "CHOKE" };
    juce::TextButton revB { "REVERSE" };
    int shownModel() const;

    std::unique_ptr<FxChainView> partFxView, globFxView;
    juce::Label fxTitle { {}, "EFFECTS CHAIN" }, fxHint { {}, "USE < > TO REORDER" };
    juce::Label gloTitle { {}, "GLOBAL EFFECTS" };
    juce::TextButton partFxPower { "ON" }, globPower { "ON" }, partFxLock { "LOCK" };

    juce::Label outTitle { {}, "OUTPUT" };

    void rebuildSynthPanel();
    void applyPartAccent (int part);
    void refreshFxRows();
    void layoutMainView(); // main tab visibility
public:
    void selectPart (int p);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumEditor)
};

} // namespace mds
