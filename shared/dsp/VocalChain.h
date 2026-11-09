// Detroit Hallelujah Blue - the full vocal channel
//
// Signal flow:
//   Input trim -> Plosive reducer -> Mic voicing (budget-mic correction + C800G-style model)
//   -> Gate -> Preamp -> EQ -> Compressor -> De-esser
//   -> [dry centred] + [doubler + delay + reverb, width-controlled]
//   -> Output gain -> Limiter
#pragma once

#include "Dynamics.h"
#include "Effects.h"
#include "Saturation.h"
#include <atomic>

namespace hb
{
enum class MicProfile { None = 0, UsbCondenser = 1, EntryXlrCondenser = 2, DynamicHandheld = 3 };
enum class MicModel   { Bypass = 0, C800GStyle = 1 };
enum class HighPass   { Off = 0, Hz80 = 1, Hz120 = 2 };

struct VocalParams
{
    // input + mic
    float inputTrimDb = 0.0f;
    MicProfile micProfile = MicProfile::None;
    MicModel micModel = MicModel::C800GStyle;
    float proximityDb = 0.0f;    // -6 .. +6
    float air = 5.0f;            // 0 .. 10
    float presence = 5.0f;       // 0 .. 10
    float warmth = 5.0f;         // 0 .. 10

    // room cleanup
    bool  gateOn = true;
    float gateThresholdDb = -55.0f;
    float gateReleaseMs = 150.0f;
    float gateRangeDb = 18.0f;
    float deEssAmount = 4.0f;
    float deEssFreq = 6500.0f;
    float plosiveAmount = 5.0f;

    // preamp
    Preamp::Flavor preampFlavor = Preamp::Flavor::Warm;
    float preampDrive = 3.0f;

    // EQ
    HighPass highPass = HighPass::Hz80;
    float lowMidFreq = 320.0f;
    float lowMidGainDb = -3.0f;
    float presenceFreq = 3500.0f;
    float presenceGainDb = 2.0f;
    float airGainDb = 2.0f;

    // compressor
    Compressor::Settings comp;

    // effects (mix values 0..1)
    float doublerMix = 0.0f;
    float reverbMix = 0.10f;
    float reverbDecay = 1.4f;
    float reverbPredelayMs = 20.0f;
    float delayMix = 0.0f;
    int   delayDivision = 1;
    float delayFeedback = 0.3f;
    float fxWidth = 1.2f;        // 0 .. 2

    // mix placement + output
    float sitInMix = 0.0f;       // 0 .. 10 macro
    float outputGainDb = 0.0f;
    float ceilingDb = -1.0f;
    bool  highQuality = false;
};

class VocalChain
{
public:
    static constexpr int kNumStaticFilters = 12;

    /** Designs every static EQ-type filter (mic profile, mic model, EQ section).
        Shared by the audio path and the editor's EQ curve overlay. */
    static std::array<BiquadCoeffs, kNumStaticFilters> designStaticFilters (const VocalParams& p, double sr)
    {
        std::array<BiquadCoeffs, kNumStaticFilters> f {};
        auto d = [sr] (FilterType t, double fr, double q, double g) { return designBiquad (t, sr, fr, q, g); };

        // 0-2: budget mic correction starting points
        switch (p.micProfile)
        {
            case MicProfile::UsbCondenser:      // thin low end, harsh upper mids, little air
                f[0] = d (FilterType::LowShelf, 150, 0.7, 2.5);
                f[1] = d (FilterType::Peak, 3000, 2.0, -3.0);
                f[2] = d (FilterType::HighShelf, 12000, 0.7, 2.0);
                break;
            case MicProfile::EntryXlrCondenser: // boxy low mids, sibilant peak
                f[0] = d (FilterType::Peak, 300, 1.0, -1.5);
                f[1] = d (FilterType::Peak, 7000, 3.0, -2.5);
                f[2] = d (FilterType::HighShelf, 14000, 0.7, 1.0);
                break;
            case MicProfile::DynamicHandheld:   // proximity boom, dull top
                f[0] = d (FilterType::LowShelf, 180, 0.7, -2.0);
                f[1] = d (FilterType::Peak, 4000, 0.8, 3.0);
                f[2] = d (FilterType::HighShelf, 9000, 0.7, 5.0);
                break;
            case MicProfile::None: break;
        }

        // 3-7: C800G-style voicing - bright airy top, forward presence, controllable body
        if (p.micModel == MicModel::C800GStyle)
        {
            f[3] = d (FilterType::LowShelf, 120, 0.7, p.proximityDb);
            f[4] = d (FilterType::Peak, 200, 0.8, (p.warmth - 4.0f) * 1.0f);
            f[5] = d (FilterType::Peak, 4500, 1.0, p.presence * 0.7f);
            f[6] = d (FilterType::HighShelf, 10000, 0.7, p.air * 0.8f);
            f[7] = d (FilterType::Peak, 11000, 2.0, 1.5);     // signature sheen
        }

        // 8-11: EQ section
        if (p.highPass != HighPass::Off)
            f[8] = d (FilterType::HighPass, p.highPass == HighPass::Hz80 ? 80.0 : 120.0, 0.707, 0.0);
        f[9]  = d (FilterType::Peak, p.lowMidFreq, 1.4, p.lowMidGainDb);
        const float sit = std::min (10.0f, std::max (0.0f, p.sitInMix)) / 10.0f;
        f[10] = d (FilterType::Peak, p.presenceFreq, 0.9, p.presenceGainDb + 3.0f * sit); // "Sit in the Mix" adds presence
        f[11] = d (FilterType::HighShelf, 12000, 0.7, p.airGainDb);
        return f;
    }

    /** Combined response of the static filters, for drawing. */
    static float responseDb (const std::array<BiquadCoeffs, kNumStaticFilters>& f, double sr, double freq)
    {
        double sum = 0.0;
        for (auto& c : f) sum += c.magnitudeDb (freq, sr);
        return (float) sum;
    }

    void prepare (double sr, int /*maxBlockSize*/)
    {
        sampleRate = sr;
        plosive.prepare (sr);
        gate.prepare (sr);
        preamp.prepare (sr);
        comp.prepare (sr);
        deEsser.prepare (sr);
        doubler.prepare (sr);
        delay.prepare (sr);
        reverb.prepare (sr);
        for (auto& b : filters) b.reset();
        trim.setTime (20.0f, sr);
        outGain.setTime (20.0f, sr);
        dblMix.setTime (30.0f, sr);
        dlyMix.setTime (30.0f, sr);
        vrbMix.setTime (30.0f, sr);
        width.setTime (30.0f, sr);
        limiterHQ = params.highQuality;
        limiter.prepare (sr, lookaheadFor (limiterHQ));
        setParams (params);
        trim.reset (trim.target); outGain.reset (outGain.target);
        dblMix.reset (dblMix.target); dlyMix.reset (dlyMix.target);
        vrbMix.reset (vrbMix.target); width.reset (width.target);
    }

    void reset()
    {
        plosive.reset(); gate.reset(); preamp.reset(); comp.reset(); deEsser.reset();
        doubler.reset(); delay.reset(); reverb.reset(); limiter.reset();
        for (auto& b : filters) b.reset();
    }

    /** Call once per block (before process) with the current parameter values. */
    void setParams (const VocalParams& p, double hostBpm = 120.0)
    {
        params = p;
        const float sit = std::min (10.0f, std::max (0.0f, p.sitInMix)) / 10.0f;

        auto coeffs = designStaticFilters (p, sampleRate);
        for (size_t i = 0; i < coeffs.size(); ++i) filters[i].c = coeffs[i];

        plosive.setAmount (p.plosiveAmount);
        gate.setParams (p.gateOn, p.gateThresholdDb, p.gateReleaseMs, p.gateRangeDb);
        preamp.setParams (p.preampFlavor, p.preampDrive, p.highQuality);

        auto cs = p.comp;
        cs.amount = std::min (10.0f, cs.amount + 3.0f * sit);   // "Sit in the Mix" adds glue
        comp.set (cs);

        deEsser.setParams (p.deEssAmount, p.deEssFreq);
        delay.setParams (hostBpm, p.delayDivision, p.delayFeedback);
        reverb.setParams (p.reverbDecay, p.reverbPredelayMs, 0.35f);

        trim.setTarget (dbToGain (p.inputTrimDb));
        outGain.setTarget (dbToGain (p.outputGainDb));
        dblMix.setTarget (p.doublerMix);
        dlyMix.setTarget (p.delayMix);
        vrbMix.setTarget (std::min (1.0f, p.reverbMix + 0.12f * sit));
        width.setTarget (p.fxWidth);

        limiter.setCeilingDb (p.ceilingDb);
        if (p.highQuality != limiterHQ)
        {
            limiterHQ = p.highQuality;
            limiter.prepare (sampleRate, lookaheadFor (limiterHQ));
            limiter.setCeilingDb (p.ceilingDb);
        }
    }

    int getLatency() const noexcept { return preamp.getLatency() + limiter.getLatency(); }

    /** Mono vocal in, stereo out. `in` may alias outL. */
    void process (const float* in, float* outL, float* outR, int numSamples) noexcept
    {
        float inPk = 0.0f, outPk = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float x = in[i] * trim.next();
            inPk = std::max (inPk, std::fabs (x));

            x = plosive.process (x);
            for (size_t f = 0; f < 8; ++f)  x = filters[f].process (x);   // mic correction + model
            x = gate.process (x);
            x = preamp.process (x);
            for (size_t f = 8; f < 12; ++f) x = filters[f].process (x);   // EQ
            x = comp.process (x);
            x = deEsser.process (x);

            // effects (send style: dry stays at full level, centred)
            float dL, dR, eL, eR, rL, rR;
            doubler.process (x, dL, dR);
            delay.process (x, eL, eR);
            reverb.process (x + 0.5f * (dL + dR) * dblMix.current, rL, rR);

            const float dm = dblMix.next(), em = dlyMix.next(), rm = vrbMix.next();
            float wL = dL * dm + eL * em + rL * rm;
            float wR = dR * dm + eR * em + rR * rm;
            applyWidth (wL, wR, width.next());

            const float g = outGain.next();
            float l = (x + wL) * g;
            float r = (x + wR) * g;
            limiter.process (l, r);

            outL[i] = l;
            outR[i] = r;
            outPk = std::max (outPk, std::max (std::fabs (l), std::fabs (r)));
        }

        meters.inputPeak.store (std::max (meters.inputPeak.load (std::memory_order_relaxed), inPk), std::memory_order_relaxed);
        meters.outputPeak.store (std::max (meters.outputPeak.load (std::memory_order_relaxed), outPk), std::memory_order_relaxed);
        meters.compGr.store (comp.getReductionDb(), std::memory_order_relaxed);
        meters.deEssGr.store (deEsser.getReductionDb(), std::memory_order_relaxed);
        meters.gateGr.store (gate.getReductionDb(), std::memory_order_relaxed);
        meters.plosiveGr.store (plosive.getReductionDb(), std::memory_order_relaxed);
        meters.limiterGr.store (limiter.getReductionDb(), std::memory_order_relaxed);
    }

    /** Meter values written by the audio thread, read (and peaks cleared) by the UI. */
    struct Meters
    {
        std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
        std::atomic<float> compGr { 0.0f }, deEssGr { 0.0f }, gateGr { 0.0f }, plosiveGr { 0.0f }, limiterGr { 0.0f };
    } meters;

private:
    int lookaheadFor (bool hq) const { return hq ? std::max (1, (int) (0.003 * sampleRate)) : 1; }

    double sampleRate = 48000.0;
    VocalParams params;
    std::array<Biquad, kNumStaticFilters> filters;
    PlosiveReducer plosive;
    Gate gate;
    Preamp preamp;
    Compressor comp;
    DeEsser deEsser;
    Doubler doubler;
    TempoDelay delay;
    FdnReverb reverb;
    Limiter limiter;
    bool limiterHQ = false;
    Smoothed trim, outGain, dblMix, dlyMix, vrbMix, width;
};

} // namespace hb
