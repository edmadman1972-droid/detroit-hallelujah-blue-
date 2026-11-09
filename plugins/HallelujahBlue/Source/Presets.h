// Detroit Hallelujah Blue - factory presets
#pragma once

#include "Parameters.h"
#include "PresetManager.h"

namespace hbp
{
inline std::vector<hbui::FactoryPreset> factoryPresets()
{
    using namespace id;
    return {
        { "C800G-style (Default)", {} },

        { "Dark and Heavy", {
            { air, 3.0f }, { presence, 4.0f }, { warmth, 7.5f }, { proximity, 2.0f },
            { eqHpf, 1 }, { eqLowMidFreq, 300 }, { eqLowMidGain, -2.0f }, { eqPresFreq, 3000 }, { eqPresGain, 1.0f }, { eqAirGain, 0.5f },
            { preFlavor, 1 }, { preDrive, 5.0f },
            { compMode, 1 }, { compAmount, 5.5f },
            { deessAmount, 4.0f }, { verbMix, 8.0f } } },

        { "Bright and Crisp", {
            { air, 7.5f }, { presence, 6.5f }, { warmth, 4.0f },
            { eqHpf, 2 }, { eqLowMidFreq, 350 }, { eqLowMidGain, -4.0f }, { eqPresFreq, 4200 }, { eqPresGain, 3.5f }, { eqAirGain, 4.0f },
            { preFlavor, 0 }, { preDrive, 2.0f },
            { compMode, 1 }, { compAmount, 5.0f },
            { deessAmount, 5.5f }, { verbMix, 6.0f } } },

        { "Melodic", {
            { air, 6.0f }, { presence, 5.0f }, { warmth, 5.5f },
            { eqHpf, 1 }, { eqLowMidGain, -3.0f }, { eqPresGain, 2.0f }, { eqAirGain, 3.0f },
            { preFlavor, 1 }, { preDrive, 3.0f },
            { compMode, 0 }, { compAmount, 4.5f },
            { dblMix, 25.0f }, { verbMix, 22.0f }, { verbDecay, 2.2f }, { verbPredelay, 35.0f },
            { dlyMix, 12.0f }, { dlySync, 4 }, { dlyFeedback, 35.0f }, { fxWidth, 140.0f } } },

        { "Ad-lib", {
            { air, 7.0f }, { presence, 6.5f }, { warmth, 3.0f },
            { eqHpf, 2 }, { eqLowMidGain, -5.0f }, { eqPresGain, 4.0f }, { eqAirGain, 3.0f },
            { preFlavor, 2 }, { preDrive, 4.5f },
            { compMode, 1 }, { compAmount, 6.5f },
            { dblMix, 40.0f }, { verbMix, 25.0f }, { verbDecay, 1.8f },
            { dlyMix, 25.0f }, { dlySync, 3 }, { dlyFeedback, 40.0f }, { fxWidth, 180.0f },
            { outGain, -3.0f } } },

        { "USB Condenser Fix", {
            { micProfile, 1 }, { gateThresh, -50.0f }, { plosive, 6.5f }, { deessAmount, 4.5f } } },

        { "Entry XLR Condenser Fix", {
            { micProfile, 2 }, { deessAmount, 5.0f } } },

        { "Dynamic Mic Lift", {
            { micProfile, 3 }, { air, 6.0f }, { gateThresh, -48.0f }, { proximity, -1.5f } } },
    };
}

/** EQ-only one-click presets for the EQ section buttons. */
inline std::vector<std::pair<const char*, float>> eqPreset (int which)
{
    using namespace id;
    switch (which)
    {
        case 0:  return { { eqHpf, 1 }, { eqLowMidFreq, 300 }, { eqLowMidGain, -2.0f }, { eqPresFreq, 3000 }, { eqPresGain, 1.0f }, { eqAirGain, 0.5f } };
        case 1:  return { { eqHpf, 2 }, { eqLowMidFreq, 350 }, { eqLowMidGain, -4.0f }, { eqPresFreq, 4200 }, { eqPresGain, 3.5f }, { eqAirGain, 4.0f } };
        default: return { { eqHpf, 1 }, { eqLowMidFreq, 320 }, { eqLowMidGain, -3.0f }, { eqPresFreq, 3500 }, { eqPresGain, 2.0f }, { eqAirGain, 3.0f } };
    }
}

} // namespace hbp
