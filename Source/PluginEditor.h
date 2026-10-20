#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Visuals.h"
#include "FxPicture.h"
#include "PastelViews.h"

namespace mds {

class PresetBrowser;

// v3 design (after Pastel EFX Grid; see mockup/redesign_v3.png): charcoal
// panels with thin edges, the selected part's pastel as the one accent,
// arc knobs, live views you drag, embedded Lato.
namespace DrumColors {
    const juce::Colour bg      { 0xff141518 };
    const juce::Colour panel   { 0xff1c1f24 };
    const juce::Colour inner   { 0xff16181c };
    const juce::Colour grid    { 0xff24272c };
    const juce::Colour border  { 0xff33363c };
    const juce::Colour edge    { 0xff3a3d43 };
    const juce::Colour control { 0xff232529 };
    const juce::Colour text    { 0xffd9d5d0 };
    const juce::Colour textDim { 0xff8f8b85 };
    const juce::Colour faint   { 0xff5c5a56 };
    const juce::Colour handle  { 0xfff4f1ec };
    const juce::Colour ink     { 0xff1b1d22 };
    const juce::Colour amber   { 0xffe8b86a }; // padlocks
}

struct PastelLNF : juce::LookAndFeel_V4 {
    PastelLNF();
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Typeface::Ptr latoReg, latoBold, latoBlack;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Colour accent = juce::Colour (0xffe9a3b8);
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

// SOUND page hero: the part's rendered hit, with its amp envelope and pitch
// sweep stacked on top as overlay views you drag (classic voice only)
struct HitView : juce::Component {
    void paint (juce::Graphics&) override;
    void resized() override;
    std::vector<float> wave;       // normalised peaks over 1.5 s
    juce::Colour colour;
    juce::String note;             // shown when there is nothing to drag
    std::unique_ptr<pastel::EnvView> amp;
    std::unique_ptr<pastel::SweepView> pitch;
};

// a titled card behind a group of knobs (dot + title, dark well)
struct CardBg : juce::Component {
    CardBg (const juce::String& t, juce::Colour c) : title (t), colour (c) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    juce::String title; juce::Colour colour;
};

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
    Knob tune, level;              // tune lives on the SOUND page now (hidden here); level is the bar
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
        FxPicture picture;    // v3: the selected effect, drawn and dragged (the knobs stay in sync)
        void syncKnobs();
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
    MeterComp meterComp { [this] { return proc.outLevel.load(); }, false };   // header, horizontal
    // main view tabs: synth engine | per-part FX | global FX
    juce::TextButton viewSynth { "SYNTH" }, viewPartFx { "PART FX" }, viewGlobFx { "GLOBAL FX" };
    int mainView = 0;
    juce::Rectangle<int> footerRect, contentRect;
    // synth panel
    juce::Component synthPanel, fxPanel, globalPanel, outPanel, footPanel;
    // pages of the part editor (AMP / PITCH live on the SOUND page's hit graph now)
    juce::TextButton tabSynth { "SOUND" }, tabAmp { "AMP" }, tabPitch { "PITCH" }, tabSend { "RETURNS" }, tabMacro { "MACROS" };
    std::unique_ptr<MacroPanel> macroPanel;
    int tab = 0;
    juce::Label partTitle;
    std::vector<std::unique_ptr<Knob>> synthKnobs;
    std::unique_ptr<HitView> hitView;               // SOUND page: the hit you drag
    juce::uint64 hitSig = 0; int hitPart = -1;
    std::vector<std::unique_ptr<CardBg>> cardBgs;   // titled groups behind the knobs
    struct Group { juce::Rectangle<int> bounds; std::vector<Knob*> knobs; };
    std::vector<Group> groups;
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
