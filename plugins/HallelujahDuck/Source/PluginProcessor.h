// Hallelujah Duck - sidechain spectral ducker for the beat bus
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "DuckEngine.h"
#include "PresetManager.h"

namespace hbd::id
{
    inline constexpr const char* depth    = "duck_depth";
    inline constexpr const char* sens     = "duck_sens";
    inline constexpr const char* attack   = "duck_attack";
    inline constexpr const char* release  = "duck_release";
    inline constexpr const char* low      = "duck_low";
    inline constexpr const char* high     = "duck_high";
    inline constexpr const char* stereo   = "duck_stereo";
    inline constexpr const char* mix      = "duck_mix";
    inline constexpr const char* out      = "duck_out";
}

class HallelujahDuckProcessor : public juce::AudioProcessor
{
public:
    HallelujahDuckProcessor();
    ~HallelujahDuckProcessor() override = default;

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
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    hbui::PresetManager presets;
    hb::DuckEngine engine;
    std::atomic<bool> sidechainConnected { false };

private:
    std::atomic<float>* pDepth = nullptr; std::atomic<float>* pSens = nullptr;
    std::atomic<float>* pAttack = nullptr; std::atomic<float>* pRelease = nullptr;
    std::atomic<float>* pLow = nullptr; std::atomic<float>* pHigh = nullptr;
    std::atomic<float>* pStereo = nullptr; std::atomic<float>* pMix = nullptr; std::atomic<float>* pOut = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HallelujahDuckProcessor)
};
