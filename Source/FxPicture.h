#pragma once
#include <JuceHeader.h>
#include <functional>
#include "Effects.h"

namespace mds {

// v3 FX-chain hero (after Pastel EFX Grid): a picture of the selected effect
// that you drag. Shared by Pastel Drum Synth and Pastel Drum Sampler (keep the
// two copies identical). The slot's six knobs are 0..1 values reached through
// get / set; the knobs underneath stay in sync through onEdit.
struct FxPicture : juce::Component {
    int type = -1;
    juce::Colour colour { 0xffe9a3b8 };
    std::function<float (int)> get;                    // knob k -> 0..1
    std::function<void (int, float)> set;              // knob k <- 0..1
    std::function<juce::String (int, float)> text;     // readout for knob k
    std::function<void()> onEdit;                      // after each drag step

    void paint (juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff16181c));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (juce::Colour (0xff24272c));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 8.f), r.getY() + 1, r.getBottom() - 1);
        for (int j = 1; j < 4; ++j) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) j / 4.f), r.getX() + 1, r.getRight() - 1);
        if (! isUsableFx (type) || ! get) return;
        const auto p = plot();
        juce::Path curve, fill;
        bool filled = true;
        switch (type) {
            case FX_EQ: {
                for (int i = 0; i <= 120; ++i) {
                    const float t = (float) i / 120.f;
                    const float y = yOfDb (eqDb (t));
                    if (i == 0) curve.startNewSubPath (p.getX(), y); else curve.lineTo (p.getX() + p.getWidth() * t, y);
                }
                break;
            }
            case FX_COMP: {
                const float thr = -48.f + get (0) * 48.f, ratio = 1.f + get (1) * 19.f;
                auto outDb = [&] (float in) { return in < thr ? in : thr + (in - thr) / ratio; };
                for (int i = 0; i <= 96; ++i) {
                    const float in = -60.f + 60.f * (float) i / 96.f;
                    const float x = p.getX() + p.getWidth() * (float) i / 96.f, y = p.getBottom() - p.getHeight() * (outDb (in) + 60.f) / 60.f;
                    if (i == 0) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
                }
                g.setColour (juce::Colour (0xff5c5a56));
                g.drawLine (p.getX(), p.getBottom(), p.getRight(), p.getY(), 1.f);   // 1:1
                break;
            }
            case FX_TRANSIENT: {
                const float att = get (0) * 2.f - 1.f, sus = get (1) * 2.f - 1.f;
                for (int i = 0; i <= 120; ++i) {
                    const float t = (float) i / 120.f;
                    float env = std::exp (-t * 4.f);
                    env *= 1.f + 0.9f * att * std::exp (-std::pow ((t - 0.05f) / 0.07f, 2.f));
                    env *= 1.f + 0.8f * sus * juce::jlimit (0.f, 1.f, (t - 0.12f) * 3.f);
                    const float y = p.getBottom() - p.getHeight() * 0.85f * juce::jlimit (0.f, 1.15f, env);
                    if (i == 0) curve.startNewSubPath (p.getX(), y); else curve.lineTo (p.getX() + p.getWidth() * t, y);
                }
                break;
            }
            case FX_DIST: {
                filled = false;
                for (int i = 0; i <= 120; ++i) {
                    const float x = (float) i / 60.f - 1.f;
                    const float y = p.getCentreY() - juce::jlimit (-1.f, 1.f, shape (x)) * p.getHeight() * 0.48f;
                    if (i == 0) curve.startNewSubPath (p.getX(), y); else curve.lineTo (p.getX() + p.getWidth() * (x + 1.f) * 0.5f, y);
                }
                g.setColour (juce::Colour (0xff5c5a56));
                g.drawLine (p.getX(), p.getBottom(), p.getRight(), p.getY(), 1.f);
                break;
            }
            case FX_REVERB: {
                const int n = 48;
                const float len = 0.25f + 0.73f * get (0);
                g.setColour (colour.withAlpha (0.85f));
                for (int i = 0; i < n; ++i) {
                    const float t = (float) i / (float) n;
                    if (t > len) break;
                    const float jag = 1.f + std::sin ((float) i * 7.1f) * 0.25f * (1.f - get (2));
                    const float h = p.getHeight() * std::exp (-t / len * 3.2f) * jag * (0.55f + 0.45f * get (1));
                    g.fillRoundedRectangle (p.getX() + p.getWidth() * t, p.getBottom() - juce::jmin (h, p.getHeight()),
                                            juce::jmax (1.5f, p.getWidth() / (float) n - 2.f), juce::jmin (h, p.getHeight()), 1.f);
                }
                filled = false;
                break;
            }
            case FX_DELAY: {
                const float gap = p.getWidth() * (0.06f + 0.3f * get (0));
                float h = p.getHeight();
                for (int i = 0; i < 16 && p.getX() + gap * (float) i < p.getRight() - 3.f; ++i) {
                    g.setColour (i == 0 ? juce::Colour (0xfff4f1ec) : colour.withAlpha (juce::jmax (0.3f, 1.f - (float) i * 0.07f)));
                    g.fillRoundedRectangle (p.getX() + gap * (float) i, p.getBottom() - h, 5.f, h, 1.5f);
                    h *= 0.2f + 0.75f * get (1);
                    if (h < 2.f) break;
                }
                filled = false;
                break;
            }
            case FX_FILTER: {
                for (int i = 0; i <= 160; ++i) {
                    const float t = (float) i / 160.f;
                    const float y = yOfDb (filterDb (t));
                    if (i == 0) curve.startNewSubPath (p.getX(), y); else curve.lineTo (p.getX() + p.getWidth() * t, y);
                }
                break;
            }
            default: break;
        }
        if (! curve.isEmpty()) {
            if (filled) {
                fill = curve;
                fill.lineTo (p.getRight(), p.getBottom()); fill.lineTo (p.getX(), p.getBottom()); fill.closeSubPath();
                g.setColour (colour.withAlpha (0.15f));
                g.fillPath (fill);
            }
            g.setColour (colour);
            g.strokePath (curve, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        for (auto& h : handles()) {
            g.setColour (colour.withAlpha (0.22f));
            g.fillEllipse (h.p.x - 8.f, h.p.y - 8.f, 16.f, 16.f);
            g.setColour (juce::Colour (0xfff4f1ec));
            g.fillEllipse (h.p.x - 5.f, h.p.y - 5.f, 10.f, 10.f);
            g.setColour (colour);
            g.drawEllipse (h.p.x - 5.f, h.p.y - 5.f, 10.f, 10.f, 2.f);
        }
        // readout: the knobs the picture moves
        juce::StringArray parts;
        for (int k : readoutKnobs())
            if (text) parts.add (juce::String (FxProcessor::knobLabel (type, k)) + " " + text (k, get (k)));
        const auto s = parts.joinIntoString (juce::String::fromUTF8 ("  \xc2\xb7  "));
        if (s.isNotEmpty()) {
            g.setFont (juce::Font (10.5f, juce::Font::bold));
            const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), s) + 16.f;
            auto rb = juce::Rectangle<float> (r.getRight() - w - 6.f, r.getY() + 6.f, w, 20.f);
            g.setColour (juce::Colour (0xe022252a));
            g.fillRoundedRectangle (rb, 4.f);
            g.setColour (juce::Colour (0xff3a3d43));
            g.drawRoundedRectangle (rb.reduced (0.5f), 4.f, 1.f);
            g.setColour (juce::Colour (0xffd9d5d0));
            g.drawText (s, rb, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override {
        active = -1;
        float best = 26.f;
        auto hs = handles();
        for (size_t i = 0; i < hs.size(); ++i)
            if (auto d = hs[i].p.getDistanceFrom (e.position); d < best) { best = d; active = (int) i; }
        startY = e.position.y;
        if (active >= 0 && hs[(size_t) active].ky >= 0) startV = get (hs[(size_t) active].ky);
    }
    void mouseDrag (const juce::MouseEvent& e) override {
        if (active < 0 || ! set) return;
        auto hs = handles();
        if (active >= (int) hs.size()) return;
        const auto h = hs[(size_t) active];
        const auto p = plot();
        const float fx = juce::jlimit (0.f, 1.f, (e.position.x - p.getX()) / p.getWidth());
        const float fy = juce::jlimit (0.f, 1.f, (p.getBottom() - e.position.y) / p.getHeight());   // 0 bottom .. 1 top
        if (h.kx >= 0) set (h.kx, xToValue (h.kx, fx));
        if (h.ky >= 0) set (h.ky, yToValue (h.ky, fy, e.position.y));
        repaint();
        if (onEdit) onEdit();
    }
    void mouseUp (const juce::MouseEvent&) override { active = -1; }

private:
    struct H { int kx, ky; juce::Point<float> p; };   // knob moved by x / y drag (-1 = none)
    int active = -1;
    float startY = 0.f, startV = 0.f;

    juce::Rectangle<float> plot() const { return getLocalBounds().toFloat().reduced (14.f, 12.f).withTrimmedTop (18.f); }
    float yOfDb (float db) const { const auto p = plot(); return juce::jlimit (p.getY(), p.getBottom(), p.getCentreY() - db / 18.f * p.getHeight() * 0.5f); }

    // EQ: four bands at fixed spots + tilt (gains match the knob text: v * 30 - 15 dB, tilt +-6 dB)
    static constexpr float eqX[4] = { 0.1f, 0.36f, 0.64f, 0.9f };
    float eqDb (float t) const {
        float db = (get (4) * 2.f - 1.f) * 6.f * (t - 0.5f) * 2.f;
        for (int b = 0; b < 4; ++b) {
            const float gdb = get (b) * 30.f - 15.f;
            const float d = (t - eqX[b]) / (b == 0 || b == 3 ? 0.22f : 0.12f);
            if (b == 0) db += gdb / (1.f + std::exp (d * 3.f));          // low shelf
            else if (b == 3) db += gdb / (1.f + std::exp (-d * 3.f));    // high shelf
            else db += gdb * std::exp (-d * d);                          // bells
        }
        return db;
    }
    // filter: cutoff 20 Hz * 900^v over the same log axis, LP / HP / BP / notch, 12 / 24 dB
    float filterDb (float t) const {
        const float fc = get (0), q = 0.6f + get (1) * 7.f;
        const int mode = juce::jlimit (0, 3, (int) (get (2) * 3.999f));
        const bool steep = get (3) >= 0.5f;
        const float x = std::pow (900.f, t - fc);   // f / fc on the log axis
        const float re = 1.f - x * x, im = x / q;
        const float den = std::sqrt (re * re + im * im);
        float mag = 1.f;
        switch (mode) {
            case 0: mag = 1.f / den; break;
            case 1: mag = x * x / den; break;
            case 2: mag = (x / q) / den; break;
            default: mag = std::abs (re) / den; break;
        }
        float db = 20.f * std::log10 (juce::jmax (1.0e-4f, mag));
        if (steep) db = juce::jmax (db * 2.f, -60.f);   // 24 dB: the 12 dB response squared
        return juce::jlimit (-60.f, 18.f, db);
    }
    float shape (float x) const {
        const float d = 1.f + get (0) * 9.f;
        const int algo = juce::jlimit (0, 7, (int) (get (2) * 7.999f));
        const float b = (get (4) * 2.f - 1.f) * 0.3f;
        switch (algo) {
            case 1: return juce::jlimit (-1.f, 1.f, x * d);                                  // hard
            case 2: return juce::jlimit (-1.f, 1.f, x * d * 2.f) * 0.9f;                     // fuzz
            case 3: return std::sin (x * d * 0.6f);                                          // fold
            case 4: return std::tanh ((x + b) * d) - std::tanh (b * d);                      // asym
            case 5: { const float s = 2.f + (1.f - get (0)) * 12.f; return std::round (x * s) / s; }   // bit
            case 6: return std::tanh (x * d) * (1.f - 0.3f * std::sin (x * 6.f));            // shape
            case 7: return std::abs (std::tanh (x * d)) * 2.f - 1.f;                         // rect
            default: return std::tanh (x * d) / std::tanh (d);                               // soft
        }
    }
    std::vector<int> readoutKnobs() const {
        switch (type) {
            case FX_EQ: return {};
            case FX_COMP: return { 0, 1 };
            case FX_TRANSIENT: return { 0, 1 };
            case FX_DIST: return { 0, 2 };
            case FX_REVERB: return { 0, 2 };
            case FX_DELAY: return { 0, 1 };
            case FX_FILTER: return { 0, 1 };
            default: return {};
        }
    }
    std::vector<H> handles() const {
        std::vector<H> hs;
        if (! isUsableFx (type) || ! get) return hs;
        const auto p = plot();
        auto at = [&] (float fx, float fy) { return juce::Point<float> (p.getX() + p.getWidth() * fx, p.getBottom() - p.getHeight() * fy); };
        switch (type) {
            case FX_EQ:
                for (int b = 0; b < 4; ++b) hs.push_back ({ -1, b, { p.getX() + p.getWidth() * eqX[b], yOfDb (eqDb (eqX[b])) } });
                break;
            case FX_COMP: {
                const float thr = -48.f + get (0) * 48.f, ratio = 1.f + get (1) * 19.f;
                hs.push_back ({ 0, -1, at ((thr + 60.f) / 60.f, (thr + 60.f) / 60.f) });   // the knee: threshold
                const float outEnd = thr + (0.f - thr) / ratio;
                hs.push_back ({ -1, 1, at (1.f, (outEnd + 60.f) / 60.f) });                // the top end: ratio
                break;
            }
            case FX_TRANSIENT:
                hs.push_back ({ -1, 0, at (0.06f, 0.15f + 0.7f * get (0)) });
                hs.push_back ({ -1, 1, at (0.55f, 0.15f + 0.7f * get (1)) });
                break;
            case FX_DIST: {
                const float x = 0.5f;
                hs.push_back ({ -1, 0, { p.getX() + p.getWidth() * (x + 1.f) * 0.5f,
                                         p.getCentreY() - juce::jlimit (-1.f, 1.f, shape (x)) * p.getHeight() * 0.48f } });
                break;
            }
            case FX_REVERB:
                hs.push_back ({ 0, 2, at (0.25f + 0.73f * get (0), 0.85f - 0.7f * get (2)) });
                break;
            case FX_DELAY:
                hs.push_back ({ 0, 1, { p.getX() + p.getWidth() * (0.06f + 0.3f * get (0)) + 2.5f,
                                        p.getBottom() - p.getHeight() * (0.2f + 0.75f * get (1)) } });
                break;
            case FX_FILTER:
                hs.push_back ({ 0, 1, { p.getX() + p.getWidth() * get (0), yOfDb (filterDb (get (0))) } });
                break;
            default: break;
        }
        return hs;
    }
    float xToValue (int k, float fx) const {
        switch (type) {
            case FX_COMP: return juce::jlimit (0.f, 1.f, (fx * 60.f - 60.f + 48.f) / 48.f);   // threshold
            case FX_REVERB: return juce::jlimit (0.f, 1.f, (fx - 0.25f) / 0.73f);
            case FX_DELAY: return juce::jlimit (0.f, 1.f, (fx - 0.06f) / 0.3f);
            default: juce::ignoreUnused (k); return fx;
        }
    }
    float yToValue (int k, float fy, float mouseY) const {
        const auto p = plot();
        switch (type) {
            case FX_EQ: {   // gain under the band: invert yOfDb
                const float db = (p.getCentreY() - mouseY) / (p.getHeight() * 0.5f) * 18.f;
                return juce::jlimit (0.f, 1.f, (db + 15.f) / 30.f);
            }
            case FX_COMP: {   // ratio from where the top end sits
                const float thr = -48.f + get (0) * 48.f;
                const float outEnd = juce::jlimit (thr + 0.01f, 0.f, fy * 60.f - 60.f);
                const float ratio = (0.f - thr) / juce::jmax (0.01f, outEnd - thr);
                return juce::jlimit (0.f, 1.f, (ratio - 1.f) / 19.f);
            }
            case FX_TRANSIENT: return juce::jlimit (0.f, 1.f, (fy - 0.15f) / 0.7f);
            case FX_DIST: return juce::jlimit (0.f, 1.f, startV + (startY - mouseY) / (p.getHeight() * 0.8f));
            case FX_REVERB: return juce::jlimit (0.f, 1.f, (0.85f - fy) / 0.7f);
            case FX_DELAY: return juce::jlimit (0.f, 1.f, (fy - 0.2f) / 0.75f);
            default: juce::ignoreUnused (k); return fy;
        }
    }
};

} // namespace mds
