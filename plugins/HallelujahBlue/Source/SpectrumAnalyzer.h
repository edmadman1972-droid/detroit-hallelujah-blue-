// Real-time spectrum analyzer with EQ curve overlay and freeze
#pragma once

#include <juce_dsp/juce_dsp.h>
#include "Meters.h"

class SpectrumAnalyzer : public juce::Component
{
public:
    static constexpr int kOrder = 12, kSize = 1 << kOrder;

    SpectrumAnalyzer (hbui::AnalyzerFifo& inFifo, hbui::AnalyzerFifo& outFifo)
        : inputFifo (inFifo), outputFifo (outFifo)
    {
        inDb.fill (-100.0f);
        outDb.fill (-100.0f);
        addAndMakeVisible (freezeButton);
        addAndMakeVisible (inputButton);
        freezeButton.setClickingTogglesState (true);
        inputButton.setClickingTogglesState (true);
        inputButton.setToggleState (true, juce::dontSendNotification);
        freezeButton.setTooltip ("Hold the display while you adjust");
        inputButton.setTooltip ("Show the raw input spectrum behind the processed one");
    }

    /** freq (Hz) -> dB of the static EQ chain; set by the editor. */
    std::function<float (double)> eqCurve;

    void update (double sampleRate)
    {
        sr = sampleRate;
        if (! freezeButton.getToggleState())
        {
            analyse (outputFifo, outDb);
            if (inputButton.getToggleState()) analyse (inputFifo, inDb);
        }
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8).removeFromTop (22);
        freezeButton.setBounds (r.removeFromRight (64));
        r.removeFromRight (6);
        inputButton.setBounds (r.removeFromRight (64));
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        using namespace hbui;
        auto bounds = getLocalBounds().toFloat();
        drawPanel (g, bounds, {});
        auto plot = bounds.reduced (10.0f, 8.0f).withTrimmedBottom (14.0f).withTrimmedRight (26.0f);

        const float minF = 20.0f, maxF = 20000.0f;
        auto fToX = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / minF) / std::log (maxF / minF)); };
        auto xToF = [&] (float x) { return minF * std::pow (maxF / minF, (x - plot.getX()) / plot.getWidth()); };
        auto dbToY = [&] (float db) { return jmap (jlimit (-90.0f, 0.0f, db), -90.0f, 0.0f, plot.getBottom(), plot.getY()); };
        auto eqToY = [&] (float db) { return jmap (jlimit (-18.0f, 18.0f, db), -18.0f, 18.0f, plot.getBottom(), plot.getY()); };

        // grid
        g.setFont (font (10.0f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = fToX (f);
            g.setColour (colours::panelEdge.withAlpha (0.7f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            g.setColour (colours::textDim);
            g.drawText (f >= 1000.0 ? String ((int) (f / 1000.0)) + "k" : String ((int) f),
                        Rectangle<float> (x - 20.0f, plot.getBottom() + 1.0f, 40.0f, 13.0f), Justification::centred);
        }
        for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            const float y = eqToY (db);
            g.setColour (db == 0.0f ? colours::panelEdge.brighter (0.3f) : colours::panelEdge.withAlpha (0.5f));
            g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            g.setColour (colours::gold.withAlpha (0.7f));
            g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (plot.getRight() + 2.0f, y - 6.0f, 24.0f, 12.0f), Justification::centredLeft);
        }

        auto spectrumPath = [&] (const std::array<float, kSize / 2>& db)
        {
            Path p;
            const double binHz = sr / kSize;
            bool started = false;
            for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
            {
                const double f0 = xToF (x), f1 = xToF (x + 2.0f);
                int b0 = jlimit (1, kSize / 2 - 1, (int) (f0 / binHz));
                int b1 = jlimit (b0, kSize / 2 - 1, (int) (f1 / binHz));
                float v = -100.0f;
                for (int b = b0; b <= b1; ++b) v = jmax (v, db[(size_t) b]);
                const float y = dbToY (v);
                if (! started) { p.startNewSubPath (x, y); started = true; }
                else p.lineTo (x, y);
            }
            return p;
        };

        if (inputButton.getToggleState())
        {
            g.setColour (colours::textDim.withAlpha (0.45f));
            g.strokePath (spectrumPath (inDb), PathStrokeType (1.0f));
        }

        auto out = spectrumPath (outDb);
        auto fill = out;
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (colours::accent.withAlpha (0.45f), 0, plot.getY(),
                                           colours::accent.withAlpha (0.03f), 0, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colours::accentBright);
        g.strokePath (out, PathStrokeType (1.5f));

        // EQ curve overlay
        if (eqCurve)
        {
            Path eq;
            for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
            {
                const float y = eqToY (eqCurve (xToF (x)));
                if (x == plot.getX()) eq.startNewSubPath (x, y); else eq.lineTo (x, y);
            }
            g.setColour (colours::gold);
            g.strokePath (eq, PathStrokeType (2.0f));
        }

        g.setColour (colours::textDim);
        g.setFont (font (11.0f, true));
        g.drawText ("SPECTRUM  +  EQ CURVE", plot.withHeight (16.0f).translated (4.0f, 2.0f), Justification::centredLeft);
        if (freezeButton.getToggleState())
        {
            g.setColour (colours::gold);
            g.drawText ("FROZEN", plot.withHeight (16.0f).translated (0.0f, 18.0f).withTrimmedRight (6.0f), Justification::centredRight);
        }
    }

private:
    void analyse (hbui::AnalyzerFifo& fifo, std::array<float, kSize / 2>& db)
    {
        fifo.copyLatest (fftData.data(), kSize);
        std::fill (fftData.begin() + kSize, fftData.end(), 0.0f);
        window.multiplyWithWindowingTable (fftData.data(), (size_t) kSize);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const float norm = 4.0f / (float) kSize;   // Hann window: sine of amplitude A -> A
        for (size_t b = 0; b < (size_t) kSize / 2; ++b)
        {
            const float level = juce::Decibels::gainToDecibels (fftData[b] * norm, -100.0f);
            db[b] = level > db[b] ? level : juce::jmax (level, db[b] - 2.5f);   // smooth fall
        }
    }

    hbui::AnalyzerFifo& inputFifo;
    hbui::AnalyzerFifo& outputFifo;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::array<float, kSize * 2> fftData {};
    std::array<float, kSize / 2> inDb {}, outDb {};
    double sr = 48000.0;
    juce::TextButton freezeButton { "Freeze" }, inputButton { "Input" };
};
