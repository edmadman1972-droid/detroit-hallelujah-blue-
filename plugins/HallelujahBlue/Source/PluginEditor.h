// Detroit Hallelujah Blue - editor
#pragma once

#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "ParamControl.h"
#include "PresetBar.h"

class HallelujahBlueEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HallelujahBlueEditor (HallelujahBlueProcessor&);
    ~HallelujahBlueEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void setAdvanced (bool);
    void applyEqPreset (int which);

    HallelujahBlueProcessor& proc;
    hbui::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };

    hbui::PresetBar presetBar;
    juce::TextButton viewButton { "Simple" };
    std::unique_ptr<hbui::ParamControl> qualityControl;

    SpectrumAnalyzer analyzer;
    hbui::LevelMeter inMeter { "IN" }, grMeter { "COMP", hbui::LevelMeter::Style::GainReduction }, outMeter { "OUT" };
    juce::Label dynamicsReadout;

    hbui::Section micSection { "MIC  -  C800G-STYLE" }, roomSection { "ROOM CLEANUP" }, preSection { "PREAMP" },
                  eqSection { "EQ" }, compSection { "COMPRESSOR" }, fxSection { "VOCAL FX" }, outSection { "MIX + OUTPUT" };

    hbui::ButtonRow eqButtons;
    juce::TextButton eqDark { "Dark" }, eqBright { "Bright" }, eqMelodic { "Melodic" };

    std::array<hb::BiquadCoeffs, hb::VocalChain::kNumStaticFilters> currentCurve {};
    juce::String lastPresetName;
    int presetRefreshCounter = 0;

    std::array<hbui::Section*, 7> sections { &micSection, &roomSection, &preSection, &eqSection, &compSection, &fxSection, &outSection };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallelujahBlueEditor)
};
