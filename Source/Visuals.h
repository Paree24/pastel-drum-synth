#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>

// ============================================================
// Shared visual feedback (ported from Pastel Drum Synth): segmented
// output meter, oscilloscope and spectrum of a mono ring buffer.
// Self-contained on purpose: the audio side only needs a float ring
// of ScopeTap::Size samples plus an ever-increasing write counter.
// ============================================================

namespace mds {

// audio thread writes, UI reads (a torn frame only shows as one odd sample)
struct ScopeTap {
    static constexpr int Size = 2048;
    float buf[Size] = {};
    std::atomic<int> pos { 0 };
    void push (float v) { int p = pos.load (std::memory_order_relaxed); buf[p & (Size - 1)] = v; pos.store (p + 1, std::memory_order_release); }
    float at (int i) const { return buf[i & (Size - 1)]; }   // i may be any absolute counter value
};

// segmented level meter (green / amber / red), vertical or horizontal
struct MeterComp : juce::Component {
    explicit MeterComp (std::function<float()> src, bool vert = true) : level (std::move (src)), vertical (vert) {}
    void paint (juce::Graphics& g) override {
        auto mb = getLocalBounds().toFloat().reduced (vertical ? 2.f : 4.f, vertical ? 4.f : 2.f);
        g.setColour (juce::Colour (0xff16181c));
        g.fillRoundedRectangle (mb, 4.f);
        float m = juce::jlimit (0.f, 1.f, level ? level() : 0.f);
        const int segs = 14;
        float sz = (vertical ? mb.getHeight() : mb.getWidth()) / segs;
        for (int i = 0; i < segs; ++i) {
            bool lit = ((float) (i + 1) / segs) <= m;
            juce::Colour c = i < 9 ? juce::Colour (0xffa9d58e) : (i < 12 ? juce::Colour (0xffe8b86a) : juce::Colour (0xffe88a8a));
            g.setColour (lit ? c : juce::Colour (0xff2a2d33));
            if (vertical) g.fillRoundedRectangle (mb.getX(), mb.getBottom() - (i + 1) * sz + 1, mb.getWidth(), sz - 2, 2.f);
            else          g.fillRoundedRectangle (mb.getX() + i * sz + 1, mb.getY(), sz - 2, mb.getHeight(), 2.f);
        }
    }
    std::function<float()> level;
    bool vertical;
};

// v3 view well: dark rounded panel, faint grid, dot + title
inline void drawViewWell (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour dot, const juce::String& title) {
    g.setColour (juce::Colour (0xff16181c));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (juce::Colour (0xff24272c));
    for (int i = 1; i < 6; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 6.f), r.getY() + 22.f, r.getBottom() - 4.f);
    for (int j = 1; j < 4; ++j) g.drawHorizontalLine ((int) (r.getY() + 22.f + (r.getHeight() - 26.f) * j / 4.f), r.getX() + 4.f, r.getRight() - 4.f);
    g.setColour (juce::Colour (0xff202328));
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
    g.setColour (dot);
    g.fillEllipse (r.getX() + 10.f, r.getY() + 9.f, 6.f, 6.f);
    g.setColour (juce::Colour (0xff8f8b85));
    g.setFont (juce::Font (9.5f, juce::Font::bold));
    g.drawText (title, (int) r.getX() + 22, (int) r.getY() + 4, 260, 16, juce::Justification::centredLeft);
}

// where a scope / spectrum view reads from (re-evaluated every paint)
struct ScopeSource {
    const ScopeTap* tap = nullptr;
    juce::Colour colour { 0xffe9a3b8 };
    juce::String title;
    double sampleRate = 44100.0;
};

struct ScopeView : juce::Component {
    explicit ScopeView (std::function<ScopeSource()> s) : source (std::move (s)) {}
    void paint (juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        auto src = source ? source() : ScopeSource {};
        drawViewWell (g, r, src.colour, "SCOPE  " + src.title.toUpperCase());
        int pos = src.tap ? src.tap->pos.load (std::memory_order_acquire) : 0;
        if (pos < 8) {
            g.setColour (juce::Colour (0xff3a3d43));
            g.drawLine (r.getX() + 6, r.getCentreY(), r.getRight() - 6, r.getCentreY());
            return;
        }
        juce::Path path;
        const int N = ScopeTap::Size;
        for (int i = 0; i < N; i += 4) {
            float v = src.tap->at (pos - N + i);
            float x = r.getX() + (float) i / N * r.getWidth();
            float y = r.getCentreY() - juce::jlimit (-1.f, 1.f, v) * r.getHeight() * 0.42f;
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (src.colour);
        g.strokePath (path, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    std::function<ScopeSource()> source;
};

struct SpectrumView : juce::Component {
    explicit SpectrumView (std::function<ScopeSource()> s) : source (std::move (s)) {}
    void paint (juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat();
        auto src = source ? source() : ScopeSource {};
        drawViewWell (g, r, src.colour, "SPECTRUM");
        const int N = ScopeTap::Size;
        int pos = src.tap ? src.tap->pos.load (std::memory_order_acquire) : 0;
        if (pos < N) return; // need a full window
        for (int i = 0; i < N; ++i) {
            float w = 0.5f - 0.5f * std::cos (2.f * juce::MathConstants<float>::pi * i / N);
            work[i] = src.tap->at (pos - N + i) * w;
            work[i + N] = 0.f;
        }
        fft.performFrequencyOnlyForwardTransform (work);   // in place on 2*N floats
        const double sr = src.sampleRate > 0.0 ? src.sampleRate : 44100.0;
        // per-pixel magnitudes, then box-blur for a smooth line
        const int Wn = juce::jlimit (8, 512, (int) r.getWidth() / 2);
        float sm[512];
        for (int px = 0; px < Wn; ++px) {
            float f = 20.f * std::pow (1000.f, (float) px / Wn); // 20..20k log
            int bin = juce::jlimit (1, N / 2 - 1, (int) (f / sr * N));
            sm[px] = 20.f * std::log10 (work[(size_t) bin] / N + 1e-7f);
        }
        for (int k = 0; k < 2; ++k)
            for (int px = 0; px < Wn; ++px) {
                float a = sm[juce::jmax (0, px - 1)], b = sm[px], c = sm[juce::jmin (Wn - 1, px + 1)];
                sm[px] = (a + b * 2.f + c) * 0.25f;
            }
        float top = r.getY() + 20, bot = r.getBottom() - 16;
        juce::Path path;
        for (int px = 0; px < Wn; ++px) {
            float y = bot - juce::jlimit (0.f, 1.f, (sm[px] + 72.f) / 72.f) * (bot - top);
            float x = r.getX() + (float) px / Wn * r.getWidth();
            if (px == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        juce::Path fill (path);
        fill.lineTo (r.getRight(), bot); fill.lineTo (r.getX(), bot); fill.closeSubPath();
        g.setColour (src.colour.withAlpha (0.15f));
        g.fillPath (fill);
        g.setColour (src.colour);
        g.strokePath (path, juce::PathStrokeType (2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float marks[4] = { 100.f, 1000.f, 10000.f, 20000.f };
        const char* names[4] = { "100", "1k", "10k", "20k" };
        g.setFont (juce::Font (9.5f));
        for (int i = 0; i < 4; ++i) {
            float px = std::log (marks[i] / 20.f) / std::log (1000.f) * r.getWidth();
            float x = r.getX() + juce::jlimit (0.f, r.getWidth() - 30.f, px);
            g.setColour (juce::Colour (0xff2c2f35));
            g.drawLine (x, top, x, bot);
            g.setColour (juce::Colour (0xff5c5a56));
            g.drawText (names[i], (int) x + 2, (int) bot + 1, 30, 12, juce::Justification::left);
        }
    }
    std::function<ScopeSource()> source;
    juce::dsp::FFT fft { 11 };   // 2^11 = ScopeTap::Size
    float work[ScopeTap::Size * 2] = {};
};

} // namespace mds
