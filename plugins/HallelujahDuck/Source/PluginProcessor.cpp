#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace hbd;

static std::vector<hbui::FactoryPreset> duckPresets()
{
    return {
        { "Gentle Space (Default)", {} },
        { "Hook Space",   { { id::depth, 6.0f }, { id::sens, -40.0f }, { id::attack, 5.0f }, { id::release, 220.0f }, { id::low, 250.0f }, { id::high, 7000.0f } } },
        { "Heavy Carve",  { { id::depth, 10.0f }, { id::sens, -45.0f }, { id::attack, 3.0f }, { id::release, 120.0f }, { id::low, 180.0f }, { id::high, 8000.0f } } },
        { "Presence Only",{ { id::depth, 5.0f }, { id::low, 1500.0f }, { id::high, 6000.0f } } },
        { "Low-Mid Only", { { id::depth, 5.0f }, { id::low, 150.0f }, { id::high, 800.0f } } },
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout HallelujahDuckProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    auto skewed = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi); r.setSkewForCentre (centre); return r; };
    auto dbText = [] (float v, int) { return String (v, 1) + " dB"; };
    auto msText = [] (float v, int) { return String (roundToInt (v)) + " ms"; };
    auto hzText = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto pctText= [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto addF = [&] (const char* pid, const char* name, NormalisableRange<float> r, float def, std::function<String (float, int)> t)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, r, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (std::move (t))));
    };

    addF (id::depth,   "Depth",       { 0.0f, 18.0f, 0.1f }, 4.0f, dbText);
    addF (id::sens,    "Sensitivity", { -60.0f, -6.0f, 0.1f }, -35.0f, dbText);
    addF (id::attack,  "Attack",      skewed (1.0f, 80.0f, 10.0f), 8.0f, msText);
    addF (id::release, "Release",     skewed (30.0f, 800.0f, 160.0f), 160.0f, msText);
    addF (id::low,     "Low Limit",   skewed (100.0f, 1500.0f, 300.0f), 200.0f, hzText);
    addF (id::high,    "High Limit",  skewed (800.0f, 10000.0f, 4000.0f), 6000.0f, hzText);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id::stereo, 1 }, "Stereo Mode", StringArray { "Stereo", "Mid Only" }, 1));
    addF (id::mix,     "Mix",         { 0.0f, 100.0f, 0.1f }, 100.0f, pctText);
    addF (id::out,     "Output",      { -12.0f, 6.0f, 0.1f }, 0.0f, dbText);
    return layout;
}

HallelujahDuckProcessor::HallelujahDuckProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout()),
      presets (apvts, "Hallelujah Duck", duckPresets())
{
    pDepth = apvts.getRawParameterValue (id::depth);
    pSens = apvts.getRawParameterValue (id::sens);
    pAttack = apvts.getRawParameterValue (id::attack);
    pRelease = apvts.getRawParameterValue (id::release);
    pLow = apvts.getRawParameterValue (id::low);
    pHigh = apvts.getRawParameterValue (id::high);
    pStereo = apvts.getRawParameterValue (id::stereo);
    pMix = apvts.getRawParameterValue (id::mix);
    pOut = apvts.getRawParameterValue (id::out);
    presets.setCurrentName ("Gentle Space (Default)");
}

bool HallelujahDuckProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void HallelujahDuckProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (0);
}

void HallelujahDuckProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    hb::DuckParams p;
    p.depthDb       = pDepth->load();
    p.sensitivityDb = pSens->load();
    p.attackMs      = pAttack->load();
    p.releaseMs     = pRelease->load();
    p.lowHz         = pLow->load();
    p.highHz        = pHigh->load();
    p.midOnly       = pStereo->load() > 0.5f;
    p.mix           = pMix->load() * 0.01f;
    p.outputGainDb  = pOut->load();
    engine.setParams (p);

    auto main = getBusBuffer (buffer, true, 0);
    const int n = buffer.getNumSamples();
    if (main.getNumChannels() == 0 || n == 0)
        return;

    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1)
    {
        auto sc = getBusBuffer (buffer, true, 1);
        if (sc.getNumChannels() > 0) scL = sc.getReadPointer (0);
        if (sc.getNumChannels() > 1) scR = sc.getReadPointer (1);
    }
    sidechainConnected.store (scL != nullptr, std::memory_order_relaxed);

    float* l = main.getWritePointer (0);
    float* r = main.getNumChannels() > 1 ? main.getWritePointer (1) : nullptr;
    engine.process (l, r, scL, scR, n);
}

juce::AudioProcessorEditor* HallelujahDuckProcessor::createEditor()
{
    return new HallelujahDuckEditor (*this);
}

void HallelujahDuckProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
    {
        xml->setAttribute ("presetName", presets.getCurrentName());
        copyXmlToBinary (*xml, destData);
    }
}

void HallelujahDuckProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            presets.setCurrentName (xml->getStringAttribute ("presetName"));
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HallelujahDuckProcessor();
}
