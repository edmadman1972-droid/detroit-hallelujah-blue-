// Level and gain-reduction meters with peak hold
#pragma once

#include "HBLookAndFeel.h"
#include "HBMath.h"

namespace hbui
{
class LevelMeter : public juce::Component
{
public:
    enum class Style { Level, GainReduction };

    LevelMeter (juce::String labelText, Style s = Style::Level) : label (std::move (labelText)), style (s) {}

    /** Call from the editor's timer. Level: peak in linear gain. GR: reduction in dB (positive). */
    void update (float value)
    {
        if (style == Style::Level)
        {
            const float db = hb::gainToDb (value, -90.0f);
            displayDb = db > displayDb ? db : juce::jmax (db, displayDb - 1.5f);   // ~45 dB/s fall
            if (db >= holdDb) { holdDb = db; holdFrames = 45; }
            else if (--holdFrames <= 0) holdDb = juce::jmax (displayDb, holdDb - 1.0f);
            if (db > -0.1f) clipFrames = 60;
            else if (clipFrames > 0) --clipFrames;
        }
        else
        {
            displayDb = value > displayDb ? value : juce::jmax (value, displayDb - 0.8f);
            if (value >= holdDb) { holdDb = value; holdFrames = 45; }
            else if (--holdFrames <= 0) holdDb = juce::jmax (displayDb, holdDb - 0.5f);
        }
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { holdDb = style == Style::Level ? -90.0f : 0.0f; clipFrames = 0; repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto r = getLocalBounds().toFloat();
        g.setColour (colours::textDim);
        g.setFont (font (11.0f, true));
        g.drawText (label, r.removeFromBottom (16.0f), Justification::centred);

        auto readout = r.removeFromTop (16.0f);
        auto bar = r.reduced (3.0f, 2.0f);
        g.setColour (colours::background);
        g.fillRoundedRectangle (bar, 3.0f);

        if (style == Style::Level)
        {
            auto toY = [&] (float db) { return jmap (jlimit (-60.0f, 0.0f, db), -60.0f, 0.0f, bar.getBottom(), bar.getY()); };
            const float y = toY (displayDb);
            ColourGradient grad (colours::meterGreen, 0, bar.getBottom(), colours::meterRed, 0, bar.getY(), false);
            grad.addColour (0.75, colours::meterYellow);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withTop (y), 3.0f);
            g.setColour (colours::text);
            g.fillRect (bar.getX(), toY (holdDb) - 1.0f, bar.getWidth(), 2.0f);
            if (clipFrames > 0)
            {
                g.setColour (colours::meterRed);
                g.fillRoundedRectangle (bar.withHeight (5.0f), 2.0f);
            }
            g.setColour (clipFrames > 0 ? colours::meterRed : colours::text);
            g.setFont (font (11.0f));
            g.drawText (holdDb <= -89.0f ? String ("-inf") : String (holdDb, 1), readout, Justification::centred);
        }
        else
        {
            auto toY = [&] (float db) { return jmap (jlimit (0.0f, 24.0f, db), 0.0f, 24.0f, bar.getY(), bar.getBottom()); };
            g.setColour (colours::gold);
            g.fillRoundedRectangle (bar.withBottom (toY (displayDb)), 3.0f);
            g.setColour (colours::text);
            g.fillRect (bar.getX(), toY (holdDb) - 1.0f, bar.getWidth(), 2.0f);
            g.setFont (font (11.0f));
            g.drawText ("-" + String (holdDb, 1), readout, Justification::centred);
        }

        // scale ticks
        g.setColour (colours::panelEdge);
        for (int i = 1; i < 6; ++i)
        {
            const float ty = bar.getY() + bar.getHeight() * (float) i / 6.0f;
            g.drawHorizontalLine ((int) ty, bar.getX(), bar.getX() + 3.0f);
        }
    }

private:
    juce::String label;
    Style style;
    float displayDb = -90.0f, holdDb = -90.0f;
    int holdFrames = 0, clipFrames = 0;
};

/** Lock-free single-producer ring the audio thread writes and the analyzer reads. */
class AnalyzerFifo
{
public:
    static constexpr int kSize = 16384;   // power of two

    void push (const float* data, int n) noexcept
    {
        int w = writePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            buffer[(size_t) w] = data[i];
            w = (w + 1) & (kSize - 1);
        }
        writePos.store (w, std::memory_order_release);
    }

    /** Copies the newest n samples (n <= kSize). Minor tearing is harmless for display. */
    void copyLatest (float* dest, int n) const noexcept
    {
        const int w = writePos.load (std::memory_order_acquire);
        int r = (w - n) & (kSize - 1);
        for (int i = 0; i < n; ++i)
        {
            dest[i] = buffer[(size_t) r];
            r = (r + 1) & (kSize - 1);
        }
    }

private:
    std::array<float, kSize> buffer {};
    std::atomic<int> writePos { 0 };
};

} // namespace hbui
