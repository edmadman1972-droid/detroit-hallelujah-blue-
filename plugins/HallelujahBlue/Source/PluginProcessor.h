// Detroit Hallelujah Blue - audio processor
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"
#include "Presets.h"
#include "Meters.h"

class HallelujahBlueProcessor : public juce::AudioProcessor
{
public:
    HallelujahBlueProcessor();
    ~HallelujahBlueProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 5.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    hbui::PresetManager presets;
    hb::VocalChain chain;
    hbui::AnalyzerFifo inputFifo, outputFifo;
    bool advancedView = false;

    /** Current values for the EQ overlay (message thread). */
    hb::VocalParams readParamsForDisplay() { const juce::ScopedLock sl (displayLock); return displayReader.read(); }
    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(); }

private:
    hbp::ParamReader reader { apvts };          // audio thread
    hbp::ParamReader displayReader { apvts };   // message thread
    juce::CriticalSection displayLock;
    juce::AudioBuffer<float> monoBuffer, rightBuffer;
    std::atomic<double> currentSampleRate { 48000.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallelujahBlueProcessor)
};
