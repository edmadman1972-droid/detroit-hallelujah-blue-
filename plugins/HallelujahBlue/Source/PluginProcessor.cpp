#include "PluginProcessor.h"
#include "PluginEditor.h"

HallelujahBlueProcessor::HallelujahBlueProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::mono(),   true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", hbp::createLayout()),
      presets (apvts, "Hallelujah Blue", hbp::factoryPresets())
{
    // fill both caches on the message thread so the audio thread never allocates
    reader.read();
    displayReader.read();
    presets.setCurrentName ("C800G-style (Default)");
}

bool HallelujahBlueProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk  = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    // mono -> mono, mono -> stereo, stereo -> stereo
    return inOk && outOk && in.size() <= out.size();
}

void HallelujahBlueProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store (sampleRate);
    const int block = juce::jmax (1, samplesPerBlock);
    monoBuffer.setSize (1, block, false, true, true);
    rightBuffer.setSize (1, block, false, true, true);

    chain.prepare (sampleRate, block);
    chain.setParams (reader.read(), 120.0);
    chain.reset();
    setLatencySamples (chain.getLatency());
}

void HallelujahBlueProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int total  = buffer.getNumSamples();
    if (total == 0 || numOut == 0)
        return;

    double bpm = 120.0;
    if (auto* playHead = getPlayHead())
        if (auto pos = playHead->getPosition())
            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;

    chain.setParams (reader.read(), bpm);
    if (chain.getLatency() != getLatencySamples())
        setLatencySamples (chain.getLatency());   // Low Latency <-> High Quality switch

    // hosts may occasionally send more samples than announced: work in chunks
    const int maxChunk = monoBuffer.getNumSamples();
    for (int start = 0; start < total; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, total - start);
        float* mono = monoBuffer.getWritePointer (0);

        if (numIn >= 2)
        {
            const float* l = buffer.getReadPointer (0, start);
            const float* r = buffer.getReadPointer (1, start);
            for (int i = 0; i < n; ++i) mono[i] = 0.5f * (l[i] + r[i]);
        }
        else if (numIn == 1)
        {
            juce::FloatVectorOperations::copy (mono, buffer.getReadPointer (0, start), n);
        }
        else
        {
            juce::FloatVectorOperations::clear (mono, n);
        }

        inputFifo.push (mono, n);

        float* outL = buffer.getWritePointer (0, start);
        float* outR = numOut >= 2 ? buffer.getWritePointer (1, start) : rightBuffer.getWritePointer (0);
        chain.process (mono, outL, outR, n);

        if (numOut == 1)
        {
            for (int i = 0; i < n; ++i) outL[i] = 0.5f * (outL[i] + outR[i]);
            outputFifo.push (outL, n);
        }
        else
        {
            for (int i = 0; i < n; ++i) mono[i] = 0.5f * (outL[i] + outR[i]);
            outputFifo.push (mono, n);
        }
    }

    for (int ch = 2; ch < numOut; ++ch)
        buffer.clear (ch, 0, total);
}

juce::AudioProcessorEditor* HallelujahBlueProcessor::createEditor()
{
    return new HallelujahBlueEditor (*this);
}

void HallelujahBlueProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        xml->setAttribute ("advancedView", advancedView);
        xml->setAttribute ("presetName", presets.getCurrentName());
        copyXmlToBinary (*xml, destData);
    }
}

void HallelujahBlueProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            advancedView = xml->getBoolAttribute ("advancedView", false);
            presets.setCurrentName (xml->getStringAttribute ("presetName"));
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HallelujahBlueProcessor();
}
