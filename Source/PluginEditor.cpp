#include "PluginEditor.h"
#include "PresetBrowser.h"
#include "DrumAssets.h"

namespace mds {

// ================= LNF =================
// Readable text for the views' readouts: "p{part}_{slot}" ids map back to the
// slot metadata (stateless, shared by every open editor through pastel::Theme).
static juce::String fmtVal (const ParamMeta& m, float v);
static juce::String drumFormat (const juce::String& id, float v) {
    if (id.startsWithChar ('p') && id.containsChar ('_')) {
        const int part = id.substring (1).getIntValue();
        const int slot = id.fromFirstOccurrenceOf ("_", false, false).getIntValue();
        if (auto* m = slotMeta (part, slot)) return fmtVal (*m, v);
    }
    return juce::String ((int) std::round (v * 100.f)) + "%";
}

PastelLNF::PastelLNF() {
    latoReg   = juce::Typeface::createSystemTypefaceFor (DrumAssets::LatoRegular_ttf, DrumAssets::LatoRegular_ttfSize);
    latoBold  = juce::Typeface::createSystemTypefaceFor (DrumAssets::LatoBold_ttf, DrumAssets::LatoBold_ttfSize);
    latoBlack = juce::Typeface::createSystemTypefaceFor (DrumAssets::LatoBlack_ttf, DrumAssets::LatoBlack_ttfSize);
    using namespace DrumColors;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::Label::textColourId, textDim);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::trackColourId, border);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, control);
    setColour (juce::PopupMenu::highlightedTextColourId, handle);
    setColour (juce::TextEditor::backgroundColourId, control);
    setColour (juce::TextEditor::outlineColourId, edge);
    setColour (juce::TooltipWindow::backgroundColourId, control);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, edge);
    // the shared v3 views
    pastel::Theme::accent = accent;  pastel::Theme::inner = inner;    pastel::Theme::grid = grid;
    pastel::Theme::edge = edge;      pastel::Theme::comboBg = control; pastel::Theme::text = text;
    pastel::Theme::textDim = textDim; pastel::Theme::faint = faint;   pastel::Theme::handle = handle;
    pastel::Theme::ink = ink;        pastel::Theme::regular = latoReg; pastel::Theme::bold = latoBold;
    pastel::Theme::format = drumFormat;
}
// every plain juce::Font in the editor resolves to the embedded Lato
juce::Typeface::Ptr PastelLNF::getTypefaceForFont (const juce::Font& f) {
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() && latoReg != nullptr)
        return f.isBold() ? latoBold : latoReg;
    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}
juce::Font PastelLNF::getTextButtonFont (juce::TextButton&, int h) { return juce::Font (juce::jmin (12.f, h * 0.5f), juce::Font::bold); }
juce::Font PastelLNF::getComboBoxFont (juce::ComboBox&) { return juce::Font (11.5f, juce::Font::bold); }
void PastelLNF::positionComboBoxText (juce::ComboBox& box, juce::Label& label) {
    label.setBounds (4, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
}
void PastelLNF::drawPopupMenuBackground (juce::Graphics& g, int w, int h) {
    g.setColour (DrumColors::panel);
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 6.f);
    g.setColour (DrumColors::border);
    g.drawRoundedRectangle (0.5f, 0.5f, w - 1.f, h - 1.f, 6.f, 1.f);
}
// arc knob: neutral track, accent value arc, body + needle
void PastelLNF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float start, float end, juce::Slider& sl) {
    auto ac = sl.findColour (juce::Slider::rotarySliderFillColourId);
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float r = juce::jmin (juce::jmin (w, h) * 0.5f - 3.f, 34.f);   // calm: no giant dials
    const float sw = juce::jmax (2.6f, r * 0.15f), arcR = r - sw * 0.5f;
    const auto stroke = juce::PathStrokeType (sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track;
    track.addCentredArc (cx, cy, arcR, arcR, 0.f, start, end, true);
    g.setColour (DrumColors::edge);
    g.strokePath (track, stroke);
    const float a = start + (end - start) * pos;
    const float from = (bool) sl.getProperties()["bipolar"] ? (start + end) * 0.5f : start;   // macros: centre = no change
    if (std::abs (a - from) > 0.01f) {
        juce::Path fill;
        fill.addCentredArc (cx, cy, arcR, arcR, 0.f, juce::jmin (from, a), juce::jmax (from, a), true);
        g.setColour (ac);
        g.strokePath (fill, stroke);
    }
    const float br = r - sw - 2.f;
    g.setColour (juce::Colour (0xff26292f));
    g.fillEllipse (cx - br, cy - br, br * 2, br * 2);
    g.setColour (DrumColors::edge);
    g.drawEllipse (cx - br, cy - br, br * 2, br * 2, 1.f);
    g.setColour (DrumColors::handle);
    g.drawLine (cx + std::sin (a) * br * 0.25f, cy - std::cos (a) * br * 0.25f,
                cx + std::sin (a) * (br - 2.f), cy - std::cos (a) * (br - 2.f), 2.f);
}
void PastelLNF::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                  float, float, juce::Slider::SliderStyle style, juce::Slider& sl) {
    auto ac = sl.findColour (juce::Slider::rotarySliderFillColourId);
    if (style == juce::Slider::LinearVertical) {
        float cx = x + w * 0.5f;
        g.setColour (DrumColors::border);
        g.fillRoundedRectangle (cx - 2, (float) y + 4, 4, (float) h - 8, 2.f);
        float fh = (h - 12) * pos;
        g.setColour (ac);
        g.fillRoundedRectangle (cx - 2, (float) (y + h - 6) - fh, 4, fh, 2.f);
        g.setColour (DrumColors::handle);
        g.fillRoundedRectangle (cx - 11, (float) (y + h - 9) - fh, 22, 8, 3.f);
        return;
    }
    // thin track + round handle (pos arrives in pixels)
    ac = sl.findColour (juce::Slider::thumbColourId);
    const float cy = y + h * 0.5f, l = (float) x + 6.f, rr = (float) (x + w) - 6.f;
    const float px = juce::jlimit (l, rr, pos);
    g.setColour (DrumColors::border);
    g.fillRoundedRectangle (l, cy - 2.f, rr - l, 4.f, 2.f);
    g.setColour (ac);
    g.fillRoundedRectangle (l, cy - 2.f, px - l, 4.f, 2.f);
    g.setColour (DrumColors::handle);
    g.fillEllipse (px - 7.f, cy - 7.f, 14.f, 14.f);
}
void PastelLNF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) {
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 5.f);
    g.setColour (DrumColors::edge);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1, (float) h - 1, 5.f, 1.f);
    juce::Path arrow;
    arrow.startNewSubPath (w - 15.f, h * 0.5f - 2.f);
    arrow.lineTo (w - 11.f, h * 0.5f + 2.f);
    arrow.lineTo (w - 7.f, h * 0.5f - 2.f);
    g.setColour (DrumColors::textDim);
    g.strokePath (arrow, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
// toggled buttons fill with their accent (dark text); the rest are dark with a thin edge
void PastelLNF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool hov, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced (1);
    if (b.getToggleState()) {
        g.setColour (b.findColour (juce::Slider::rotarySliderFillColourId));
        g.fillRoundedRectangle (r, 5.f);
        return;
    }
    g.setColour (down ? bg.brighter (0.12f) : bg);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (hov ? DrumColors::textDim : DrumColors::edge);
    g.drawRoundedRectangle (r, 5.f, 1.f);
}

// ================= Knob =================
Knob::Knob (const juce::String& label, juce::Colour ac) : accent (ac) {
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.setColour (juce::Slider::rotarySliderFillColourId, ac);
    lab.setText (label, juce::dontSendNotification);
    lab.setFont (juce::Font (9.5f, juce::Font::bold));
    lab.setColour (juce::Label::textColourId, DrumColors::textDim);
    lab.setJustificationType (juce::Justification::centred);
    val.setFont (juce::Font (11.f, juce::Font::bold));
    val.setColour (juce::Label::textColourId, DrumColors::text);
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
    drawPadlock (g, { sb.getRight() - 15.f, sb.getY() + 3.f, 10.f, 12.f }, DrumColors::amber);
}
void LockLabel::paint (juce::Graphics& g) {
    juce::Label::paint (g);
    if (locked) drawPadlock (g, { (float) getWidth() - 9.f, 2.f, 7.f, 9.f }, DrumColors::amber);
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
    tune.setVisible (false);                       // part tune lives on the SOUND page (MIX & SENDS)
    level.s.setSliderStyle (juce::Slider::LinearHorizontal);
    level.s.setColour (juce::Slider::thumbColourId, partColour (i));
    level.s.setTooltip ("Level");
    level.s.onValueChange = [this] { repaint(); };   // the dB readout is painted
    solo.setButtonText ("S"); mute.setButtonText ("M");
    solo.setClickingTogglesState (true); mute.setClickingTogglesState (true);
    solo.setColour (juce::Slider::rotarySliderFillColourId, partColour (i));
    mute.setColour (juce::Slider::rotarySliderFillColourId, partColour (i));
    solo.onClick = [this] { proc.parts[idx].solo = solo.getToggleState(); };
    mute.onClick = [this] { proc.parts[idx].mute = mute.getToggleState(); };
    addAndMakeVisible (tune); addAndMakeVisible (level);
    addAndMakeVisible (solo); addAndMakeVisible (mute);
}
// Line icons for the twelve parts (kick front, snare / toms side on, cymbals edge on...).
static void drawPartIcon (juce::Graphics& g, int part, juce::Rectangle<float> box, juce::Colour c) {
    const float s = juce::jmin (box.getWidth() * 0.5f, box.getHeight());   // icon size
    const auto ic = box.withSizeKeepingCentre (s * 2.f, s);
    const float cx = ic.getCentreX(), cy = ic.getCentreY(), H = s;
    const auto stroke = juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    auto drum = [&] (float w, float h, float yTop, bool legs) {          // a drum seen from the side
        juce::Path body;
        body.addEllipse (cx - w * 0.5f, yTop, w, h * 0.28f);              // head
        body.startNewSubPath (cx - w * 0.5f, yTop + h * 0.14f);
        body.lineTo (cx - w * 0.5f, yTop + h);
        body.addCentredArc (cx, yTop + h, w * 0.5f, h * 0.14f, 0.f, -juce::MathConstants<float>::halfPi,
                            juce::MathConstants<float>::halfPi, false);
        body.lineTo (cx + w * 0.5f, yTop + h * 0.14f);
        g.setColour (c.withAlpha (0.16f));
        g.fillRect (cx - w * 0.5f, yTop + h * 0.14f, w, h * 0.86f);
        g.setColour (c);
        g.strokePath (body, stroke);
        for (int i = 1; i < 4; ++i) {                                     // tension rods
            const float x = cx - w * 0.5f + w * (float) i / 4.f;
            g.drawLine (x, yTop + h * 0.32f, x, yTop + h * 0.92f, 1.2f);
        }
        if (legs) {
            g.drawLine (cx - w * 0.42f, yTop + h + h * 0.08f, cx - w * 0.55f, yTop + h + h * 0.3f, 1.4f);
            g.drawLine (cx + w * 0.42f, yTop + h + h * 0.08f, cx + w * 0.55f, yTop + h + h * 0.3f, 1.4f);
        }
    };
    auto cymbal = [&] (float w, float y, float tilt) {                   // a cymbal edge on (bell on top)
        juce::Path p;
        p.startNewSubPath (cx - w * 0.5f, y + tilt);
        p.quadraticTo (cx, y - H * 0.16f, cx + w * 0.5f, y - tilt);
        p.quadraticTo (cx, y + H * 0.04f, cx - w * 0.5f, y + tilt);
        p.closeSubPath();
        g.setColour (c.withAlpha (0.2f)); g.fillPath (p);
        g.setColour (c); g.strokePath (p, stroke);
        g.fillEllipse (cx - w * 0.07f, y - H * 0.14f, w * 0.14f, H * 0.08f);
    };
    auto stand = [&] (float yTop) { g.setColour (c.withAlpha (0.7f)); g.drawLine (cx, yTop, cx, ic.getBottom(), 1.4f); };
    switch (part) {
        case Kick: {      // bass drum, front on: shell, head, beater patch
            const float r = H * 0.48f;
            g.setColour (c.withAlpha (0.16f)); g.fillEllipse (cx - r, cy - r, r * 2, r * 2);
            g.setColour (c); g.drawEllipse (cx - r, cy - r, r * 2, r * 2, 1.6f);
            g.drawEllipse (cx - r * 0.78f, cy - r * 0.78f, r * 1.56f, r * 1.56f, 1.f);
            g.fillEllipse (cx - r * 0.18f, cy - r * 0.18f, r * 0.36f, r * 0.36f);
            break;
        }
        case Snare:  drum (s * 1.25f, H * 0.62f, ic.getY() + H * 0.12f, false);
                     g.setColour (c); g.drawLine (cx - s * 0.6f, ic.getY() + H * 0.05f, cx - s * 0.2f, ic.getY() + H * 0.22f, 1.6f);   // stick
                     break;
        case ClosedHat: cymbal (s * 1.5f, cy - H * 0.1f, 0.f); cymbal (s * 1.5f, cy - H * 0.02f, 0.f); stand (cy); break;
        case OpenHat:   cymbal (s * 1.5f, cy - H * 0.26f, 0.f); cymbal (s * 1.5f, cy + H * 0.06f, 0.f); stand (cy + H * 0.08f); break;
        case Ride:      cymbal (s * 1.9f, cy - H * 0.12f, 0.f); stand (cy - H * 0.06f); break;
        case Crash:     cymbal (s * 1.6f, cy - H * 0.14f, H * 0.12f); stand (cy - H * 0.06f); break;
        case Tom1:   drum (s * 0.8f, H * 0.5f, ic.getY() + H * 0.2f, false); break;
        case Tom2:   drum (s * 0.95f, H * 0.58f, ic.getY() + H * 0.16f, false); break;
        case LowTom: drum (s * 1.1f, H * 0.62f, ic.getY() + H * 0.04f, true); break;
        case Membrane: {  // conga: a tall barrel
            juce::Path p;
            const float w = s * 0.62f, top = ic.getY() + H * 0.04f, bot = ic.getBottom() - H * 0.02f;
            p.addEllipse (cx - w * 0.5f, top, w, H * 0.16f);
            p.startNewSubPath (cx - w * 0.5f, top + H * 0.08f);
            p.quadraticTo (cx - w * 0.72f, (top + bot) * 0.5f, cx - w * 0.36f, bot);
            p.lineTo (cx + w * 0.36f, bot);
            p.quadraticTo (cx + w * 0.72f, (top + bot) * 0.5f, cx + w * 0.5f, top + H * 0.08f);
            g.setColour (c.withAlpha (0.16f)); g.fillPath (p);
            g.setColour (c); g.strokePath (p, stroke);
            break;
        }
        case Metallic: {  // cowbell
            juce::Path p;
            const float top = ic.getY() + H * 0.12f, bot = ic.getBottom() - H * 0.06f;
            p.startNewSubPath (cx - s * 0.22f, top);
            p.lineTo (cx + s * 0.22f, top);
            p.lineTo (cx + s * 0.42f, bot);
            p.lineTo (cx - s * 0.42f, bot);
            p.closeSubPath();
            g.setColour (c.withAlpha (0.16f)); g.fillPath (p);
            g.setColour (c); g.strokePath (p, stroke);
            g.drawLine (cx - s * 0.1f, top, cx - s * 0.1f, top - H * 0.08f, 1.4f);
            g.drawLine (cx + s * 0.1f, top, cx + s * 0.1f, top - H * 0.08f, 1.4f);
            break;
        }
        default: {        // SFX: a bolt
            juce::Path p;
            p.startNewSubPath (cx + s * 0.12f, ic.getY());
            p.lineTo (cx - s * 0.24f, cy + H * 0.06f);
            p.lineTo (cx + s * 0.02f, cy + H * 0.06f);
            p.lineTo (cx - s * 0.12f, ic.getBottom());
            p.lineTo (cx + s * 0.26f, cy - H * 0.08f);
            p.lineTo (cx, cy - H * 0.08f);
            p.closeSubPath();
            g.setColour (c.withAlpha (0.25f)); g.fillPath (p);
            g.setColour (c); g.strokePath (p, stroke);
            break;
        }
    }
}

void PartCard::paint (juce::Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced (3.f, 2.f);
    const bool isSel = proc.selectedPart.load() == idx;
    const auto col = partColour (idx);
    g.setColour (DrumColors::panel);
    g.fillRoundedRectangle (r, 8.f);
    if (isSel) { g.setColour (col.withAlpha (0.09f)); g.fillRoundedRectangle (r, 8.f); }
    g.setColour (isSel ? col : DrumColors::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.f, isSel ? 1.8f : 1.f);
    // dot + name, note on the right
    g.setColour (col);
    g.fillEllipse (r.getX() + 8.f, r.getY() + 9.f, 6.f, 6.f);
    g.setColour (DrumColors::text);
    g.setFont (juce::Font (11.f, juce::Font::bold));
    g.drawText (juce::String (partName (idx)), (int) r.getX() + 18, (int) r.getY() + 4, (int) r.getWidth() - 44, 16, juce::Justification::centredLeft);
    g.setColour (DrumColors::faint);
    g.setFont (juce::Font (9.f, juce::Font::bold));
    g.drawText (noteName (proc.parts[idx].midiNote), (int) r.getRight() - 32, (int) r.getY() + 4, 26, 16, juce::Justification::centredRight);
    // engine model + REVERSE badge (blank for the CLASSIC voice)
    juce::String sub;
    if (auto* md = modelDef (idx, proc.parts[idx].params.model)) sub = md->name;
    if (proc.parts[idx].params.reverse) sub = sub.isEmpty() ? "REV" : sub + " / REV";
    g.setColour (col.withAlpha (sub.isEmpty() ? 0.45f : 0.9f));
    g.setFont (juce::Font (8.5f, juce::Font::bold));
    g.drawText (sub.isEmpty() ? juce::String ("CLASSIC") : sub, (int) r.getX() + 8, (int) r.getY() + 19, (int) r.getWidth() - 12, 11,
                juce::Justification::centredLeft);
    // what the part is: a line icon in its colour
    auto well = juce::Rectangle<float> (r.getX() + 5.f, r.getY() + 34.f, r.getWidth() - 10.f, r.getHeight() - 82.f);
    g.setColour (DrumColors::inner);
    g.fillRoundedRectangle (well, 4.f);
    drawPartIcon (g, idx, well.reduced (4.f, 3.f), col.withAlpha (isSel ? 1.f : 0.85f));
    // level readout under the bar
    const float lv = (float) level.s.getValue();
    g.setColour (DrumColors::textDim);
    g.setFont (juce::Font (9.f, juce::Font::bold));
    g.drawText (lv <= 0.0005f ? juce::String ("-inf") : juce::String (20.f * std::log10 (lv), 1) + " dB",
                (int) r.getX() + 6, getHeight() - 22, 60, 16, juce::Justification::centredLeft);
}

// ---------------- SOUND page hero ----------------
void HitView::resized() {
    if (amp != nullptr) amp->setBounds (getLocalBounds());
    if (pitch != nullptr) pitch->setBounds (getLocalBounds().withTrimmedBottom (22));
}
void HitView::paint (juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    auto well = r.withTrimmedBottom (amp != nullptr ? 22.f : 0.f);
    g.setColour (DrumColors::inner);
    g.fillRoundedRectangle (well, 6.f);
    g.setColour (DrumColors::grid);
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (well.getX() + well.getWidth() * (float) i / 8.f), well.getY() + 1, well.getBottom() - 1);
    for (int j = 1; j < 4; ++j) g.drawHorizontalLine ((int) (well.getY() + well.getHeight() * (float) j / 4.f), well.getX() + 1, well.getRight() - 1);
    if (! wave.empty()) {   // the rendered hit, faint, under the envelopes
        const auto p = well.reduced (10.f, 10.f);
        const float cy = p.getCentreY(), half = p.getHeight() * 0.5f;
        const float bw = p.getWidth() / (float) wave.size();
        g.setColour (colour.withAlpha (amp != nullptr ? 0.32f : 0.75f));
        for (size_t i = 0; i < wave.size(); ++i) {
            const float a = juce::jmax (0.5f, wave[i] * half);
            g.fillRect (p.getX() + bw * (float) i, cy - a, juce::jmax (1.f, bw * 0.7f), 2.f * a);
        }
    }
    if (note.isNotEmpty()) {
        g.setColour (DrumColors::textDim);
        g.setFont (juce::Font (10.f, juce::Font::bold));
        g.drawText (note.toUpperCase(), well.reduced (10.f, 6.f), juce::Justification::topLeft);
    }
}

void CardBg::paint (juce::Graphics& g) {
    auto r = getLocalBounds().toFloat();
    g.setColour (DrumColors::inner);
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (juce::Colour (0xff22252a));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
    g.setColour (colour);
    g.fillEllipse (r.getX() + 10.f, r.getY() + 11.f, 5.f, 5.f);
    g.setColour (DrumColors::text);
    g.setFont (juce::Font (10.f, juce::Font::bold));
    g.drawText (title, (int) r.getX() + 20, (int) r.getY() + 4, (int) r.getWidth() - 24, 18, juce::Justification::centredLeft);
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
    // name / model / hit thumbnail are painted; level bar, then S / M
    const int w = getWidth(), h = getHeight();
    level.setBounds (6, h - 40, w - 12, 16);
    mute.setBounds (w - 27, h - 22, 20, 16);
    solo.setBounds (w - 49, h - 22, 20, 16);
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
    bypassB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa9d58e));
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
    emptyLab.setColour (juce::Label::textColourId, DrumColors::faint);
    addAndMakeVisible (typeBox); addAndMakeVisible (bypassB);
    addAndMakeVisible (addB); addAndMakeVisible (delB);
    addAndMakeVisible (upB); addAndMakeVisible (dnB);
    addAndMakeVisible (emptyLab);
    for (int i = 0; i < FxProcessor::MaxKnobs; ++i) {
        knobs.push_back (std::make_unique<Knob> ("", juce::Colour (0xffe9a3b8)));
        addAndMakeVisible (*knobs.back());
    }
    addAndMakeVisible (picture);
    picture.get = [this] (int k) { return selected >= 0 ? getParam (selected, k) : 0.f; };
    picture.set = [this] (int k, float v) { if (selected >= 0) setParam (selected, k, v); };
    picture.text = [this] (int k, float v) { return fxKnobText (picture.type, k, v); };
    picture.onEdit = [this] { syncKnobs(); };
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
    static const juce::uint32 kcols[8] = { 0xffe9a3b8,0xffe88a8a,0xfff2b48f,0xffa9d58e,0xff8fd0c2,0xffb9a7f0,0xff86c5c5,0xff8fc6e8 };
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
    picture.setVisible (hasSel);
    picture.type = hasSel ? t : -1;
    for (auto& k : knobs) k->setVisible (hasSel);
    emptyLab.setVisible (! hasSel);
    if (hasSel) {
        auto& sl = ch.slots[(size_t) selected];
        typeBox.setSelectedId (t + 1, juce::dontSendNotification);
        bypassB.setToggleState (! sl.bypass.load(), juce::dontSendNotification);
        bypassB.setButtonText (sl.bypass.load() ? "OFF" : "ON");
        auto c = juce::Colour (kcols[t < 8 ? t : 0]);
        picture.colour = c;
        picture.repaint();
        for (int k = 0; k < nk; ++k) {
            knobs[(size_t) k]->setVisible (true);
            knobs[(size_t) k]->s.setColour (juce::Slider::rotarySliderFillColourId, c);
            knobs[(size_t) k]->lab.setText (FxProcessor::knobLabel (t, k), juce::dontSendNotification);
            knobs[(size_t) k]->val.setColour (juce::Label::textColourId, DrumColors::text);
            int slot = selected;
            knobs[(size_t) k]->setManual (getParam (slot, k),
                [this, slot, k] (float nv) { setParam (slot, k, nv); picture.repaint(); },
                [t, k] (float nv) { return fxKnobText (t, k, nv); });
        }
        for (int k = nk; k < (int) knobs.size(); ++k) knobs[(size_t) k]->setVisible (false);
    }
    resized();
    repaint();
}

void DrumEditor::FxChainView::syncKnobs() {
    if (selected < 0) return;
    const int t = chain().slots[(size_t) selected].type.load();
    for (int k = 0; k < FxProcessor::numKnobs (t) && k < (int) knobs.size(); ++k) {
        const float v = getParam (selected, k);
        knobs[(size_t) k]->s.setValue (v, juce::dontSendNotification);
        knobs[(size_t) k]->val.setText (fxKnobText (t, k, v), juce::dontSendNotification);
    }
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
    dt.removeFromTop (6);
    picture.setBounds (dt.removeFromTop ((int) (dt.getHeight() * 0.58f)).reduced (2, 0));
    dt.removeFromTop (8);
    // the slot's knobs: one compact row under the picture
    int vis = 0;
    for (auto& k : knobs) if (k->isVisible()) ++vis;
    const int kcw = dt.getWidth() / juce::jmax (1, vis);
    int vi = 0;
    for (auto& k : knobs) {
        if (! k->isVisible()) continue;
        k->setBounds (dt.getX() + kcw * vi, dt.getY(), kcw, juce::jmin (dt.getHeight(), 112));
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
        knobs[k] = std::make_unique<Knob> ("", juce::Colour (0xffe9a3b8));
        knobs[k]->val.setVisible (false); // value is painted next to the name instead
        knobs[k]->lab.setVisible (false);
        knobs[k]->s.setTooltip ("Macro: centre = no change. Click the target strip below to choose what it moves.");
        knobs[k]->s.getProperties().set ("bipolar", true);
        addAndMakeVisible (*knobs[k]);
        auto& nl = names[k];
        nl.setEditable (false, true, false);
        nl.setFont (juce::Font (11.f, juce::Font::bold));
        nl.setColour (juce::Label::textColourId, DrumColors::text);
        nl.setColour (juce::Label::backgroundColourId, DrumColors::control);
        nl.setColour (juce::Label::outlineColourId, DrumColors::edge);
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
    g.setColour (DrumColors::faint);
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText ("CLICK TARGETS TO EDIT", 0, 0, getWidth(), 14, juce::Justification::centredLeft);
    float uw = (float) getWidth() / DrumProcessor::NumMacros;
    for (int k = 0; k < DrumProcessor::NumMacros; ++k) {
        float ux = uw * k;
        if (k > 0) { g.setColour (DrumColors::border); g.drawVerticalLine ((int) ux, 20.f, (float) getHeight() - 4.f); }
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
        g.setColour (DrumColors::control);
        g.fillRoundedRectangle (tr.toFloat(), 3.f);
        g.setColour (labels.isEmpty() ? DrumColors::faint : DrumColors::text);
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
    presetBtn.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe9a3b8));
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
    randPartB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe9a3b8));
    mutPartB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffb9a7f0));
    masterDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    masterDial.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    masterDial.setColour (juce::Slider::rotarySliderFillColourId, DrumColors::amber);
    // MASTER dial + OUTPUT fader both drive the "master" parameter (the
    // processor re-reads it every block, so direct writes were overwritten)
    masterDialAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterDial);
    masterLab.setText ("MASTER", juce::dontSendNotification);
    masterLab.setFont (juce::Font (9.5f, juce::Font::bold));
    masterLab.setJustificationType (juce::Justification::centredRight);
    masterLab.setColour (juce::Label::textColourId, DrumColors::textDim);
    limitB.setClickingTogglesState (true);
    limitB.setToggleState (proc.limiterOn.load(), juce::dontSendNotification);
    limitB.onClick = [this] { proc.limiterOn = limitB.getToggleState(); };

    addAndMakeVisible (presetBtn); addAndMakeVisible (prevB); addAndMakeVisible (nextB);
    addAndMakeVisible (scopeBox); addAndMakeVisible (randPartB); addAndMakeVisible (mutPartB);
    addAndMakeVisible (randKitB); addAndMakeVisible (undoB); addAndMakeVisible (redoB); addAndMakeVisible (initB);
    addAndMakeVisible (masterDial); addAndMakeVisible (masterLab);
    kitLab.setFont (juce::Font (10.5f, juce::Font::bold));
    kitLab.setColour (juce::Label::textColourId, DrumColors::faint);
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
    revB.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffb9a7f0));
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
    for (auto* l : { (juce::Label*) &modelLab, &outLab, &chokeLab, (juce::Label*) &waveLab, (juce::Label*) &noiseLab }) {
        l->setFont (juce::Font (10.f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, DrumColors::textDim);
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
    lockHint.setColour (juce::Label::textColourId, DrumColors::faint);
    synthPanel.addAndMakeVisible (lockHint);
    addAndMakeVisible (scopeView); addAndMakeVisible (analyzerView);   // right column, every view
    addAndMakeVisible (meterComp);                                     // header
    lockHint.setText ("RIGHT-CLICK A KNOB = LOCK", juce::dontSendNotification);
    addAndMakeVisible (synthPanel);

    // fx chain: tabbed detail view (empty by default, + ADD to insert)
    fxPanel.addAndMakeVisible (fxTitle); fxPanel.addAndMakeVisible (fxHint);
    for (auto* l : { &fxTitle, &gloTitle, &fxHint }) {
        l->setFont (juce::Font (l == &fxHint ? 9.f : 11.f, juce::Font::bold));
        l->setColour (juce::Label::textColourId, l == &fxHint ? DrumColors::faint : DrumColors::text);
    }
    fxPanel.addAndMakeVisible (partFxPower);
    fxPanel.addAndMakeVisible (partFxLock);
    partFxLock.setClickingTogglesState (true);
    partFxLock.setTooltip ("Lock this part's effects chain from RANDOMIZE / MUTATE / KIT RND");
    partFxLock.setColour (juce::Slider::rotarySliderFillColourId, DrumColors::amber); // amber like the padlocks
    partFxLock.onClick = [this] { proc.setLocked (sel(), DrumProcessor::LockFx, partFxLock.getToggleState()); };
    partFxPower.setClickingTogglesState (true);
    partFxPower.setToggleState (true, juce::dontSendNotification);
    partFxPower.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa9d58e));
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
    globPower.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffa9d58e));
    globPower.onClick = [this] {
        proc.globalFx.enabled.store (globPower.getToggleState());
        refreshFxRows();
    };
    globFxView = std::make_unique<FxChainView> (*this, true);
    globalPanel.addAndMakeVisible (*globFxView);
    addAndMakeVisible (globalPanel);

    outPanel.addAndMakeVisible (outTitle);
    outTitle.setFont (juce::Font (9.5f, juce::Font::bold));
    outTitle.setColour (juce::Label::textColourId, DrumColors::textDim);
    outPanel.addAndMakeVisible (masterFader);
    outPanel.addAndMakeVisible (limitB);
    masterFader.setColour (juce::Slider::thumbColourId, juce::Colour (0xffe9a3b8));
    masterFader.setColour (juce::Slider::trackColourId, juce::Colour (0xffe9a3b8).withAlpha (0.5f));
    masterFader.setColour (juce::Slider::backgroundColourId, DrumColors::border);
    masterFaderAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "master", masterFader);
    addAndMakeVisible (outPanel);
    outPanel.setVisible (false);   // v3: master + limiter live in the header (the dial drives the same parameter)
    addAndMakeVisible (limitB);

    viewSynth.setClickingTogglesState (true); viewPartFx.setClickingTogglesState (true); viewGlobFx.setClickingTogglesState (true);
    viewSynth.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe9a3b8));
    viewPartFx.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe9a3b8));
    viewGlobFx.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffe9a3b8));
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
    for (auto& c : cardBgs) synthPanel.removeChildComponent (c.get());
    cardBgs.clear();
    groups.clear();
    if (hitView != nullptr) { synthPanel.removeChildComponent (hitView.get()); hitView.reset(); }
    int p = sel();
    applyPartAccent (p);
    const auto col = partColour (p);
    partTitle.setText (juce::String (partName (p)), juce::dontSendNotification);
    partTitle.setFont (juce::Font (20.f, juce::Font::bold));
    partTitle.setColour (juce::Label::textColourId, DrumColors::text);
    const int model = shownModel();
    const ModelDef* md = modelDef (p, model);
    if (tab == 1 || tab == 2) tab = 0;   // AMP / PITCH live on the SOUND page's hit graph now
    tabAmp.setVisible (false); tabPitch.setVisible (false);
    waveBox.setVisible (md == nullptr && tab == 0); waveLab.setVisible (md == nullptr && tab == 0);
    noiseBox.setVisible (md == nullptr && tab == 0); noiseLab.setVisible (md == nullptr && tab == 0);
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
    tabSend.setToggleState (tab == 3, juce::dontSendNotification);
    tabMacro.setToggleState (tab == 4, juce::dontSendNotification);
    macroPanel->setVisible (tab == 4);
    if (tab == 4) macroPanel->refresh();

    // ---- knob factories (every knob keeps its right-click lock) ----
    auto adopt = [&] (std::unique_ptr<Knob> kb) {
        kb->s.setColour (juce::Slider::rotarySliderFillColourId, col);
        kb->val.setColour (juce::Label::textColourId, DrumColors::text);
        synthPanel.addAndMakeVisible (*kb);
        synthKnobs.push_back (std::move (kb));
        return synthKnobs.back().get();
    };
    auto slotKnob = [&] (int s) {
        const ParamMeta* sm = slotMeta (p, s);
        ParamMeta mm = sm ? *sm : ParamMeta { "?", 0.f, 1.f, 0.f, false, "" };
        auto* kb = adopt (std::make_unique<Knob> (mm.label, col));
        kb->attach (proc.apvts, synthParamID (p, s));
        kb->s.onValueChange = [kb, mm] { kb->val.setText (fmtVal (mm, (float) kb->s.getValue()), juce::dontSendNotification); };
        kb->locked = proc.isLocked (p, s);
        kb->s.onRightClick = [this, p, s, kb] { proc.toggleLock (p, s); kb->locked = proc.isLocked (p, s); kb->repaint(); };
        kb->s.onValueChange();
        return kb;
    };
    auto modelKnob = [&] (int k) {
        const ModelParam mp = md->p[k];
        auto* kb = adopt (std::make_unique<Knob> (mp.label, col));
        kb->attach (proc.apvts, DrumProcessor::modelParamID (p, k));
        kb->s.onValueChange = [kb, mp] { kb->val.setText (modelFormat (mp, modelCook (mp, (float) kb->s.getValue())), juce::dontSendNotification); };
        const int key = DrumProcessor::modelKnobLock (k);
        kb->locked = proc.isLocked (p, key);
        kb->s.onRightClick = [this, p, key, kb] {
            proc.toggleLock (p, key);
            kb->locked = proc.isLocked (p, key);
            kb->repaint();
            if (kb->locked) {   // a knob lock holds the model too (its knob means something else in another model)
                proc.setLocked (p, DrumProcessor::LockModel, true);
                modelLab.locked = true; modelLab.repaint();
            }
        };
        kb->s.onValueChange();
        return kb;
    };
    auto paramKnob = [&] (const juce::String& label, const juce::String& id, std::function<juce::String (float)> fmt, int lockKey) {
        auto* kb = adopt (std::make_unique<Knob> (label, col));
        kb->attach (proc.apvts, id);
        kb->s.onValueChange = [kb, fmt] { kb->val.setText (fmt ((float) kb->s.getValue()), juce::dontSendNotification); };
        if (lockKey >= 0) {
            kb->locked = proc.isLocked (p, lockKey);
            kb->s.onRightClick = [this, p, lockKey, kb] { proc.toggleLock (p, lockKey); kb->locked = proc.isLocked (p, lockKey); kb->repaint(); };
        }
        kb->s.onValueChange();
        return kb;
    };
    auto group = [&] (const juce::String& title, std::vector<Knob*> ks) {
        if (ks.empty()) return;
        cardBgs.push_back (std::make_unique<CardBg> (title, col));
        synthPanel.addAndMakeVisible (*cardBgs.back());
        cardBgs.back()->toBack();
        groups.push_back ({ {}, std::move (ks) });
    };
    auto pct = [] (float v) { return juce::String ((int) std::round (v * 100)) + "%"; };

    if (tab == 0) {
        // the hero: the part's rendered hit, amp envelope + pitch sweep dragged on top
        const bool cymbal = p == ClosedHat || p == OpenHat || p == Ride || p == Crash;
        hitView = std::make_unique<HitView>();
        hitView->colour = col;
        if (md == nullptr) {
            hitView->amp = std::make_unique<pastel::EnvView> (proc.apvts,
                juce::StringArray { synthParamID (p, 4), synthParamID (p, 5), synthParamID (p, 6), synthParamID (p, 7) }, "AMP");
            hitView->amp->setOverlay (true);
            hitView->amp->setTimeSkew (2.f);
            hitView->amp->getProperties().set ("accent", (int) col.getARGB());
            hitView->amp->setTooltip ("Amp: drag attack, decay / sustain, release (double-click = defaults)");
            hitView->addAndMakeVisible (*hitView->amp);
            if (! cymbal) {
                hitView->pitch = std::make_unique<pastel::SweepView> (proc.apvts, synthParamID (p, 1), synthParamID (p, 2), synthParamID (p, 3), "PITCH");
                hitView->pitch->setOverlay (true);
                hitView->pitch->setTimeSkew (2.f);
                hitView->pitch->getProperties().set ("accent", (int) DrumColors::handle.getARGB());
                hitView->pitch->setTooltip ("Pitch: drag the start for the amount, the end for the decay, the middle for the curve");
                hitView->addAndMakeVisible (*hitView->pitch);
            }
        } else {
            hitView->note = juce::String (md->name) + " shapes its own envelope: see its knobs";
        }
        if (hitPart != p) { hitPart = p; hitSig = 0; }
        else hitSig = 0;   // a rebuilt view starts empty: render again
        synthPanel.addAndMakeVisible (*hitView);

        if (md != nullptr) {
            std::vector<Knob*> ks;
            for (int k = 0; k < md->nparams; ++k) ks.push_back (modelKnob (k));
            group ("MODEL  " + juce::String (md->name), ks);
        } else {
            // the classic voice's knobs, grouped by what they shape
            std::vector<int> slots { 0 };
            if (cymbal) slots.push_back (1);   // SPREAD (cymbals have no pitch sweep)
            for (int s = 8; s < 16; ++s) slots.push_back (s);
            for (int s = 16; s < 20; ++s) if (advMetaForPart (p, s)) slots.push_back (s);
            const char* order[] = { "TONE", "SUB", "CLICK", "NOISE", "LOW BOOST", "LFO" };
            auto groupOf = [] (const juce::String& l) -> juce::String {
                if (l.startsWith ("SUB")) return "SUB";
                if (l.startsWith ("CLICK")) return "CLICK";
                if (l.contains ("NOISE") || l == "N DECAY" || l == "NZ>FX") return "NOISE";
                if (l.startsWith ("BOOST")) return "LOW BOOST";
                if (l.startsWith ("LFO") || l == "FM RATIO") return "LFO";
                return "TONE";
            };
            for (auto* g : order) {
                std::vector<Knob*> ks;
                for (int s : slots)
                    if (auto* sm = slotMeta (p, s); sm != nullptr && groupOf (sm->label) == g) ks.push_back (slotKnob (s));
                group (g, ks);
            }
        }
        // mix & sends: part tune / level / pan + the two send levels
        auto dbFmt = [] (float v) { return v <= 0.0005f ? juce::String ("-inf") : juce::String (20.f * std::log10 (v), 1) + " dB"; };
        auto panFmt = [] (float v) { return std::abs (v - 0.5f) < 0.02f ? juce::String ("C") : (v < 0.5f ? "L" + juce::String ((int) ((0.5f - v) * 200)) : "R" + juce::String ((int) ((v - 0.5f) * 200))); };
        std::vector<Knob*> mix;
        mix.push_back (paramKnob ("TUNE", "tune" + juce::String (p), [] (float v) { return (v > 0 ? "+" : "") + juce::String (v, 1) + " st"; }, -1));
        mix.push_back (paramKnob ("LEVEL", "mix" + juce::String (p) + "_lvl", dbFmt, -1));
        mix.push_back (paramKnob ("PAN", "mix" + juce::String (p) + "_pan", panFmt, -1));
        mix.push_back (paramKnob ("SEND REV", DrumProcessor::sendParamID (p, false), pct, DrumProcessor::LockSendRev));
        mix.push_back (paramKnob ("SEND DLY", DrumProcessor::sendParamID (p, true), pct, DrumProcessor::LockSendDly));
        mix[1]->s.getProperties().set ("bipolar", false);
        mix[2]->s.getProperties().set ("bipolar", true);
        mix[0]->s.getProperties().set ("bipolar", true);
        group ("MIX & SENDS", mix);
    }
    else if (tab == 3) {
        // the shared reverb / delay returns (every part feeds them)
        auto* pr = &proc;
        auto manual = [&] (const char* label, std::atomic<float>* tgt, std::function<juce::String (float)> fmt) {
            auto* kb = adopt (std::make_unique<Knob> (label, col));
            kb->setManual (tgt->load(), [tgt] (float v) { *tgt = v; }, fmt);
            return kb;
        };
        auto dlyFmt = [pr] (float v) {
            int d = pr->dlySync.load();
            float sec = d >= 0 ? (float) TempoSync::divisionSeconds (d, pr->hostBpm.load()) : FxProcessor::sendDelaySeconds (v);
            return (d >= 0 ? juce::String (TempoSync::divisionName (d)) + "  " : juce::String()) + juce::String (juce::roundToInt (sec * 1000.f)) + "ms";
        };
        auto* rd = manual ("REV DECAY", &proc.revDecay, [] (float v) { return juce::String (FxProcessor::rt60FromKnob (v), 2) + "s"; });
        auto* rp = manual ("REV DAMP", &proc.revDamp, pct);
        group ("SHARED REVERB", { rd, rp });
        auto* timeK = manual ("DLY TIME", &proc.dlyTime, dlyFmt);
        auto* fb = manual ("DLY FDBK", &proc.dlyFb, pct);
        // DLY SYNC: FREE, then the shared tempo divisions (host tempo, 120 BPM without one)
        static constexpr int steps = TempoSync::NumDivisions;   // static: usable in the lambdas below without a capture (MSVC)
        auto* sk = adopt (std::make_unique<Knob> ("DLY SYNC", col));
        sk->setManual ((float) (proc.dlySync.load() + 1) / steps, [pr, timeK] (float v) {
            pr->dlySync = juce::roundToInt (v * steps) - 1;
            timeK->s.setAlpha (pr->dlySync.load() >= 0 ? 0.45f : 1.f);
            timeK->val.setText (timeK->fmtr ((float) timeK->s.getValue()), juce::dontSendNotification);
        }, [] (float v) { int d = juce::roundToInt (v * steps) - 1; return d < 0 ? juce::String ("FREE") : juce::String (TempoSync::divisionName (d)); });
        sk->s.setRange (0.0, 1.0, 1.0 / steps);
        sk->s.setTooltip ("FREE: DLY TIME sets the delay. Otherwise the delay follows the host tempo.");
        timeK->s.setAlpha (proc.dlySync.load() >= 0 ? 0.45f : 1.f);
        group ("SHARED DELAY", { timeK, fb, sk });
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
    g.fillAll (DrumColors::bg);
    // header: the Pastel family mark + name
    const juce::uint32 cols[4] = { 0xffe88a8a, 0xff8fd0c2, 0xffe9a3b8, 0xffa9d58e };
    for (int i = 0; i < 4; ++i) {
        g.setColour (juce::Colour (cols[i]));
        g.fillRoundedRectangle (16.f + (float) (i % 2) * 10.f, 16.f + (float) (i / 2) * 10.f, 8.f, 8.f, 2.f);
    }
    g.setColour (DrumColors::text);
    g.setFont (lnf.latoBlack != nullptr ? juce::Font (juce::FontOptions (lnf.latoBlack)).withHeight (16.f) : juce::Font (16.f, juce::Font::bold));
    g.drawText ("PASTEL DRUM SYNTH", 42, 12, 200, 28, juce::Justification::centredLeft);
    g.setColour (DrumColors::border);
    g.fillRect (8, 51, getWidth() - 16, 1);
    if (! contentRect.isEmpty()) {
        g.setColour (DrumColors::panel);
        g.fillRoundedRectangle (contentRect.toFloat(), 8.f);
        g.setColour (DrumColors::border);
        g.drawRoundedRectangle (contentRect.toFloat().reduced (0.5f), 8.f, 1.f);
    }
}

void DrumEditor::resized() {
    if (getWidth() < 50 || getHeight() < 50) return;
    if (cards.size() < 12 || ! partFxView || ! globFxView) return;
    auto r = getLocalBounds();
    // ---- header: logo | undo redo | < kit > | ... | PART/KIT RANDOMIZE MUTATE KIT RND INIT | LIMITER meter MASTER
    auto header = r.removeFromTop (52).reduced (4, 10);
    header.removeFromLeft (232);                         // logo + name (painted)
    undoB.setBounds (header.removeFromLeft (52).reduced (2, 0));
    redoB.setBounds (header.removeFromLeft (52).reduced (2, 0));
    header.removeFromLeft (10);
    prevB.setBounds (header.removeFromLeft (30).reduced (2, 0));
    presetBtn.setBounds (header.removeFromLeft (200).reduced (2, 0));
    nextB.setBounds (header.removeFromLeft (30).reduced (2, 0));
    auto mhead = header.removeFromRight (100);
    masterLab.setBounds (mhead.removeFromLeft (54));
    masterDial.setBounds (mhead.withSizeKeepingCentre (34, 34));
    meterComp.setBounds (header.removeFromRight (64).reduced (2, 8));
    limitB.setBounds (header.removeFromRight (72).reduced (2, 0));
    header.removeFromRight (6);
    initB.setBounds (header.removeFromRight (50).reduced (2, 0));
    randKitB.setBounds (header.removeFromRight (70).reduced (2, 0));
    mutPartB.setBounds (header.removeFromRight (74).reduced (2, 0));
    randPartB.setBounds (header.removeFromRight (94).reduced (2, 0));
    scopeBox.setBounds (header.removeFromRight (70).reduced (2, 0));
    kitLab.setBounds (header.reduced (6, 0)); // free space: kit genre
    kitLab.setVisible (header.getWidth() > 60);

    // ---- pad strip: every part's hit
    auto cardRowRect = r.removeFromTop (124).reduced (4, 2);
    cardRow.setBounds (cardRowRect);
    int cw = cardRowRect.getWidth() / 12;
    for (int i = 0; i < 12; ++i) {
        int x = i * cw;
        int w = (i == 11) ? (cardRowRect.getWidth() - x) : cw;
        this->cards[(size_t) i]->setBounds (x, 0, w, cardRowRect.getHeight());
    }

    // ---- left: view tabs + the part editor; right: scope over spectrum
    r.removeFromBottom (8);
    auto right = r.removeFromRight ((int) (r.getWidth() * 0.35f)).reduced (4, 4);
    scopeView.setBounds (right.removeFromTop ((int) (right.getHeight() * 0.52f)).withTrimmedBottom (4));
    analyzerView.setBounds (right.withTrimmedTop (4));
    auto tabRowMain = r.removeFromTop (36).reduced (8, 4);
    viewSynth.setBounds (tabRowMain.removeFromLeft (118));
    viewPartFx.setBounds (tabRowMain.removeFromLeft (118));
    viewGlobFx.setBounds (tabRowMain.removeFromLeft (118));
    footerRect = {};
    contentRect = r.reduced (8, 2);
    auto content = contentRect.reduced (4);
    synthPanel.setBounds (content);
    fxPanel.setBounds (content);
    globalPanel.setBounds (content);

    // ---- part editor internals
    auto sr = synthPanel.getLocalBounds().reduced (10, 8);
    {   // name | MODEL | REVERSE | CHOKE | OUT | TRIGGER
        auto titleRow = sr.removeFromTop (28);
        trigB.setBounds (titleRow.removeFromRight (96).reduced (1));
        titleRow.removeFromRight (8);
        outBox.setBounds (titleRow.removeFromRight (84).reduced (1));
        outLab.setBounds (titleRow.removeFromRight (34).reduced (1));
        chokeBox.setBounds (titleRow.removeFromRight (84).reduced (1));
        chokeLab.setBounds (titleRow.removeFromRight (46).reduced (1));
        revB.setBounds (titleRow.removeFromRight (78).reduced (1, 2));
        titleRow.removeFromRight (4);
        modelBox.setBounds (titleRow.removeFromRight (136).reduced (1));
        modelLab.setBounds (titleRow.removeFromRight (54).reduced (1));
        partTitle.setBounds (titleRow);
    }
    sr.removeFromTop (6);
    {   // pages | wave / noise
        auto pageRow = sr.removeFromTop (24);
        tabSynth.setBounds (pageRow.removeFromLeft (82));
        tabSend.setBounds (pageRow.removeFromLeft (82));
        tabMacro.setBounds (pageRow.removeFromLeft (82));
        lockHint.setBounds (pageRow.removeFromLeft (180).reduced (10, 1));
        noiseBox.setBounds (pageRow.removeFromRight (104).reduced (1, 0));
        noiseLab.setBounds (pageRow.removeFromRight (52).reduced (1, 0));   // room for the padlock
        pageRow.removeFromRight (6);
        waveBox.setBounds (pageRow.removeFromRight (96).reduced (1, 0));
        waveLab.setBounds (pageRow.removeFromRight (48).reduced (1, 0));
    }
    sr.removeFromTop (8);
    if (macroPanel) macroPanel->setBounds (sr);
    if (hitView != nullptr) {
        hitView->setBounds (sr.removeFromTop (juce::jmin (190, sr.getHeight() / 2)));
        sr.removeFromTop (8);
    }
    // knob cards: packed left to right in rows; narrower cells if they do not fit
    if (! groups.empty()) {
        const int gap = 8, titleH = 24;
        for (int cellW : { 72, 64, 56 }) {
            const int cellH = juce::jmin (100, (sr.getHeight() - gap) / 2 - titleH - 4);
            int x = sr.getX(), y = sr.getY(), rowH = titleH + cellH + 4;
            bool fits = true;
            for (auto& gr : groups) {
                const int w = 12 + (int) gr.knobs.size() * cellW;
                if (x + w > sr.getRight() && x > sr.getX()) { x = sr.getX(); y += rowH + gap; }
                gr.bounds = { x, y, juce::jmin (w, sr.getWidth()), rowH };
                x += w + gap;
            }
            if (y + rowH > sr.getBottom() + 1) fits = false;
            for (size_t gi = 0; gi < groups.size(); ++gi) {
                auto& gr = groups[gi];
                cardBgs[gi]->setBounds (gr.bounds);
                for (size_t k = 0; k < gr.knobs.size(); ++k)
                    gr.knobs[k]->setBounds (gr.bounds.getX() + 6 + (int) k * cellW, gr.bounds.getY() + titleH, cellW, cellH);
            }
            if (fits) break;
        }
    }

    // fx chain views (chips + tabbed detail)
    auto fr = fxPanel.getLocalBounds().reduced (8);
    auto fxHead = fr.removeFromTop (18);
    partFxPower.setBounds (fxHead.removeFromRight (56).reduced (1));
    partFxLock.setBounds (fxHead.removeFromRight (56).reduced (1));
    fxHint.setBounds (fxHead.removeFromRight (140).reduced (1));
    fxTitle.setBounds (fxHead.reduced (1));
    if (partFxView) partFxView->setBounds (fr);
    auto gr = globalPanel.getLocalBounds().reduced (8);
    auto gHead = gr.removeFromTop (18);
    globPower.setBounds (gHead.removeFromRight (56).reduced (1));
    gloTitle.setBounds (gHead.reduced (1));
    if (globFxView) globFxView->setBounds (gr);

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
    // the selected part's rendered hit for the SOUND page (renders only when its sound changed)
    if (hitView != nullptr) {
        auto th = proc.renderThumbnail (sel(), 320, hitSig);
        if (! th.empty()) { hitView->wave = std::move (th); hitView->repaint(); }
    }
    scopeView.repaint();
    analyzerView.repaint();
    meterComp.repaint();
    repaint();
}

} // namespace mds

juce::AudioProcessorEditor* mds::DrumProcessor::createEditor() { return new mds::DrumEditor (*this); }
