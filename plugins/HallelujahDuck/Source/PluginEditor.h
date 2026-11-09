// Hallelujah Duck - editor
#pragma once

#include "PluginProcessor.h"
#include "ParamControl.h"
#include "PresetBar.h"
#include "Meters.h"

/** Shows how far each band of the beat is being dipped right now. */
class DuckDisplay : public juce::Component
{
public:
    explicit DuckDisplay (HallelujahDuckProcessor& p) : proc (p) {}

    void update()
    {
        for (size_t b = 0; b < shown.size(); ++b)
        {
            const float v = proc.engine.bandReductionDb[b].load (std::memory_order_relaxed);
            shown[b] = v > shown[b] ? v : juce::jmax (v, shown[b] - 0.6f);
        }
        const float sc = proc.engine.sidechainPeak.exchange (0.0f);
        scDb = juce::jmax (hb::gainToDb (sc, -90.0f), scDb - 1.5f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        using namespace hbui;
        auto r = getLocalBounds().toFloat();
        drawPanel (g, r, "WHERE THE BEAT IS MAKING ROOM");
        r = r.reduced (12.0f).withTrimmedTop (20.0f);

        auto scArea = r.removeFromRight (70.0f);
        auto labels = r.removeFromBottom (16.0f);
        const float depthMax = jmax (6.0f, proc.apvts.getRawParameterValue (hbd::id::depth)->load());
        const float lo = proc.apvts.getRawParameterValue (hbd::id::low)->load();
        const float hi = proc.apvts.getRawParameterValue (hbd::id::high)->load();

        const float bw = r.getWidth() / (float) shown.size();
        for (size_t b = 0; b < shown.size(); ++b)
        {
            auto col = Rectangle<float> (r.getX() + bw * (float) b, r.getY(), bw, r.getHeight()).reduced (5.0f, 0.0f);
            const float weight = hb::DuckEngine::bandWeight (hb::DuckEngine::bandFreqs[b], lo, hi);
            g.setColour (colours::background);
            g.fillRoundedRectangle (col, 4.0f);
            if (weight > 0.0f)
            {
                g.setColour (colours::panelEdge.withAlpha (0.4f + 0.6f * weight));
                g.drawRoundedRectangle (col, 4.0f, 1.0f);
            }
            const float h = col.getHeight() * jlimit (0.0f, 1.0f, shown[b] / depthMax);
            g.setColour (colours::accent.interpolatedWith (colours::gold, jlimit (0.0f, 1.0f, shown[b] / 12.0f)));
            g.fillRoundedRectangle (col.withHeight (h), 4.0f);

            const float f = hb::DuckEngine::bandFreqs[b];
            g.setColour (weight > 0.0f ? colours::text : colours::textDim);
            g.setFont (font (11.0f));
            g.drawText (f >= 1000.0f ? String (f / 1000.0f, 1) + "k" : String ((int) f),
                        Rectangle<float> (col.getX() - 4.0f, labels.getY(), col.getWidth() + 8.0f, 16.0f), Justification::centred);
            g.drawText ("-" + String (shown[b], 1), col.withHeight (16.0f).translated (0.0f, h > 18.0f ? h - 17.0f : h + 2.0f),
                        Justification::centred);
        }

        // vocal sidechain level
        auto bar = scArea.reduced (18.0f, 0.0f).withTrimmedBottom (16.0f);
        g.setColour (colours::background);
        g.fillRoundedRectangle (bar, 4.0f);
        const float y = jmap (jlimit (-60.0f, 0.0f, scDb), -60.0f, 0.0f, bar.getBottom(), bar.getY());
        g.setColour (colours::meterGreen);
        g.fillRoundedRectangle (bar.withTop (y), 4.0f);
        g.setColour (colours::textDim);
        g.setFont (font (10.5f, true));
        g.drawText ("VOCAL", scArea.removeFromBottom (16.0f), Justification::centred);

        if (! proc.sidechainConnected.load() || scDb < -80.0f)
        {
            g.setColour (colours::gold);
            g.setFont (font (13.0f, true));
            g.drawText (proc.sidechainConnected.load() ? "Waiting for vocal on the sidechain..."
                                                       : "Route your vocal track to this plug-in's sidechain input",
                        r.withHeight (22.0f).translated (0.0f, r.getHeight() * 0.4f), Justification::centred);
        }
    }

private:
    HallelujahDuckProcessor& proc;
    std::array<float, hb::DuckEngine::kBands> shown {};
    float scDb = -90.0f;
};

class HallelujahDuckEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HallelujahDuckEditor (HallelujahDuckProcessor& p)
        : AudioProcessorEditor (&p), proc (p), presetBar (p.presets), display (p)
    {
        setLookAndFeel (&lnf);
        using namespace hbd;
        auto& s = proc.apvts;
        duckSection.add (s, id::depth, "Depth");
        duckSection.add (s, id::sens, "Sensitivity");
        timeSection.add (s, id::attack, "Attack");
        timeSection.add (s, id::release, "Release");
        rangeSection.add (s, id::low, "Low Limit");
        rangeSection.add (s, id::high, "High Limit");
        outSection.add (s, id::stereo, "Stereo");
        outSection.add (s, id::mix, "Mix");
        outSection.add (s, id::out, "Output");
        for (auto* sec : { &duckSection, &timeSection, &rangeSection, &outSection })
        {
            sec->setAdvanced (true);
            addAndMakeVisible (sec);
        }
        addAndMakeVisible (presetBar);
        addAndMakeVisible (display);
        setSize (820, 520);
        startTimerHz (30);
    }

    ~HallelujahDuckEditor() override
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace hbui;
        g.fillAll (colours::background);
        auto header = getLocalBounds().removeFromTop (56).toFloat();
        g.setGradientFill (juce::ColourGradient (colours::panel, 0, 0, colours::background, 0, header.getBottom(), false));
        g.fillRect (header);
        g.setColour (colours::accent);
        g.fillRect (header.removeFromBottom (2.0f));
        juce::AttributedString title;
        title.append ("HALLELUJAH ", font (22.0f, true), colours::text);
        title.append ("DUCK", font (22.0f, true), colours::accent);
        title.setJustification (juce::Justification::centredLeft);
        title.draw (g, { 18.0f, 6.0f, 260.0f, 28.0f });
        g.setColour (colours::textDim);
        g.setFont (font (11.0f));
        g.drawText ("BEAT BUS  -  VOCAL SIDECHAIN", 20, 32, 260, 16, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (56).reduced (12, 12);
        header.removeFromLeft (270);
        presetBar.setBounds (header);

        r.reduce (10, 8);
        display.setBounds (r.removeFromTop (220));
        r.removeFromTop (8);
        const int w = r.getWidth() / 4;
        duckSection.setBounds (r.removeFromLeft (w));
        timeSection.setBounds (r.removeFromLeft (w));
        rangeSection.setBounds (r.removeFromLeft (w));
        outSection.setBounds (r);
    }

private:
    void timerCallback() override
    {
        display.update();
        if (++refreshCounter >= 30)
        {
            refreshCounter = 0;
            if (proc.presets.getCurrentName() != lastPresetName)
            {
                lastPresetName = proc.presets.getCurrentName();
                presetBar.refresh();
            }
        }
    }

    HallelujahDuckProcessor& proc;
    hbui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    hbui::PresetBar presetBar;
    DuckDisplay display;
    hbui::Section duckSection { "DUCKING" }, timeSection { "TIMING" }, rangeSection { "FREQUENCY RANGE" }, outSection { "OUTPUT" };
    juce::String lastPresetName;
    int refreshCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallelujahDuckEditor)
};
