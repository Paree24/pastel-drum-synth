#include "PluginEditor.h"
#include "PresetBrowser.h"

namespace mds {

// ================= LNF =================
PastelLNF::PastelLNF() {
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff101014));
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c1c22));
    setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8e8ee));
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff232330));
    setColour (juce::TextButton::textColourOnId, juce::Colour (0xff101014));
    setColour (juce::Label::textColourId, juce::Colour (0xffb8b8c4));
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::trackColourId, juce::Colour (0xff2a2a34));
}
void PastelLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float start, float end, juce::Slider& sl) {
    auto ac = sl.findColour (juce::Slider::rotarySliderFillColourId);
    auto cx = x + w * 0.5f, cy = y + h * 0.5f - 6.f, r = juce::jmin (w, h) * 0.32f;
    g.setColour (juce::Colour (0xff1e1e26));
    g.fillEllipse (cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2);
    g.setColour (juce::Colour (0xff2c2c38));
    g.drawEllipse (cx - r - 3, cy - r - 3, (r + 3) * 2, (r + 3) * 2, 1.2f);
    // track
    juce::Path track;
    track.addArc (cx - r, cy - r, r * 2, r * 2, start, end, true);
    g.setColour (juce::Colour (0xff33333f));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path fill;
    fill.addArc (cx - r, cy - r, r * 2, r * 2, start, start + (end - start) * pos, true);
    g.setColour (ac);
    g.strokePath (fill, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    float ang = start + (end - start) * pos;
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.drawLine (cx, cy, cx + std::cos (ang - juce::MathConstants<float>::halfPi) * r * 0.75f,
                cy + std::sin (ang - juce::MathConstants<float>::halfPi) * r * 0.75f, 2.f);
}
void PastelLNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float, float, juce::Slider::SliderStyle style, juce::Slider& sl) {
    if (style != juce::Slider::LinearVertical) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, 0, 1, style, sl); return; }
    auto ac = sl.findColour (juce::Slider::rotarySliderFillColourId);
    float cx = x + w * 0.5f;
    g.setColour (juce::Colour (0xff26262f));
    g.fillRoundedRectangle (cx - 3, (float) y + 4, 6, (float) h - 8, 3.f);
    float fh = (h - 12) * pos;
    g.setColour (ac.withAlpha (0.85f));
    g.fillRoundedRectangle (cx - 3, (float) (y + h - 6) - fh, 6, fh, 3.f);
    g.setColour (juce::Colour (0xffd8d8e2));
    g.fillRoundedRectangle (cx - 11, (float) (y + h - 8) - fh, 22, 7, 2.f);
}
void PastelLNF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.f);
    g.setColour (juce::Colour (0xff34343f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.f, 1.f);
}
void PastelLNF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hov, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced (1);
    juce::Colour c = bg;
    if (b.getToggleState()) c = b.findColour (juce::Slider::rotarySliderFillColourId);
    else if (down) c = c.brighter (0.15f);
    else if (hov) c = c.brighter (0.07f);
    g.setColour (c);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (r, 5.f, 1.f);
}

// ================= Knob =================
Knob::Knob (const juce::String& label, juce::Colour ac) : accent (ac) {
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, ac);
    lab.setText (label, juce::dontSendNotification);
    lab.setFont (juce::Font (11.f, juce::Font::bold));
    lab.setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa8));
    lab.setJustificationType (juce::Justification::centred);
    val.setFont (juce::Font (11.5f));
    val.setColour (juce::Label::textColourId, ac.brighter (0.25f));
    val.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (s); addAndMakeVisible (lab); addAndMakeVisible (val);
    s.onValueChange = [this] {
        float v = (float) s.getValue();
        if (fmtr) val.setText (fmtr (v), juce::dontSendNotification);
        else val.setText (juce::String (v, 1), juce::dontSendNotification);
        if (onChange) onChange (v);
    };
}
void Knob::attach (juce::AudioProcessorValueTreeState& ap, const juce::String& id) {
    if (auto* pf = dynamic_cast<juce::AudioParameterFloat*> (ap.getParameter (id))) {
        // full range incl. skew (log knobs map correctly); slider works in double
        juce::NormalisableRange<double> r ((double) pf->range.start, (double) pf->range.end,
                                           (double) pf->range.interval, (double) pf->range.skew,
                                           pf->range.symmetricSkew);
        s.setNormalisableRange (r);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (ap, id, s);
        s.onValueChange();
    }
}
void Knob::setManual (float v, std::function<void(float)> cb, std::function<juce::String(float)> fmt) {
    onChange = cb; fmtr = fmt;
    s.setNormalisableRange ({ 0.0, 1.0 });
    s.setValue (v, juce::dontSendNotification);
    s.onValueChange();
}
void Knob::setValue01 (float v01, bool notify) {
    s.setValue (s.getMinimum() + v01 * (s.getMaximum() - s.getMinimum()),
                notify ? juce::sendNotification : juce::dontSendNotification);
}
void drawPadlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    auto body = r.removeFromBottom (r.getHeight() * 0.58f);
    g.setColour (c);
    g.fillRoundedRectangle (body, 1.5f);
    juce::Path shackle;
    float w = body.getWidth() * 0.62f, x = body.getCentreX() - w * 0.5f;
    shackle.addRoundedRectangle (x, r.getY() + 0.5f, w, r.getHeight() + body.getHeight() * 0.5f, w * 0.5f);
    g.strokePath (shackle, juce::PathStrokeType (1.6f));
}
void Knob::paintOverChildren (juce::Graphics& g) {
    if (! locked) return;
    auto sb = s.getBounds().toFloat();
    drawPadlock (g, { sb.getRight() - 15.f, sb.getY() + 3.f, 10.f, 12.f }, juce::Colour (0xfffbbf24));
}
void LockLabel::paint (juce::Graphics& g) {
    juce::Label::paint (g);
    if (locked) drawPadlock (g, { (float) getWidth() - 9.f, 2.f, 7.f, 9.f }, juce::Colour (0xfffbbf24));
}

void Knob::resized() {
    auto r = getLocalBounds();
    if (r.getHeight() < 30) { s.setBounds (r); lab.setBounds (0, 0, 0, 0); val.setBounds (0, 0, 0, 0); return; }
    val.setBounds (r.removeFromBottom (14));
    lab.setBounds (r.removeFromBottom (14));
    s.setBounds (r);
}

// ================= PartCard =================
PartCard::PartCard (int i, DrumProcessor& pr)
    : idx (i), proc (pr), tune ("TUNE", partColour (i)), level ("LEVEL", partColour (i)) {
    tune.s.setColour (juce::Slider::rotarySliderFillColourId, partColour (i));
    level.s.setColour (juce::Slider::rotarySliderFillColourId, partColour (i));
    tune.attach (pr.apvts, "tune" + juce::String (i));
    level.attach (pr.apvts, "mix" + juce::String (i) + "_lvl");
    tune.val.setVisible (false); level.val.setVisible (false);
    solo.setButtonText ("S"); mute.setButtonText ("M");
    solo.setClickingTogglesState (true); mute.setClickingTogglesState (true);
    solo.setColour (juce::Slider::rotarySliderFillColourId, partColour (i));
    mute.setColour (juce::Slider::rotarySliderFillColourId, partColour (i));
    solo.onClick = [this] { proc.parts[idx].solo = solo.getToggleState(); };
    mute.onClick = [this] { proc.parts[idx].mute = mute.getToggleState(); };
    addAndMakeVisible (tune); addAndMakeVisible (level);
    addAndMakeVisible (solo); addAndMakeVisible (mute);
}
void PartCard::paint (juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    bool isSel = proc.selectedPart.load() == idx;
    g.setColour (isSel ? juce::Colour (0xff22222c) : juce::Colour (0xff17171d));
    g.fillRoundedRectangle (r.reduced (1), 8.f);
    g.setColour (partColour (idx).withAlpha (isSel ? 0.9f : 0.45f));
    g.drawRoundedRectangle (r.reduced (1), 8.f, isSel ? 1.6f : 1.f);
    g.setColour (partColour (idx));
    g.setFont (juce::Font (12.f, juce::Font::bold));
    g.drawText (juce::String (partName (idx)), 4, 6, getWidth() - 44, 14, juce::Justification::centred);
    g.setColour (juce::Colour (0xff6a6a78));
    g.setFont (juce::Font (9.f));
    g.drawText (noteName (proc.parts[idx].midiNote), getWidth() - 42, 6, 38, 14, juce::Justification::centredRight);
    // engine model + REVERSE badge (blank for the CLASSIC voice)
    juce::String sub;
    if (auto* md = modelDef (idx, proc.parts[idx].params.model)) sub = md->name;
    if (proc.parts[idx].params.reverse) sub = sub.isEmpty() ? "REV" : sub + " / REV";
    if (sub.isNotEmpty()) {
        g.setColour (partColour (idx).withAlpha (0.75f));
        g.setFont (juce::Font (9.f, juce::Font::bold));
        g.drawText (sub, 4, 20, getWidth() - 8, 11, juce::Justification::centred);
    }
    // accent underline
    g.setColour (partColour (idx).withAlpha (0.8f));
    g.fillRoundedRectangle (8, (float) getHeight() - 7, (float) getWidth() - 16, 3, 1.5f);
}
void PartCard::mouseDown (const juce::MouseEvent& e) {
    if (e.mods.isPopupMenu()) {   // part clipboard
        juce::PopupMenu m;
        m.addSectionHeader (partName (idx));
        m.addItem (1, "Copy part");
        m.addItem (2, "Paste part", proc.partClipboard.isValid());
        m.addSeparator();
        m.addItem (3, "Clear part");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
            [sp = juce::Component::SafePointer<PartCard> (this)] (int id) {
                if (sp == nullptr || id <= 0) return;
                auto& p = sp->proc; const int i = sp->idx;
                if (id == 1) { p.copyPart (i); return; }
                if (id == 2) p.pastePart (i);
                else if (id == 3) p.clearPart (i);
                if (auto* ed = dynamic_cast<DrumEditor*> (p.getActiveEditor())) ed->selectPart (i);
            });
        return;
    }
    proc.selectedPart = idx;
    if (auto* ed = dynamic_cast<DrumEditor*> (proc.getActiveEditor())) ed->timerCallback();
}
void PartCard::resized() {
    auto r = getLocalBounds().reduced (5);
    r.removeFromTop (30); // name + model painted in paint()
    auto btns = r.removeFromBottom (24);
    solo.setBounds (btns.removeFromLeft (btns.getWidth() / 2).reduced (4, 3));
    mute.setBounds (btns.reduced (4, 3));
    r.removeFromBottom (4); // accent underline space
    tune.setBounds (r.removeFromLeft (r.getWidth() / 2).reduced (1));
    level.setBounds (r.reduced (1));
}

// ================= FxRow =================
static juce::String fxKnobText (int type, int k, float v01) {
    if ((type == FX_DIST && k == 3) || (type == FX_TRANSIENT && k == 4))
        return juce::String ((v01 * 2.f - 1.f) * 12.f, 1) + "dB";
    if (type == FX_EQ && k <= 3) return juce::String (v01 * 30.f - 15.f, 1) + "dB";
    if (type == FX_EQ && k == 4) return juce::String ((v01 * 2.f - 1.f) * 6.f, 1) + "dB";
    if (type == FX_COMP && k == 1) return juce::String (1.f + v01 * 19.f, 1) + ":1";
    if (type == FX_COMP && k == 2) return juce::String ((0.0005f + v01 * v01 * 0.15f) * 1000.f, 1) + "ms";
    if (type == FX_COMP && k == 3) return juce::String ((0.01f + v01 * v01 * 0.6f) * 1000.f, 0) + "ms";
    if (type == FX_COMP && k == 4) return "+" + juce::String (v01 * 6.f, 1) + "dB";
    if (type == FX_REVERB && k == 3) return juce::String (v01 * 200.f, 0) + "ms";
    if (type == FX_REVERB && k == 0) return juce::String (FxProcessor::rt60FromKnob (v01), 2) + "s";
    if (type == FX_DELAY && k == 3) return juce::String ((int) (20.f * std::pow (100.f, v01))) + "Hz";
    if (type == FX_TRANSIENT && k == 2) return juce::String (1.f * std::pow (60.f, v01), 1) + "ms";
    if (type == FX_DIST && k == 2) {
        static const char* algos[8] = { "Soft","Hard","Fuzz","Fold","Asym","Bit","Shape","Rect" };
        return algos[juce::jlimit (0, 7, (int) (v01 * 7.999f))];
    }
    if (type == FX_FILTER && k == 0)
        return juce::String ((int) (20.f * std::pow (900.f, v01))) + "Hz";
    if (type == FX_FILTER && k == 2) {
        static const char* modes[4] = { "LP","HP","BP","Notch" };
        return modes[juce::jlimit (0, 3, (int) (v01 * 3.999f))];
    }
    if (type == FX_FILTER && k == 3) return v01 < 0.5f ? "12dB" : "24dB";
    if (k == 5) return juce::String ((int) (v01 * 100)) + "%";
    switch (type) {
        case FX_COMP: if (k == 0) return juce::String (-48 + v01 * 48, 1) + "dB"; break;
        case FX_DELAY: if (k == 0) return juce::String ((0.02f + v01 * v01 * 1.2f) * 1000, 0) + "ms"; break;
        default: break;
    }
    return juce::String ((int) (v01 * 100)) + "%";
}

DrumEditor::FxChainView::FxChainView (DrumEditor& e, bool g) : ed (e), isGlobal (g) {
    for (int i = 0; i < 8; ++i) if (isUsableFx (i)) typeBox.addItem (fxTypeName (i), i + 1);
    typeBox.onChange = [this] {
        auto& ch = chain();
        if (selected < 0 || ch.slots[(size_t) selected].type.load() == FX_NONE) return;
        ch.slots[(size_t) selected].type = typeBox.getSelectedId() - 1;
        refresh();
    };
    bypassB.setClickingTogglesState (true);
    bypassB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa3e635));
    bypassB.onClick = [this] {
        auto& ch = chain();
        if (selected < 0) return;
        ch.slots[(size_t) selected].bypass.store (! bypassB.getToggleState());
        refresh();
    };
    addB.onClick = [this] {
        auto& ch = chain();
        int free = -1;
        for (int i = 0; i < FxChainState::NumSlots; ++i)
            if (ch.slots[(size_t) i].type.load() == FX_NONE) { free = i; break; }
        if (free < 0) return; // chain full
        juce::PopupMenu menu;
        for (int t = 0; t < 8; ++t) menu.addItem (t + 1, fxTypeName (t));
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addB),
            [this, free] (int id) {
                if (id <= 0) return;
                FxProcessor::defaultSlot (id - 1, chain().slots[(size_t) free]);
                selectSlot (free);
            });
    };
    delB.onClick = [this] {
        auto& ch = chain();
        if (selected < 0) return;
        FxProcessor::defaultSlot (FX_NONE, ch.slots[(size_t) selected]);
        selected = -1;
        refresh();
    };
    upB.onClick = [this] { moveSlot (selected, -1); };
    dnB.onClick = [this] { moveSlot (selected, +1); };
    emptyLab.setJustificationType (juce::Justification::centred);
    emptyLab.setColour (juce::Label::textColourId, juce::Colour (0xff6a6a78));
    addAndMakeVisible (typeBox); addAndMakeVisible (bypassB);
    addAndMakeVisible (addB); addAndMakeVisible (delB);
    addAndMakeVisible (upB); addAndMakeVisible (dnB);
    addAndMakeVisible (emptyLab);
    for (int i = 0; i < FxProcessor::MaxKnobs; ++i) {
        knobs.push_back (std::make_unique<Knob> ("", juce::Colour (0xfff49ac1)));
        addAndMakeVisible (*knobs.back());
    }
    refresh();
}

DrumEditor::FxChainView::Chip::Chip (FxChainView& v, int s) : view (v), slot (s) {
    onClick = [this] { view.selectSlot (slot); };
}

FxChainState& DrumEditor::FxChainView::chain() {
    return isGlobal ? ed.proc.globalFx : ed.proc.parts[ed.sel()].fx;
}
float DrumEditor::FxChainView::getParam (int slot, int k) const {
    auto& ch = isGlobal ? ed.proc.globalFx : ed.proc.parts[ed.sel()].fx;
    auto& sl = ch.slots[(size_t) slot];
    switch (k) {
        case 0: return sl.p0.load(); case 1: return sl.p1.load(); case 2: return sl.p2.load();
        case 3: return sl.p3.load(); case 4: return sl.p4.load(); default: return sl.p5.load();
    }
}
void DrumEditor::FxChainView::setParam (int slot, int k, float v) {
    auto& ch = isGlobal ? ed.proc.globalFx : ed.proc.parts[ed.sel()].fx;
    auto& sl = ch.slots[(size_t) slot];
    v = juce::jlimit (0.f, 1.f, v);
    switch (k) {
        case 0: sl.p0 = v; break; case 1: sl.p1 = v; break; case 2: sl.p2 = v; break;
        case 3: sl.p3 = v; break; case 4: sl.p4 = v; break; default: sl.p5 = v; break;
    }
}

void DrumEditor::FxChainView::selectSlot (int slot) {
    selected = slot;
    refresh();
}

static std::vector<int> activeSlotsInOrder (FxChainState& ch) {
    std::vector<int> out;
    bool seen[FxChainState::NumSlots] = {};
    for (int k = 0; k < FxChainState::NumSlots; ++k) {
        int s = ch.order[(size_t) k];
        if (s < 0 || s >= FxChainState::NumSlots || seen[(size_t) s]) continue; // corrupt order can never duplicate chips
        seen[(size_t) s] = true;
        if (ch.slots[(size_t) s].type.load() != FX_NONE) out.push_back (s);
    }
    return out;
}

void DrumEditor::FxChainView::moveSlot (int slot, int dir) {
    if (slot < 0) return;
    auto& ch = chain();
    auto act = activeSlotsInOrder (ch);
    int idx = -1;
    for (size_t i = 0; i < act.size(); ++i) if (act[i] == slot) idx = (int) i;
    if (idx < 0) return;
    int nidx = juce::jlimit (0, (int) act.size() - 1, idx + dir);
    if (nidx == idx) return;
    // swap the two slots' positions inside order[]
    int posA = -1, posB = -1;
    for (int k = 0; k < FxChainState::NumSlots; ++k) {
        if (ch.order[(size_t) k] == act[(size_t) idx]) posA = k;
        if (ch.order[(size_t) k] == act[(size_t) nidx]) posB = k;
    }
    if (posA < 0 || posB < 0) return;
    std::swap (ch.order[(size_t) posA], ch.order[(size_t) posB]);
    refresh();
}

void DrumEditor::FxChainView::refresh() {
    auto& ch = chain();
    if (selected < 0 || selected >= FxChainState::NumSlots || ch.slots[(size_t) selected].type.load() == FX_NONE) {
        auto act = activeSlotsInOrder (ch);
        selected = act.empty() ? -1 : act[0];
    }
    // rebuild chips (selection/type/count may all have changed)
    for (auto& c : chips) removeChildComponent (c.get());
    chips.clear();
    auto act = activeSlotsInOrder (ch);
    static const juce::uint32 kcols[8] = { 0xfff49ac1,0xfff87171,0xfffb923c,0xffa3e635,0xff22d3ee,0xffa78bfa,0xff34d399,0xff7dd3fc };
    for (size_t i = 0; i < act.size(); ++i) {
        int s = act[i];
        auto cp = std::make_unique<Chip> (*this, s);
        int t = ch.slots[(size_t) s].type.load();
        cp->setButtonText (juce::String ((int) i + 1) + " " + fxTypeName (t));
        cp->setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (kcols[t < 8 ? t : 0]));
        cp->setClickingTogglesState (true);
        cp->setToggleState (s == selected, juce::dontSendNotification);
        addAndMakeVisible (*cp);
        chips.push_back (std::move (cp));
    }
    bool hasSel = selected >= 0 && ch.slots[(size_t) selected].type.load() != FX_NONE;
    int t = hasSel ? ch.slots[(size_t) selected].type.load() : 0;
    int nk = FxProcessor::numKnobs (t);
    typeBox.setVisible (hasSel); bypassB.setVisible (hasSel);
    for (auto& k : knobs) k->setVisible (hasSel);
    emptyLab.setVisible (! hasSel);
    if (hasSel) {
        auto& sl = ch.slots[(size_t) selected];
        typeBox.setSelectedId (t + 1, juce::dontSendNotification);
        bypassB.setToggleState (! sl.bypass.load(), juce::dontSendNotification);
        bypassB.setButtonText (sl.bypass.load() ? "OFF" : "ON");
        auto c = juce::Colour (kcols[t < 8 ? t : 0]);
        for (int k = 0; k < nk; ++k) {
            knobs[(size_t) k]->setVisible (true);
            knobs[(size_t) k]->s.setColour (juce::Slider::rotarySliderFillColourId, c);
            knobs[(size_t) k]->lab.setText (FxProcessor::knobLabel (t, k), juce::dontSendNotification);
            knobs[(size_t) k]->val.setColour (juce::Label::textColourId, c.brighter (0.2f));
            int slot = selected;
            knobs[(size_t) k]->setManual (getParam (slot, k),
                [this, slot, k] (float nv) { setParam (slot, k, nv); },
                [t, k] (float nv) { return fxKnobText (t, k, nv); });
        }
        for (int k = nk; k < (int) knobs.size(); ++k) knobs[(size_t) k]->setVisible (false);
    }
    resized();
    repaint();
}

void DrumEditor::FxChainView::resized() {
    auto r = getLocalBounds();
    if (r.getWidth() < 60 || r.getHeight() < 60) return;
    int perRow = 6;
    int rows = juce::jmax (1, ((int) chips.size() + perRow - 1) / perRow);
    auto strip = r.removeFromTop (30 * rows);
    int chipW = juce::jmax (40, juce::jmin (92, (strip.getWidth() - 190) / perRow - 4));
    for (size_t i = 0; i < chips.size(); ++i) {
        int rr = (int) i / perRow, cc = (int) i % perRow;
        chips[i]->setBounds (4 + cc * (chipW + 4), strip.getY() + 3 + rr * 30, chipW, 24);
    }
    auto topRow = juce::Rectangle<int> (strip.getX(), strip.getY(), strip.getWidth(), 30);
    dnB.setBounds (topRow.removeFromRight (40).reduced (2));
    upB.setBounds (topRow.removeFromRight (40).reduced (2));
    delB.setBounds (topRow.removeFromRight (56).reduced (2));
    addB.setBounds (topRow.removeFromRight (76).reduced (2));
    auto dt = r;
    auto typeRow = dt.removeFromTop (28);
    typeBox.setBounds (typeRow.removeFromLeft (170).reduced (2));
    bypassB.setBounds (typeRow.removeFromLeft (70).reduced (2));
    emptyLab.setBounds (dt);
    // 6 knobs per effect: 3 x 2 grid
    int vis = 0;
    for (auto& k : knobs) if (k->isVisible()) ++vis;
    int cols = vis > 6 ? 5 : 3, krows = 2;
    int kcw = dt.getWidth() / cols, kh = dt.getHeight() / krows;
    int vi = 0;
    for (int k = 0; k < (int) knobs.size(); ++k) {
        if (! knobs[(size_t) k]->isVisible()) continue;
        knobs[(size_t) k]->setBounds (kcw * (vi % cols), dt.getY() + kh * (vi / cols), kcw, kh);
        ++vi;
    }
}

ScopeSource DrumEditor::selectedScope() const {
    ScopeSource s;
    int p = sel();
    s.tap = &proc.parts[p].scope;
    s.colour = partColour (p);
    s.title = partName (p);
    s.sampleRate = proc.currentSampleRate();
    return s;
}

// ================= MacroPanel =================
MacroPanel::MacroPanel (DrumEditor& e) : ed (e) {
    for (int k = 0; k < DrumProcessor::NumMacros; ++k) {
        knobs[k] = std::make_unique<Knob> ("", juce::Colour (0xfff49ac1));
        knobs[k]->val.setVisible (false); // value is painted next to the name instead
        knobs[k]->lab.setVisible (false);
        knobs[k]->s.setTooltip ("Macro: centre = no change. Click the target strip below to choose what it moves.");
        addAndMakeVisible (*knobs[k]);
        auto& nl = names[k];
        nl.setEditable (false, true, false);
        nl.setFont (juce::Font (11.f, juce::Font::bold));
        nl.setColour (juce::Label::textColourId, juce::Colour (0xffe8e8ee));
        nl.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1c1c22));
        nl.setColour (juce::Label::outlineColourId, juce::Colour (0xff34343f));
        nl.setTooltip ("Double-click to rename");
        nl.onTextChange = [this, k] {
            auto t = names[k].getText().trim().toUpperCase().substring (0, 14);
            ed.proc.parts[ed.sel()].macros[k].name = t.isEmpty() ? "MACRO " + juce::String (k + 1) : t;
            names[k].setText (ed.proc.parts[ed.sel()].macros[k].name, juce::dontSendNotification);
        };
        addAndMakeVisible (nl);
    }
}

void MacroPanel::refresh() {
    int p = ed.sel();
    auto col = partColour (p);
    for (int k = 0; k < DrumProcessor::NumMacros; ++k) {
        auto& kb = *knobs[k];
        kb.att.reset();
        kb.accent = col;
        kb.s.setColour (juce::Slider::rotarySliderFillColourId, col);
        kb.attach (ed.proc.apvts, DrumProcessor::macroParamID (p, k));
        kb.s.setDoubleClickReturnValue (true, 0.5);
        kb.s.onValueChange = [this] { repaint(); };
        names[k].setText (ed.proc.parts[p].macros[k].name, juce::dontSendNotification);
    }
    repaint();
}

void MacroPanel::paint (juce::Graphics& g) {
    int p = ed.sel();
    auto col = partColour (p);
    g.setColour (juce::Colour (0xff6a6a78));
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("CLICK TARGETS TO EDIT", 0, 0, getWidth(), 14, juce::Justification::centredLeft);
    float uw = (float) getWidth() / DrumProcessor::NumMacros;
    for (int k = 0; k < DrumProcessor::NumMacros; ++k) {
        float ux = uw * k;
        if (k > 0) { g.setColour (juce::Colour (0xff26262f)); g.drawVerticalLine ((int) ux, 20.f, (float) getHeight() - 4.f); }
        float v = (float) knobs[k]->s.getValue();
        int pc = juce::roundToInt ((v - 0.5f) * 200.f);
        auto nb = names[k].getBounds();
        g.setColour (col);
        g.setFont (juce::Font (11.f));
        g.drawText ((pc > 0 ? "+" : "") + juce::String (pc) + "%", nb.getX(), nb.getBottom() + 4, nb.getWidth(), 14, juce::Justification::centredLeft);
        juce::StringArray labels;
        for (auto& t : ed.proc.parts[p].macros[k].t)
            if (t.param.load() >= 0 && ed.proc.macroTargetValid (p, t.param.load())) labels.add (ed.proc.macroTargetLabel (p, t.param.load()));
        auto tr = targetRects[k];
        g.setColour (juce::Colour (0xff1c1c22));
        g.fillRoundedRectangle (tr.toFloat(), 3.f);
        g.setColour (labels.isEmpty() ? juce::Colour (0xff6a6a78) : juce::Colour (0xffb8b8c4));
        g.setFont (juce::Font (9.f));
        g.drawFittedText (labels.isEmpty() ? juce::String ("+ add target") : labels.joinIntoString (juce::String::fromUTF8 (" \xc2\xb7 ")),
                          tr.reduced (6, 2), juce::Justification::centred, 3);
    }
}

void MacroPanel::resized() {
    float uw = (float) getWidth() / DrumProcessor::NumMacros;
    for (int k = 0; k < DrumProcessor::NumMacros; ++k) {
        int ux = (int) (uw * k);
        int kw = juce::jmin (96, (int) uw / 2);
        knobs[k]->setBounds (ux + 4, 22, kw, kw + 28); // + hidden label rows
        names[k].setBounds (ux + kw + 8, 34, (int) uw - kw - 16, 20);
        targetRects[k] = { ux + 6, 22 + kw + 34, (int) uw - 14, juce::jmin (64, getHeight() - (22 + kw + 38)) };
    }
}

void MacroPanel::mouseDown (const juce::MouseEvent& e) {
    for (int k = 0; k < DrumProcessor::NumMacros; ++k)
        if (targetRects[k].contains (e.getPosition())) { showTargetMenu (k); return; }
}

void MacroPanel::showTargetMenu (int k) {
    int p = ed.sel();
    auto& pr = ed.proc;
    auto& m = pr.parts[p].macros[k];
    static const float depths[] = { 1.f, 0.5f, 0.25f, 0.1f, -0.1f, -0.25f, -0.5f, -1.f };
    auto pct = [] (float d) { int v = juce::roundToInt (d * 100.f); return (v > 0 ? "+" : "") + juce::String (v) + "%"; };
    juce::PopupMenu menu;
    menu.addSectionHeader (juce::String (k + 1) + "  " + m.name);
    int n = 0;
    for (auto& t : m.t) {
        int prm = t.param.load();
        if (prm < 0) continue;
        juce::PopupMenu sub;
        for (int di = 0; di < (int) std::size (depths); ++di)
            sub.addItem (1000 + prm * 20 + di, pct (depths[di]), true, std::abs (t.depth.load() - depths[di]) < 1.0e-3f);
        sub.addSeparator();
        sub.addItem (1000 + prm * 20 + 19, "Remove");
        auto lab = pr.macroTargetLabel (p, prm) + (pr.macroTargetValid (p, prm) ? "" : "  (inactive)");
        menu.addSubMenu (lab + "   " + pct (t.depth.load()), sub);
        ++n;
    }
    if (n == 0) menu.addItem (-1, "(no targets yet)", false);
    menu.addSeparator();
    juce::PopupMenu add;
    for (int prm = 0; prm < DrumProcessor::NumMacroTargets; ++prm)
        if (pr.macroTargetValid (p, prm) && pr.macroDepth (p, k, prm) == 0.f) add.addItem (5000 + prm, pr.macroTargetLabel (p, prm));
    menu.addSubMenu ("Add target", add, n < DrumProcessor::MaxMacroTargets);
    menu.addItem (2, "Rename...");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (targetRects[k])),
        [sp = juce::Component::SafePointer<MacroPanel> (this), p, k] (int id) {
            if (sp == nullptr || id <= 0) return;
            auto& prc = sp->ed.proc;
            if (id == 2) { sp->names[k].showEditor(); return; }
            prc.pushUndo();
            if (id >= 5000) prc.setMacroTarget (p, k, id - 5000, 0.5f);
            else if (id >= 1000) {
                int prm = (id - 1000) / 20, di = (id - 1000) % 20;
                prc.setMacroTarget (p, k, prm, di == 19 ? 0.f : depths[di]);
            }
            sp->repaint();
        });
}

// ================= Editor =================
DrumEditor::DrumEditor (DrumProcessor& p) : juce::AudioProcessorEditor (p), proc (p) {
    setLookAndFeel (&lnf);

    // header
    presetLab.setText ("", juce::dontSendNotification);
    presetBtn.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff49ac1));
    presetBtn.onClick = [this] {
        rescanFactoryPresets();
        browser->setVisible (true);
    };
    auto stepProgram = [this] (int dir) {
        auto& all = getFactoryPresets();
        if (all.empty()) return;
        int pos = 0;
        for (size_t i = 0; i < all.size(); ++i)
            if (all[i].index == proc.currentKit) { pos = (int) i; break; }
        int npos = (pos + dir + (int) all.size()) % (int) all.size();
        proc.setCurrentProgram (all[(size_t) npos].index);
        rebuildSynthPanel(); refreshFxRows();
    };
    prevB.onClick = [this, stepProgram] { stepProgram (-1); };
    nextB.onClick = [this, stepProgram] { stepProgram (+1); };
    scopeBox.addItem ("PART", 1); scopeBox.addItem ("KIT", 2);
    scopeBox.setSelectedId (1, juce::dontSendNotification);
    randPartB.onClick = [this] {
        if (scopeBox.getSelectedId() == 1) proc.randomizePart (sel(), 1.f);
        else proc.randomizeKit (1.f);
        rebuildSynthPanel(); refreshFxRows();
    };
    mutPartB.onClick = [this] {
        if (scopeBox.getSelectedId() == 1) proc.mutatePart (sel(), 0.3f);
        else proc.mutateKit (0.2f);
        rebuildSynthPanel(); refreshFxRows();
    };
    randKitB.onClick = [this] { proc.randomizeKit (0.7f); rebuildSynthPanel(); refreshFxRows(); };
    undoB.onClick = [this] { proc.undo(); rebuildSynthPanel(); refreshFxRows(); };
    redoB.onClick = [this] { proc.redo(); rebuildSynthPanel(); refreshFxRows(); };
    undoB.setTooltip ("Undo the last change: knob turns, kit edits, randomize"); redoB.setTooltip ("Redo what UNDO took back");
    initB.onClick = [this] { proc.initKit(); rebuildSynthPanel(); refreshFxRows(); };
    randPartB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff49ac1));
    mutPartB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa78bfa));
    masterDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterDial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterDial.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfffbbf24));
    // MASTER dial + OUTPUT fader both drive the "master" parameter (the
    // processor re-reads it every block, so direct writes were overwritten)
    masterDialAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterDial);
    masterLab.setText ("MASTER", juce::dontSendNotification);
    masterLab.setFont (juce::Font (9.f, juce::Font::bold));
    limitB.setClickingTogglesState (true);
    limitB.setToggleState (proc.limiterOn.load(), juce::dontSendNotification);
    limitB.onClick = [this] { proc.limiterOn = limitB.getToggleState(); };

    addAndMakeVisible (presetBtn); addAndMakeVisible (prevB); addAndMakeVisible (nextB);
    addAndMakeVisible (scopeBox); addAndMakeVisible (randPartB); addAndMakeVisible (mutPartB);
    addAndMakeVisible (randKitB); addAndMakeVisible (undoB); addAndMakeVisible (redoB); addAndMakeVisible (initB);
    addAndMakeVisible (masterDial); addAndMakeVisible (masterLab);
    kitLab.setFont (juce::Font (12.f, juce::Font::bold));
    kitLab.setColour (juce::Label::textColourId, juce::Colour (0xff8a8a98));
    kitLab.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (kitLab);

    // cards
    for (int i = 0; i < 12; ++i) { cards.push_back (std::make_unique<PartCard> (i, proc)); cardRow.addAndMakeVisible (*cards.back()); }
    addAndMakeVisible (cardRow);

    // synth tabs
    tabSynth.onClick = [this] { tab = 0; rebuildSynthPanel(); };
    tabAmp.onClick = [this] { tab = 1; rebuildSynthPanel(); };
    tabPitch.onClick = [this] { tab = 2; rebuildSynthPanel(); };
    tabSend.onClick = [this] { tab = 3; rebuildSynthPanel(); };
    tabMacro.onClick = [this] { tab = 4; rebuildSynthPanel(); };
    tabSynth.setClickingTogglesState (true); tabAmp.setClickingTogglesState (true);
    tabPitch.setClickingTogglesState (true); tabSend.setClickingTogglesState (true);
    tabMacro.setClickingTogglesState (true);
    macroPanel = std::make_unique<MacroPanel> (*this);
    synthPanel.addChildComponent (*macroPanel);
    tabSynth.setToggleState (true, juce::dontSendNotification);
    trigB.onClick = [this] { proc.auditionReq = sel(); };   // audio thread plays it like a MIDI hit (models, REVERSE)
    // MODEL: picking one loads its default knobs (undoable); right-click locks it from RANDOMIZE
    modelBox.onChange = [this] {
        int m = modelBox.getSelectedId() - 1;
        if (m >= 0 && m != shownModel()) { proc.setPartModel (sel(), m); rebuildSynthPanel(); }
    };
    modelBox.setTooltip ("Synthesis engine for this part. CLASSIC = the original voice");
    modelLab.onRightClick = [this] {   // locks the engine choice only; knobs lock one by one
        proc.toggleLock (sel(), DrumProcessor::LockModel);
        modelLab.locked = proc.isLocked (sel(), DrumProcessor::LockModel); modelLab.repaint();
    };
    revB.setClickingTogglesState (true);
    revB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa78bfa));
    revB.setTooltip ("Play this part's sound backwards");
    revB.onClick = [this] {
        if (auto* q = proc.apvts.getParameter ("p" + juce::String (sel()) + "_rev"))
            q->setValueNotifyingHost (revB.getToggleState() ? 1.f : 0.f);
        for (auto& c : cards) c->repaint();
    };
    outBox.addItem ("MAIN", 1);
    for (int o = 1; o <= DrumProcessor::NumAuxOuts; ++o) outBox.addItem ("OUT " + juce::String (o + 1), o + 1);
    outBox.setTooltip ("Output bus for this part (enable the extra outputs in your host; disabled buses fall back to MAIN)");
    outBox.onChange = [this] { proc.parts[sel()].output = outBox.getSelectedId() - 1; };
    chokeBox.addItem ("NONE", 1);
    for (int i = 1; i <= DrumProcessor::NumChokeGroups; ++i) chokeBox.addItem ("GROUP " + juce::String (i), i + 1);
    chokeBox.setTooltip ("Parts in the same choke group cut each other off (the hats share group 1)");
    chokeBox.onChange = [this] { proc.parts[sel()].choke = chokeBox.getSelectedId() - 1; };
    synthPanel.addAndMakeVisible (chokeBox); synthPanel.addAndMakeVisible (chokeLab);
    for (auto* l : { (juce::Label*) &modelLab, &outLab, &chokeLab }) {
        l->setFont (juce::Font (10.f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, juce::Colour (0xff9a9aa8));
        l->setJustificationType (juce::Justification::centredRight);
    }
    synthPanel.addAndMakeVisible (modelBox); synthPanel.addAndMakeVisible (modelLab);
    synthPanel.addAndMakeVisible (revB); synthPanel.addAndMakeVisible (outBox); synthPanel.addAndMakeVisible (outLab);
    waveBox.addItemList ({ "Sine","Tri","Saw","Square","FM" }, 1);
    noiseBox.addItemList ({ "White","Pink","Violet","Metallic","Digital","Crackle","Tape","Resonant" }, 1);
    waveBox.onChange = [this] {
        if (auto* q = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("p" + juce::String (sel()) + "_wave")))
            q->setValueNotifyingHost (q->convertTo0to1 (waveBox.getSelectedId() - 1));
    };
    noiseBox.onChange = [this] {
        if (auto* q = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("p" + juce::String (sel()) + "_noise")))
            q->setValueNotifyingHost (q->convertTo0to1 (noiseBox.getSelectedId() - 1));
    };
    // SAT selector removed: voice saturation is fixed Soft, DRIVE knob + DISTORTION FX carry the grit.
    synthPanel.addAndMakeVisible (tabSynth); synthPanel.addAndMakeVisible (tabAmp);
    synthPanel.addAndMakeVisible (tabPitch); synthPanel.addAndMakeVisible (tabSend); synthPanel.addAndMakeVisible (tabMacro);
    synthPanel.addAndMakeVisible (partTitle); synthPanel.addAndMakeVisible (trigB);
    synthPanel.addAndMakeVisible (waveBox); synthPanel.addAndMakeVisible (noiseBox);
    synthPanel.addAndMakeVisible (waveLab); synthPanel.addAndMakeVisible (noiseLab);
    waveLab.onRightClick = [this] { proc.toggleLock (sel(), DrumProcessor::LockWave); waveLab.locked = proc.isLocked (sel(), DrumProcessor::LockWave); waveLab.repaint(); };
    noiseLab.onRightClick = [this] { proc.toggleLock (sel(), DrumProcessor::LockNoise); noiseLab.locked = proc.isLocked (sel(), DrumProcessor::LockNoise); noiseLab.repaint(); };
    lockHint.setFont (juce::Font (10.f, juce::Font::bold));
    lockHint.setColour (juce::Label::textColourId, juce::Colour (0xff6a6a78));
    synthPanel.addAndMakeVisible (lockHint);
    synthPanel.addAndMakeVisible (scopeView); synthPanel.addAndMakeVisible (analyzerView);
    synthPanel.addAndMakeVisible (meterComp);
    addAndMakeVisible (synthPanel);

    // fx chain: tabbed detail view (empty by default, + ADD to insert)
    fxPanel.addAndMakeVisible (fxTitle); fxPanel.addAndMakeVisible (fxHint);
    fxPanel.addAndMakeVisible (partFxPower);
    fxPanel.addAndMakeVisible (partFxLock);
    partFxLock.setClickingTogglesState (true);
    partFxLock.setTooltip ("Lock this part's effects chain from RANDOMIZE / MUTATE / KIT RND");
    partFxLock.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfffbbf24)); // amber like the padlocks
    partFxLock.onClick = [this] { proc.setLocked (sel(), DrumProcessor::LockFx, partFxLock.getToggleState()); };
    partFxPower.setClickingTogglesState (true);
    partFxPower.setToggleState (true, juce::dontSendNotification);
    partFxPower.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa3e635));
    partFxPower.onClick = [this] {
        proc.parts[sel()].fx.enabled.store (partFxPower.getToggleState());
        refreshFxRows();
    };
    partFxView = std::make_unique<FxChainView> (*this, false);
    fxPanel.addAndMakeVisible (*partFxView);
    addAndMakeVisible (fxPanel);

    // global
    globalPanel.addAndMakeVisible (gloTitle);
    globalPanel.addAndMakeVisible (globPower);
    globPower.setClickingTogglesState (true);
    globPower.setToggleState (true, juce::dontSendNotification);
    globPower.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa3e635));
    globPower.onClick = [this] {
        proc.globalFx.enabled.store (globPower.getToggleState());
        refreshFxRows();
    };
    globFxView = std::make_unique<FxChainView> (*this, true);
    globalPanel.addAndMakeVisible (*globFxView);
    addAndMakeVisible (globalPanel);

    outPanel.addAndMakeVisible (outTitle);
    outPanel.addAndMakeVisible (masterFader);
    outPanel.addAndMakeVisible (limitB);
    masterFader.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff49ac1));
    masterFader.setColour (juce::Slider::trackColourId, juce::Colour (0xfff49ac1).withAlpha (0.5f));
    masterFader.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2a2a34));
    masterFaderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterFader);
    addAndMakeVisible (outPanel);

    viewSynth.setClickingTogglesState (true); viewPartFx.setClickingTogglesState (true); viewGlobFx.setClickingTogglesState (true);
    viewSynth.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff49ac1));
    viewPartFx.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff49ac1));
    viewGlobFx.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xfff49ac1));
    viewSynth.onClick = [this] { mainView = 0; layoutMainView(); };
    viewPartFx.onClick = [this] { mainView = 1; layoutMainView(); };
    viewGlobFx.onClick = [this] { mainView = 2; layoutMainView(); };
    addAndMakeVisible (viewSynth); addAndMakeVisible (viewPartFx); addAndMakeVisible (viewGlobFx);

    rebuildSynthPanel();
    refreshFxRows();
    layoutMainView();
    browser = std::make_unique<PresetBrowser> (proc, lnf);
    browser->onClose = [this] { browser->setVisible (false); };
    browser->onPresetChanged = [this] { rebuildSynthPanel(); refreshFxRows(); };
    addAndMakeVisible (*browser); // last: on top
    browser->setVisible (false);
    setSize (1280, 800);
    setResizeLimits (1140, 680, 2560, 1600);
    setResizable (true, true);
    startTimerHz (30);
}
DrumEditor::~DrumEditor() { setLookAndFeel (nullptr); }

static juce::String fmtVal (const ParamMeta& m, float v) {
    juce::String s = m.fmt;
    if (s == "st") return juce::String (v, 1) + " st";
    if (s == "ms") return v < 20 ? juce::String (v, 1) + " ms" : juce::String ((int) v) + " ms";
    if (s == "s") return juce::String (v / 1000.f, 2) + " s";
    if (s == "%") return juce::String ((int) v) + "%";
    if (s == "dB") return v <= -59 ? "-inf" : juce::String (v, 1) + " dB";
    if (s == "x") return juce::String (v, 0) + " Oct";
    if (s == "Hz") return v < 10.f ? juce::String (v, 1) + "Hz" : juce::String ((int) v) + "Hz";
    if (s == "ratio") return juce::String (v, 2) + "x";
    if (s == "q") return juce::String (v, 1);
    return juce::String (v, 1);
}

int DrumEditor::shownModel() const {
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("p" + juce::String (sel()) + "_model")))
        return c->getIndex();
    return 0;
}

// the selected part's colour drives the shared accent: page / sub tabs, toggles,
// output fader and limiter all follow the section being edited
void DrumEditor::applyPartAccent (int p) {
    const auto col = partColour (p);
    lnf.accent = col;
    lnf.setColour (juce::Slider::rotarySliderFillColourId, col);
    for (auto* b : { &viewSynth, &viewPartFx, &viewGlobFx })
        b->setColour (juce::Slider::rotarySliderFillColourId, col);
    masterFader.setColour (juce::Slider::thumbColourId, col);
    masterFader.setColour (juce::Slider::trackColourId, col.withAlpha (0.5f));
    repaint();
}

void DrumEditor::rebuildSynthPanel() {
    for (auto& k : synthKnobs) synthPanel.removeChildComponent (k.get());
    synthKnobs.clear();
    int p = sel();
    applyPartAccent (p);
    partTitle.setText (juce::String (partName (p)), juce::dontSendNotification);
    partTitle.setFont (juce::Font (15.f, juce::Font::bold));
    partTitle.setColour (juce::Label::textColourId, partColour (p));
    const int model = shownModel();
    const ModelDef* md = modelDef (p, model);
    if (md && tab == 2) tab = 0;   // models have no PITCH page (their pitch knobs live on SYNTH)
    tabPitch.setVisible (md == nullptr);
    waveBox.setVisible (md == nullptr); waveLab.setVisible (md == nullptr);
    noiseBox.setVisible (md == nullptr); noiseLab.setVisible (md == nullptr);
    {
        auto names = modelNames (p);
        bool same = modelBox.getNumItems() == names.size();
        for (int i = 0; same && i < names.size(); ++i) same = modelBox.getItemText (i) == names[i];
        if (! same) { modelBox.clear (juce::dontSendNotification); modelBox.addItemList (names, 1); }
        modelBox.setSelectedId (model + 1, juce::dontSendNotification);
        modelLab.locked = proc.isLocked (p, DrumProcessor::LockModel); modelLab.repaint();
        revB.setToggleState (proc.apvts.getRawParameterValue ("p" + juce::String (p) + "_rev")->load() > 0.5f, juce::dontSendNotification);
        outBox.setSelectedId (proc.parts[p].output.load() + 1, juce::dontSendNotification);
        chokeBox.setSelectedId (proc.parts[p].choke.load() + 1, juce::dontSendNotification);
    }
    tabSynth.setToggleState (tab == 0, juce::dontSendNotification);
    tabAmp.setToggleState (tab == 1, juce::dontSendNotification);
    tabPitch.setToggleState (tab == 2, juce::dontSendNotification);
    tabSend.setToggleState (tab == 3, juce::dontSendNotification);
    tabMacro.setToggleState (tab == 4, juce::dontSendNotification);
    macroPanel->setVisible (tab == 4);
    if (tab == 4) macroPanel->refresh();

    std::vector<int> slots;
    if (md != nullptr) {
        // engine model: its own knobs on SYNTH, only PAN on AMP (the model shapes its own envelope)
        if (tab == 0)
            for (int k = 0; k < md->nparams; ++k) {
                const ModelParam mp = md->p[k];
                auto kb = std::make_unique<Knob> (mp.label, partColour (p));
                kb->s.setColour (juce::Slider::rotarySliderFillColourId, partColour (p));
                kb->val.setColour (juce::Label::textColourId, partColour (p).brighter (0.2f));
                kb->attach (proc.apvts, DrumProcessor::modelParamID (p, k));
                kb->s.onValueChange = [kbp = kb.get(), mp] {
                    kbp->val.setText (modelFormat (mp, modelCook (mp, (float) kbp->s.getValue())), juce::dontSendNotification);
                };
                const int key = DrumProcessor::modelKnobLock (k);
                kb->locked = proc.isLocked (p, key);
                kb->s.onRightClick = [this, p, key, kbp = kb.get()] {
                    proc.toggleLock (p, key);
                    kbp->locked = proc.isLocked (p, key);
                    kbp->repaint();
                    if (kbp->locked) {   // a knob lock holds the model too (its knob means something else in another model)
                        proc.setLocked (p, DrumProcessor::LockModel, true);
                        modelLab.locked = true; modelLab.repaint();
                    }
                };
                kb->s.onValueChange();
                synthPanel.addAndMakeVisible (*kb);
                synthKnobs.push_back (std::move (kb));
            }
    }
    else if (tab == 0) {
        // snare: BODY/NOISE/N DECAY/TONE/DRIVE/COMP/SUB/VEL all live here
        for (int s : { 0, 8, 9, 10, 11, 12, 13, 14, 15 }) slots.push_back (s);
        for (int s = 16; s < 20; ++s) if (advMetaForPart (p, s)) slots.push_back (s); // PUNCH/BOOST/NZ>FX, SFX LFO...
    }
    else if (tab == 1) { for (int s : { 4, 5, 6, 7 }) slots.push_back (s); }
    else if (tab == 2) {
        // cymbals have no pitch sweep: TUNE + SPREAD only (no dead P DECAY/CURVE)
        if (p == ClosedHat || p == OpenHat || p == Ride || p == Crash) { slots.push_back (0); slots.push_back (1); }
        else for (int s : { 0, 1, 2, 3 }) slots.push_back (s);
    }
    else { /* FX SEND: send levels + shared returns, built below */ }

    if (tab == 4) { /* MACROS: the macro panel takes the knob area */ }
    else if (tab != 3) {
        for (int s : slots) {
            const ParamMeta* sm = slotMeta (p, s);
            ParamMeta mm = sm ? *sm : ParamMeta { "?", 0.f, 1.f, 0.f, false, "" };
            auto kb = std::make_unique<Knob> (mm.label, partColour (p));
            kb->s.setColour (juce::Slider::rotarySliderFillColourId, partColour (p));
            kb->val.setColour (juce::Label::textColourId, partColour (p).brighter (0.2f));
            kb->attach (proc.apvts, synthParamID (p, s));
            kb->s.onValueChange = [kbp = kb.get(), mm] {
                float v = (float) kbp->s.getValue();
                kbp->val.setText (fmtVal (mm, v), juce::dontSendNotification);
            };
            kb->locked = proc.isLocked (p, s);
            kb->s.onRightClick = [this, p, s, kbp = kb.get()] {
                proc.toggleLock (p, s);
                kbp->locked = proc.isLocked (p, s);
                kbp->repaint();
            };
            kb->s.onValueChange();
            synthPanel.addAndMakeVisible (*kb);
            synthKnobs.push_back (std::move (kb));
        }
        if (tab == 1) {
            // PAN lives here now that the mixer is gone
            auto pankb = std::make_unique<Knob> ("PAN", partColour (p));
            pankb->s.setColour (juce::Slider::rotarySliderFillColourId, partColour (p));
            pankb->val.setColour (juce::Label::textColourId, partColour (p).brighter (0.2f));
            pankb->attach (proc.apvts, "mix" + juce::String (p) + "_pan");
            pankb->s.onValueChange = [kbp = pankb.get()] {
                float v = (float) kbp->s.getValue();
                juce::String t = std::abs (v - 0.5f) < 0.02f ? "C" : (v < 0.5f ? "L" + juce::String ((int) ((0.5f - v) * 200)) : "R" + juce::String ((int) ((v - 0.5f) * 200)));
                kbp->val.setText (t, juce::dontSendNotification);
            };
            pankb->s.onValueChange();
            synthPanel.addAndMakeVisible (*pankb);
            synthKnobs.push_back (std::move (pankb));
        }
    } else {
        // per-part sends (automatable parameters) + the shared return settings (affect every part)
        auto pct = [] (float v) { return juce::String ((int) std::round (v * 100)) + "%"; };
        for (int d = 0; d < 2; ++d) {
            auto kb = std::make_unique<Knob> (d ? "SEND DLY" : "SEND REV", partColour (p));
            kb->s.setColour (juce::Slider::rotarySliderFillColourId, partColour (p));
            kb->val.setColour (juce::Label::textColourId, partColour (p).brighter (0.2f));
            kb->attach (proc.apvts, DrumProcessor::sendParamID (p, d == 1));
            kb->s.onValueChange = [kbp = kb.get(), pct] { kbp->val.setText (pct ((float) kbp->s.getValue()), juce::dontSendNotification); };
            kb->s.onValueChange();
            const int key = d ? DrumProcessor::LockSendDly : DrumProcessor::LockSendRev;
            kb->locked = proc.isLocked (p, key);
            kb->s.onRightClick = [this, p, key, kbp = kb.get()] {
                proc.toggleLock (p, key);
                kbp->locked = proc.isLocked (p, key);
                kbp->repaint();
            };
            synthPanel.addAndMakeVisible (*kb);
            synthKnobs.push_back (std::move (kb));
        }
        auto* pr = &proc;
        auto dlyFmt = [pr] (float v) {
            int d = pr->dlySync.load();
            float sec = d >= 0 ? (float) TempoSync::divisionSeconds (d, pr->hostBpm.load()) : FxProcessor::sendDelaySeconds (v);
            return (d >= 0 ? juce::String (TempoSync::divisionName (d)) + "  " : juce::String()) + juce::String (juce::roundToInt (sec * 1000.f)) + "ms";
        };
        struct ReturnKnob { const char* label; std::atomic<float>* tgt; std::function<juce::String (float)> fmt; };
        ReturnKnob defs[4] = {   // the shared return knobs are never randomized: nothing to lock
            { "REV DECAY", &proc.revDecay, [] (float v) { return juce::String (FxProcessor::rt60FromKnob (v), 2) + "s"; } },
            { "REV DAMP",  &proc.revDamp, pct },
            { "DLY TIME",  &proc.dlyTime, dlyFmt },
            { "DLY FDBK",  &proc.dlyFb, pct },
        };
        Knob* timeK = nullptr;
        for (auto& d : defs) {
            auto kb = std::make_unique<Knob> (d.label, partColour (p));
            kb->s.setColour (juce::Slider::rotarySliderFillColourId, partColour (p));
            auto* tgt = d.tgt;
            kb->setManual (tgt->load(), [tgt] (float v) { *tgt = v; }, d.fmt);
            if (tgt == &proc.dlyTime) timeK = kb.get();
            synthPanel.addAndMakeVisible (*kb);
            synthKnobs.push_back (std::move (kb));
        }
        // DLY SYNC: FREE, then the shared tempo divisions (host tempo, 120 BPM without one)
        static constexpr int steps = TempoSync::NumDivisions;   // static: usable in the lambdas below without a capture (MSVC)
        auto sk = std::make_unique<Knob> ("DLY SYNC", partColour (p));
        sk->s.setColour (juce::Slider::rotarySliderFillColourId, partColour (p));
        sk->setManual ((float) (proc.dlySync.load() + 1) / steps, [pr, timeK] (float v) {
            pr->dlySync = juce::roundToInt (v * steps) - 1;
            timeK->s.setAlpha (pr->dlySync.load() >= 0 ? 0.45f : 1.f);
            timeK->val.setText (timeK->fmtr ((float) timeK->s.getValue()), juce::dontSendNotification);
        }, [] (float v) { int d = juce::roundToInt (v * steps) - 1; return d < 0 ? juce::String ("FREE") : juce::String (TempoSync::divisionName (d)); });
        sk->s.setRange (0.0, 1.0, 1.0 / steps);
        sk->s.setTooltip ("FREE: DLY TIME sets the delay. Otherwise the delay follows the host tempo.");
        timeK->s.setAlpha (proc.dlySync.load() >= 0 ? 0.45f : 1.f);
        synthPanel.addAndMakeVisible (*sk);
        synthKnobs.push_back (std::move (sk));
    }
    waveLab.locked = proc.isLocked (p, DrumProcessor::LockWave);
    noiseLab.locked = proc.isLocked (p, DrumProcessor::LockNoise);
    waveLab.repaint(); noiseLab.repaint();
    if (auto* w = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("p" + juce::String (p) + "_wave")))
        waveBox.setSelectedId (w->getIndex() + 1, juce::dontSendNotification);
    if (auto* n = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter ("p" + juce::String (p) + "_noise")))
        noiseBox.setSelectedId (n->getIndex() + 1, juce::dontSendNotification);
    resized();
}

void DrumEditor::refreshFxRows() {
    if (partFxView) partFxView->refresh();
    if (globFxView) globFxView->refresh();
    bool pe = proc.parts[sel()].fx.enabled.load();
    partFxPower.setToggleState (pe, juce::dontSendNotification);
    partFxPower.setButtonText (pe ? "ON" : "OFF");
    partFxLock.setToggleState (proc.isLocked (sel(), DrumProcessor::LockFx), juce::dontSendNotification);
    bool ge = proc.globalFx.enabled.load();
    globPower.setToggleState (ge, juce::dontSendNotification);
    globPower.setButtonText (ge ? "ON" : "OFF");
    // resync buttons that engine state can change behind the UI's back
    // (kit loads, undo, init): stuck invisible solo/mute looks exactly like "muted"
    for (auto& c : cards) {
        c->solo.setToggleState (proc.parts[c->idx].solo, juce::dontSendNotification);
        c->mute.setToggleState (proc.parts[c->idx].mute, juce::dontSendNotification);
    }
    limitB.setToggleState (proc.limiterOn.load(), juce::dontSendNotification);
    repaint();
}
void DrumEditor::selectPart (int p) { proc.selectedPart = p; rebuildSynthPanel(); refreshFxRows(); }

void DrumEditor::paint (juce::Graphics& g) {
    g.fillAll (juce::Colour (0xff101014));
    // footer background separation
    if (footerRect.getWidth() > 0) {
        g.setColour (juce::Colour (0xff141419));
        g.fillRect (footerRect);
    }
}

void DrumEditor::resized() {
    if (getWidth() < 50 || getHeight() < 50) return;
    if (cards.size() < 12 || ! partFxView || ! globFxView) return;
    auto r = getLocalBounds();
    auto header = r.removeFromTop (52);
    prevB.setBounds (header.removeFromLeft (30).reduced (2));
    presetBtn.setBounds (header.removeFromLeft (230).reduced (2));
    nextB.setBounds (header.removeFromLeft (30).reduced (2));
    header.removeFromLeft (8);
    undoB.setBounds (header.removeFromLeft (62).reduced (2));
    redoB.setBounds (header.removeFromLeft (62).reduced (2));
    initB.setBounds (header.removeFromLeft (60).reduced (2));
    scopeBox.setBounds (header.removeFromLeft (80).reduced (2));
    randPartB.setBounds (header.removeFromLeft (110).reduced (2));
    mutPartB.setBounds (header.removeFromLeft (100).reduced (2));
    randKitB.setBounds (header.removeFromLeft (70).reduced (2));
    auto mhead = header.removeFromRight (120);
    masterLab.setBounds (mhead.removeFromLeft (60).reduced (2));
    masterDial.setBounds (mhead.removeFromLeft (46).reduced (2));
    kitLab.setBounds (header.reduced (4)); // middle free space: kit name + genre
    kitLab.setVisible (getWidth() >= 1100); // never collide with master block when narrow

    auto cardRowRect = r.removeFromTop (140);
    cardRow.setBounds (cardRowRect);
    int cw = cardRowRect.getWidth() / 12;
    for (int i = 0; i < 12; ++i) {
        int x = i * cw;
        int w = (i == 11) ? (cardRowRect.getWidth() - x) : cw;
        this->cards[(size_t) i]->setBounds (x, 0, w, cardRowRect.getHeight());
    }

    // main view tabs: synth engine | per-part FX | global FX (one visible)
    auto tabRowMain = r.removeFromTop (32);
    viewSynth.setBounds (tabRowMain.removeFromLeft (130).reduced (2));
    viewPartFx.setBounds (tabRowMain.removeFromLeft (130).reduced (2));
    viewGlobFx.setBounds (tabRowMain.removeFromLeft (130).reduced (2));

    footerRect = r.removeFromBottom (64);
    outPanel.setBounds (footerRect);
    auto content = r.reduced (4);
    synthPanel.setBounds (content);
    fxPanel.setBounds (content);
    globalPanel.setBounds (content);
    // outPanel IS the footer now (set above); mid/bot split is gone (tabbed views).

    // synth panel internals
    auto sr = synthPanel.getLocalBounds().reduced (10);
    {
        auto titleRow = sr.removeFromTop (24);
        outBox.setBounds (titleRow.removeFromRight (90).reduced (1));
        outLab.setBounds (titleRow.removeFromRight (36).reduced (1));
        titleRow.removeFromRight (8);
        chokeBox.setBounds (titleRow.removeFromRight (92).reduced (1));
        chokeLab.setBounds (titleRow.removeFromRight (50).reduced (1));
        titleRow.removeFromRight (8);
        revB.setBounds (titleRow.removeFromRight (78).reduced (1));
        titleRow.removeFromRight (8);
        modelBox.setBounds (titleRow.removeFromRight (150).reduced (1));
        modelLab.setBounds (titleRow.removeFromRight (62).reduced (1));
        partTitle.setBounds (titleRow);
        sr.removeFromTop (2);
    }
    auto tabRow = sr.removeFromTop (26);
    tabSynth.setBounds (tabRow.removeFromLeft (62).reduced (1));
    tabAmp.setBounds (tabRow.removeFromLeft (48).reduced (1));
    if (tabPitch.isVisible()) tabPitch.setBounds (tabRow.removeFromLeft (52).reduced (1));   // hidden for engine models
    tabSend.setBounds (tabRow.removeFromLeft (62).reduced (1));
    tabMacro.setBounds (tabRow.removeFromLeft (66).reduced (1));
    lockHint.setBounds (tabRow.removeFromLeft (120).reduced (6, 1));
    trigB.setBounds (tabRow.removeFromRight (72).reduced (1));
    noiseBox.setBounds (tabRow.removeFromRight (110).reduced (1));
    noiseLab.setBounds (tabRow.removeFromRight (58).reduced (1)); // room for the padlock
    waveBox.setBounds (tabRow.removeFromRight (100).reduced (1));
    waveLab.setBounds (tabRow.removeFromRight (54).reduced (1));
    int n = (int) synthKnobs.size();
    // knobs left (5 cols, compact), meter strip, scope + analyzer right
    auto knobArea = sr.removeFromLeft ((int) (sr.getWidth() * 0.56));
    auto visArea = sr;
    if (macroPanel) macroPanel->setBounds (knobArea.reduced (4));
    meterComp.setBounds (visArea.removeFromLeft (22).reduced (2)); // real child: no coordinate bugs
    scopeView.setBounds (visArea.removeFromTop ((int) (visArea.getHeight() * 0.42)).reduced (4));
    analyzerView.setBounds (visArea.reduced (4));
    if (n > 0) {
        int cols = n > 13 ? 6 : 5, rows = (n + cols - 1) / cols;
        int kcw = knobArea.getWidth() / cols, kh = juce::jmin (150, knobArea.getHeight() / juce::jmax (1, rows));   // few knobs: no giant dials
        for (int i = 0; i < n; ++i) {
            auto kb = juce::Rectangle<int> (knobArea.getX() + (i % cols) * kcw, knobArea.getY() + (i / cols) * kh, kcw, kh).reduced (4);
            synthKnobs[(size_t) i]->setBounds (kb);
        }
    }

    // fx chain view (chips + tabbed detail)
    auto fr = fxPanel.getLocalBounds().reduced (8);
    auto fxHead = fr.removeFromTop (18);
    partFxPower.setBounds (fxHead.removeFromRight (56).reduced (1));
    partFxLock.setBounds (fxHead.removeFromRight (56).reduced (1));
    fxHint.setBounds (fxHead.removeFromRight (140).reduced (1));
    fxTitle.setBounds (fxHead.reduced (1));
    if (partFxView) partFxView->setBounds (fr);

    // mixer strips (removed; level on pads, pan on AMP tab)

    // global chain view (chips + tabbed detail)
    auto gr = globalPanel.getLocalBounds().reduced (8);
    auto gHead = gr.removeFromTop (18);
    globPower.setBounds (gHead.removeFromRight (56).reduced (1));
    gloTitle.setBounds (gHead.reduced (1));
    if (globFxView) globFxView->setBounds (gr);
    // footer strip: title | master fader | limiter (meter moved next to scope)
    auto orr = outPanel.getLocalBounds().reduced (8);
    outTitle.setBounds (orr.removeFromLeft (80).reduced (2));
    limitB.setBounds (orr.removeFromRight (90).reduced (14, 8));
    masterFader.setBounds (orr.reduced (2));

    if (browser) browser->setBounds (getLocalBounds());
}

void DrumEditor::layoutMainView() {
    viewSynth.setToggleState (mainView == 0, juce::dontSendNotification);
    viewPartFx.setToggleState (mainView == 1, juce::dontSendNotification);
    viewGlobFx.setToggleState (mainView == 2, juce::dontSendNotification);
    synthPanel.setVisible (mainView == 0);
    fxPanel.setVisible (mainView == 1);
    globalPanel.setVisible (mainView == 2);
}

void DrumEditor::timerCallback() {
    // sync selected part UI if changed externally
    static int lastSel = -1, lastKit = -1, lastVer = -1;
    static juce::String lastKitName;
    int ver = proc.stateVersion.load();
    if (lastSel != sel() || lastKit != proc.currentKit || ver != lastVer) {
        lastSel = sel(); lastKit = proc.currentKit; lastVer = ver;
        rebuildSynthPanel(); refreshFxRows();
        for (auto& c : cards) c->repaint();
    }
    if (lastKitName != proc.kitName) {
        lastKitName = proc.kitName;
        auto style = presetGenreOf (proc.currentKit);
        kitLab.setText (style.isEmpty() ? proc.kitName : proc.kitName + "  |  " + style, juce::dontSendNotification);
        presetBtn.setButtonText (proc.kitName);
    }
    undoB.setEnabled (! proc.undoStack.empty());
    redoB.setEnabled (! proc.redoStack.empty());
    scopeView.repaint();
    analyzerView.repaint();
    meterComp.repaint();
    repaint();
}

} // namespace mds

juce::AudioProcessorEditor* mds::DrumProcessor::createEditor() { return new mds::DrumEditor (*this); }
