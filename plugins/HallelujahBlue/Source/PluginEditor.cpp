#include "PluginEditor.h"

using namespace hbp;

HallelujahBlueEditor::HallelujahBlueEditor (HallelujahBlueProcessor& p)
    : AudioProcessorEditor (&p), proc (p), presetBar (p.presets), analyzer (p.inputFifo, p.outputFifo)
{
    setLookAndFeel (&lnf);
    auto& s = proc.apvts;

    // ---- sections: (parameter, label, advanced-only) ------------------------------
    micSection.add (s, id::micProfile, "Your Mic");
    micSection.add (s, id::micModel,   "Mic Model", true);
    micSection.add (s, id::inTrim,     "Input");
    micSection.add (s, id::proximity,  "Proximity", true);
    micSection.add (s, id::air,        "Air");
    micSection.add (s, id::presence,   "Presence");
    micSection.add (s, id::warmth,     "Warmth");

    roomSection.add (s, id::gateOn,      "Noise Gate", true);
    roomSection.add (s, id::gateThresh,  "Gate");
    roomSection.add (s, id::gateRelease, "Release", true);
    roomSection.add (s, id::gateRange,   "Range", true);
    roomSection.add (s, id::deessAmount, "De-Ess");
    roomSection.add (s, id::deessFreq,   "S Freq", true);
    roomSection.add (s, id::plosive,     "Plosive");

    preSection.add (s, id::preFlavor, "Flavor");
    preSection.add (s, id::preDrive,  "Drive");

    eqSection.add (s, id::eqHpf,        "High-Pass");
    eqSection.add (s, id::eqLowMidFreq, "Box Freq", true);
    eqSection.add (s, id::eqLowMidGain, "Box Cut");
    eqSection.add (s, id::eqPresFreq,   "Pres Freq", true);
    eqSection.add (s, id::eqPresGain,   "Presence");
    eqSection.add (s, id::eqAirGain,    "Air");

    compSection.add (s, id::compMode,    "Mode");
    compSection.add (s, id::compManual,  "Manual Controls", true);
    compSection.add (s, id::compAmount,  "Amount");
    compSection.add (s, id::compRatio,   "Ratio", true);
    compSection.add (s, id::compAttack,  "Attack", true);
    compSection.add (s, id::compRelease, "Release", true);
    compSection.add (s, id::compMakeup,  "Makeup", true);

    fxSection.add (s, id::dlySync,      "Delay Time", true);
    fxSection.add (s, id::dblMix,       "Doubler");
    fxSection.add (s, id::verbMix,      "Reverb");
    fxSection.add (s, id::verbDecay,    "Decay", true);
    fxSection.add (s, id::verbPredelay, "Pre-Delay", true);
    fxSection.add (s, id::dlyMix,       "Delay");
    fxSection.add (s, id::dlyFeedback,  "Feedback", true);
    fxSection.add (s, id::fxWidth,      "Width");

    outSection.add (s, id::sitMix,     "Sit in the Mix");
    outSection.add (s, id::outGain,    "Output");
    outSection.add (s, id::outCeiling, "Ceiling", true);

    // one-click rap EQ presets
    for (auto* b : { &eqDark, &eqBright, &eqMelodic })
        eqButtons.addAndMakeVisible (b);
    eqDark.setTooltip ("Dark and Heavy");
    eqBright.setTooltip ("Bright and Crisp");
    eqMelodic.setTooltip ("Melodic");
    eqDark.onClick    = [this] { applyEqPreset (0); };
    eqBright.onClick  = [this] { applyEqPreset (1); };
    eqMelodic.onClick = [this] { applyEqPreset (2); };
    eqSection.setFooter (&eqButtons, 24);

    for (auto* sec : sections) addAndMakeVisible (sec);

    // ---- header ---------------------------------------------------------------------
    addAndMakeVisible (presetBar);
    addAndMakeVisible (viewButton);
    viewButton.setTooltip ("Switch between the simple big-knob view and full control");
    viewButton.onClick = [this] { setAdvanced (! proc.advancedView); };
    qualityControl = std::make_unique<hbui::ParamControl> (s, id::quality, "Mode");
    addAndMakeVisible (*qualityControl);

    // ---- analyzer + meters ------------------------------------------------------------
    addAndMakeVisible (analyzer);
    analyzer.eqCurve = [this] (double f)
    {
        return hb::VocalChain::responseDb (currentCurve, proc.getCurrentSampleRate(), f);
    };
    for (auto* m : { &inMeter, &grMeter, &outMeter })
        addAndMakeVisible (m);
    addAndMakeVisible (dynamicsReadout);
    dynamicsReadout.setFont (hbui::font (11.0f));
    dynamicsReadout.setColour (juce::Label::textColourId, hbui::colours::textDim);
    dynamicsReadout.setJustificationType (juce::Justification::topLeft);

    setAdvanced (proc.advancedView);
    setSize (1180, 720);
    startTimerHz (30);
}

HallelujahBlueEditor::~HallelujahBlueEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HallelujahBlueEditor::setAdvanced (bool advanced)
{
    proc.advancedView = advanced;
    viewButton.setButtonText (advanced ? "Advanced" : "Simple");
    viewButton.setToggleState (advanced, juce::dontSendNotification);
    for (auto* sec : sections) sec->setAdvanced (advanced);
}

void HallelujahBlueEditor::applyEqPreset (int which)
{
    for (auto& [pid, value] : eqPreset (which))
        if (auto* param = proc.apvts.getParameter (pid))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (value));
            param->endChangeGesture();
        }
}

void HallelujahBlueEditor::timerCallback()
{
    auto& m = proc.chain.meters;
    inMeter.update (m.inputPeak.exchange (0.0f));
    outMeter.update (m.outputPeak.exchange (0.0f));
    grMeter.update (m.compGr.load());

    dynamicsReadout.setText ("De-ess  -" + juce::String (m.deEssGr.load(), 1) + " dB\n"
                             "Gate    -" + juce::String (m.gateGr.load(), 1) + " dB\n"
                             "Plosive -" + juce::String (m.plosiveGr.load(), 1) + " dB\n"
                             "Limiter -" + juce::String (m.limiterGr.load(), 1) + " dB\n"
                             "Latency  " + juce::String (proc.getLatencySamples()) + " smp",
                             juce::dontSendNotification);

    currentCurve = hb::VocalChain::designStaticFilters (proc.readParamsForDisplay(), proc.getCurrentSampleRate());
    analyzer.update (proc.getCurrentSampleRate());

    if (presetBar.isShowing() && ++presetRefreshCounter >= 30)   // pick up host-side state loads
    {
        presetRefreshCounter = 0;
        if (proc.presets.getCurrentName() != lastPresetName)
        {
            lastPresetName = proc.presets.getCurrentName();
            presetBar.refresh();
        }
    }
}

void HallelujahBlueEditor::paint (juce::Graphics& g)
{
    using namespace hbui;
    g.fillAll (colours::background);

    // header band
    auto header = getLocalBounds().removeFromTop (56).toFloat();
    g.setGradientFill (juce::ColourGradient (colours::panel, 0, 0, colours::background, 0, header.getBottom(), false));
    g.fillRect (header);
    g.setColour (colours::accent);
    g.fillRect (header.removeFromBottom (2.0f));

    juce::AttributedString title;
    title.append ("DETROIT HALLELUJAH ", font (22.0f, true), colours::text);
    title.append ("BLUE", font (22.0f, true), colours::accent);
    title.setJustification (juce::Justification::centredLeft);
    title.draw (g, { 18.0f, 6.0f, 320.0f, 28.0f });
    g.setColour (colours::textDim);
    g.setFont (font (11.0f));
    g.drawText ("VOCAL CHANNEL", 20, 32, 300, 16, juce::Justification::centredLeft);
}

void HallelujahBlueEditor::resized()
{
    auto r = getLocalBounds();

    auto header = r.removeFromTop (56).reduced (12, 12);
    header.removeFromLeft (330);
    qualityControl->setBounds (header.removeFromRight (190).withTrimmedTop (-12).withHeight (44));
    header.removeFromRight (8);
    viewButton.setBounds (header.removeFromRight (90).reduced (0, 2));
    header.removeFromRight (12);
    presetBar.setBounds (header);

    r.reduce (10, 8);
    auto top = r.removeFromTop (250);
    auto meterArea = top.removeFromRight (250);
    top.removeFromRight (8);
    analyzer.setBounds (top);

    auto meters = meterArea.removeFromLeft (150);
    const int mw = meters.getWidth() / 3;
    inMeter.setBounds (meters.removeFromLeft (mw).reduced (3, 0));
    grMeter.setBounds (meters.removeFromLeft (mw).reduced (3, 0));
    outMeter.setBounds (meters.reduced (3, 0));
    dynamicsReadout.setBounds (meterArea.reduced (8, 18));

    r.removeFromTop (8);
    const float weights[] = { 1.25f, 1.15f, 0.75f, 1.25f, 1.15f, 1.3f, 0.85f };
    float total = 0; for (float w : weights) total += w;
    const int width = r.getWidth();
    for (size_t i = 0; i < sections.size(); ++i)
        sections[i]->setBounds (r.removeFromLeft (i + 1 == sections.size() ? r.getWidth()
                                                  : juce::roundToInt ((float) width * weights[i] / total)));
}
