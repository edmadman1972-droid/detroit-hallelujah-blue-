// Detroit Hallelujah Blue - parameter definitions
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "VocalChain.h"
#include <map>

namespace hbp
{
// Parameter IDs (never rename these once released: sessions and presets store them)
namespace id
{
    inline constexpr const char* inTrim        = "in_trim";
    inline constexpr const char* micProfile    = "mic_profile";
    inline constexpr const char* micModel      = "mic_model";
    inline constexpr const char* proximity     = "mic_proximity";
    inline constexpr const char* air           = "mic_air";
    inline constexpr const char* presence      = "mic_presence";
    inline constexpr const char* warmth        = "mic_warmth";

    inline constexpr const char* gateOn        = "gate_on";
    inline constexpr const char* gateThresh    = "gate_thresh";
    inline constexpr const char* gateRelease   = "gate_release";
    inline constexpr const char* gateRange     = "gate_range";
    inline constexpr const char* deessAmount   = "deess_amount";
    inline constexpr const char* deessFreq     = "deess_freq";
    inline constexpr const char* plosive       = "plosive_amount";

    inline constexpr const char* preFlavor     = "pre_flavor";
    inline constexpr const char* preDrive      = "pre_drive";

    inline constexpr const char* eqHpf         = "eq_hpf";
    inline constexpr const char* eqLowMidFreq  = "eq_lowmid_freq";
    inline constexpr const char* eqLowMidGain  = "eq_lowmid_gain";
    inline constexpr const char* eqPresFreq    = "eq_pres_freq";
    inline constexpr const char* eqPresGain    = "eq_pres_gain";
    inline constexpr const char* eqAirGain     = "eq_air_gain";

    inline constexpr const char* compMode      = "comp_mode";
    inline constexpr const char* compAmount    = "comp_amount";
    inline constexpr const char* compManual    = "comp_manual";
    inline constexpr const char* compRatio     = "comp_ratio";
    inline constexpr const char* compAttack    = "comp_attack";
    inline constexpr const char* compRelease   = "comp_release";
    inline constexpr const char* compMakeup    = "comp_makeup";

    inline constexpr const char* dblMix        = "dbl_mix";
    inline constexpr const char* verbMix       = "verb_mix";
    inline constexpr const char* verbDecay     = "verb_decay";
    inline constexpr const char* verbPredelay  = "verb_predelay";
    inline constexpr const char* dlyMix        = "dly_mix";
    inline constexpr const char* dlySync       = "dly_sync";
    inline constexpr const char* dlyFeedback   = "dly_feedback";
    inline constexpr const char* fxWidth       = "fx_width";

    inline constexpr const char* sitMix        = "sit_mix";
    inline constexpr const char* outGain       = "out_gain";
    inline constexpr const char* outCeiling    = "out_ceiling";
    inline constexpr const char* quality       = "quality";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto dbText   = [] (float v, int) { return String (v, 1) + " dB"; };
    auto msText   = [] (float v, int) { return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; };
    auto hzText   = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto tenText  = [] (float v, int) { return String (v, 1); };
    auto pctText  = [] (float v, int) { return String (roundToInt (v)) + " %"; };
    auto secText  = [] (float v, int) { return String (v, 2) + " s"; };
    auto ratioText= [] (float v, int) { return String (v, 1) + ":1"; };

    auto addF = [&] (const char* pid, const char* name, NormalisableRange<float> range, float def,
                     std::function<String (float, int)> text)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name, range, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (std::move (text))));
    };
    auto addC = [&] (const char* pid, const char* name, StringArray choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, choices, def));
    };
    auto addB = [&] (const char* pid, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def));
    };
    auto skewed = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };

    // Mic
    addF (id::inTrim,    "Input Trim",  { -24.0f, 24.0f, 0.1f }, 0.0f, dbText);
    addC (id::micProfile,"Your Mic",    { "Generic / Off", "USB Condenser", "Entry XLR Condenser", "Dynamic Handheld" }, 0);
    addC (id::micModel,  "Mic Model",   { "Bypass", "C800G-style" }, 1);
    addF (id::proximity, "Proximity",   { -6.0f, 6.0f, 0.1f }, 0.0f, dbText);
    addF (id::air,       "Air",         { 0.0f, 10.0f, 0.01f }, 5.0f, tenText);
    addF (id::presence,  "Presence",    { 0.0f, 10.0f, 0.01f }, 5.0f, tenText);
    addF (id::warmth,    "Warmth",      { 0.0f, 10.0f, 0.01f }, 5.0f, tenText);

    // Room cleanup
    addB (id::gateOn,     "Gate On", true);
    addF (id::gateThresh, "Gate Threshold", { -80.0f, -20.0f, 0.1f }, -55.0f, dbText);
    addF (id::gateRelease,"Gate Release",   skewed (20.0f, 1000.0f, 150.0f), 150.0f, msText);
    addF (id::gateRange,  "Gate Range",     { 0.0f, 60.0f, 0.1f }, 18.0f, dbText);
    addF (id::deessAmount,"De-Ess",         { 0.0f, 10.0f, 0.01f }, 4.0f, tenText);
    addF (id::deessFreq,  "De-Ess Freq",    skewed (3000.0f, 10000.0f, 6000.0f), 6500.0f, hzText);
    addF (id::plosive,    "Plosive",        { 0.0f, 10.0f, 0.01f }, 5.0f, tenText);

    // Preamp
    addC (id::preFlavor, "Preamp Flavor", { "Clean", "Warm", "Gritty" }, 1);
    addF (id::preDrive,  "Drive",         { 0.0f, 10.0f, 0.01f }, 3.0f, tenText);

    // EQ
    addC (id::eqHpf,        "High-Pass",      { "Off", "80 Hz", "120 Hz" }, 1);
    addF (id::eqLowMidFreq, "Box Cut Freq",   { 250.0f, 400.0f, 1.0f }, 320.0f, hzText);
    addF (id::eqLowMidGain, "Box Cut",        { -12.0f, 0.0f, 0.1f }, -3.0f, dbText);
    addF (id::eqPresFreq,   "Presence Freq",  skewed (2000.0f, 6000.0f, 3500.0f), 3500.0f, hzText);
    addF (id::eqPresGain,   "Presence Boost", { 0.0f, 10.0f, 0.1f }, 2.0f, dbText);
    addF (id::eqAirGain,    "Air Shelf",      { 0.0f, 10.0f, 0.1f }, 2.0f, dbText);

    // Compressor
    addC (id::compMode,    "Comp Mode",   { "Smooth (Opto)", "Punch (FET)" }, 0);
    addF (id::compAmount,  "Compression", { 0.0f, 10.0f, 0.01f }, 4.0f, tenText);
    addB (id::compManual,  "Manual Comp", false);
    addF (id::compRatio,   "Ratio",       skewed (1.5f, 20.0f, 4.0f), 4.0f, ratioText);
    addF (id::compAttack,  "Attack",      skewed (0.1f, 80.0f, 5.0f), 5.0f, msText);
    addF (id::compRelease, "Release",     skewed (20.0f, 1500.0f, 150.0f), 150.0f, msText);
    addF (id::compMakeup,  "Makeup",      { -6.0f, 12.0f, 0.1f }, 0.0f, dbText);

    // Effects
    addF (id::dblMix,       "Doubler",       { 0.0f, 100.0f, 0.1f }, 0.0f, pctText);
    addF (id::verbMix,      "Reverb",        { 0.0f, 100.0f, 0.1f }, 10.0f, pctText);
    addF (id::verbDecay,    "Reverb Decay",  skewed (0.3f, 5.0f, 1.4f), 1.4f, secText);
    addF (id::verbPredelay, "Pre-Delay",     { 0.0f, 120.0f, 0.1f }, 20.0f, msText);
    addF (id::dlyMix,       "Delay",         { 0.0f, 100.0f, 0.1f }, 0.0f, pctText);
    StringArray divisions;
    for (int i = 0; i < hb::TempoDelay::kNumDivisions; ++i) divisions.add (hb::TempoDelay::divisionName (i));
    addC (id::dlySync,      "Delay Time",    divisions, 1);
    addF (id::dlyFeedback,  "Feedback",      { 0.0f, 90.0f, 0.1f }, 30.0f, pctText);
    addF (id::fxWidth,      "FX Width",      { 0.0f, 200.0f, 0.1f }, 120.0f, pctText);

    // Mix placement + output
    addF (id::sitMix,     "Sit in the Mix", { 0.0f, 10.0f, 0.01f }, 0.0f, tenText);
    addF (id::outGain,    "Output",         { -24.0f, 12.0f, 0.1f }, 0.0f, dbText);
    addF (id::outCeiling, "Ceiling",        { -6.0f, 0.0f, 0.1f }, -1.0f, dbText);
    addC (id::quality,    "Mode",           { "Low Latency (Tracking)", "High Quality (Mixing)" }, 0);

    return layout;
}

/** Cached raw-value pointers, read on the audio thread each block.
    Keys are the id:: pointers themselves, so lookups never allocate. Call read() once
    on the message thread (the processor constructor does) to fill the cache. */
class ParamReader
{
public:
    explicit ParamReader (juce::AudioProcessorValueTreeState& s) : apvts (s) {}

    float get (const char* pid)
    {
        auto it = cache.find (pid);
        if (it == cache.end())
            it = cache.emplace (pid, apvts.getRawParameterValue (pid)).first;
        jassert (it->second != nullptr);
        return it->second != nullptr ? it->second->load() : 0.0f;
    }

    hb::VocalParams read()
    {
        hb::VocalParams p;
        p.inputTrimDb    = get (id::inTrim);
        p.micProfile     = (hb::MicProfile) (int) get (id::micProfile);
        p.micModel       = (hb::MicModel) (int) get (id::micModel);
        p.proximityDb    = get (id::proximity);
        p.air            = get (id::air);
        p.presence       = get (id::presence);
        p.warmth         = get (id::warmth);

        p.gateOn          = get (id::gateOn) > 0.5f;
        p.gateThresholdDb = get (id::gateThresh);
        p.gateReleaseMs   = get (id::gateRelease);
        p.gateRangeDb     = get (id::gateRange);
        p.deEssAmount     = get (id::deessAmount);
        p.deEssFreq       = get (id::deessFreq);
        p.plosiveAmount   = get (id::plosive);

        p.preampFlavor = (hb::Preamp::Flavor) (int) get (id::preFlavor);
        p.preampDrive  = get (id::preDrive);

        p.highPass       = (hb::HighPass) (int) get (id::eqHpf);
        p.lowMidFreq     = get (id::eqLowMidFreq);
        p.lowMidGainDb   = get (id::eqLowMidGain);
        p.presenceFreq   = get (id::eqPresFreq);
        p.presenceGainDb = get (id::eqPresGain);
        p.airGainDb      = get (id::eqAirGain);

        p.comp.mode      = (hb::Compressor::Mode) (int) get (id::compMode);
        p.comp.amount    = get (id::compAmount);
        p.comp.manual    = get (id::compManual) > 0.5f;
        p.comp.ratio     = get (id::compRatio);
        p.comp.attackMs  = get (id::compAttack);
        p.comp.releaseMs = get (id::compRelease);
        p.comp.makeupDb  = get (id::compMakeup);

        p.doublerMix       = get (id::dblMix) * 0.01f;
        p.reverbMix        = get (id::verbMix) * 0.01f;
        p.reverbDecay      = get (id::verbDecay);
        p.reverbPredelayMs = get (id::verbPredelay);
        p.delayMix         = get (id::dlyMix) * 0.01f;
        p.delayDivision    = (int) get (id::dlySync);
        p.delayFeedback    = get (id::dlyFeedback) * 0.01f;
        p.fxWidth          = get (id::fxWidth) * 0.01f;

        p.sitInMix     = get (id::sitMix);
        p.outputGainDb = get (id::outGain);
        p.ceilingDb    = get (id::outCeiling);
        p.highQuality  = get (id::quality) > 0.5f;
        return p;
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::map<const char*, std::atomic<float>*> cache;
};

} // namespace hbp
