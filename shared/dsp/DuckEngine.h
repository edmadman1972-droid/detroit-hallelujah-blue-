// Detroit Hallelujah Duck - spectral sidechain ducking for the beat bus
//
// The vocal arrives on the sidechain input. It is split into 8 bands; in each band,
// the more vocal energy there is, the more that same band of the beat is dipped
// (up to `depthDb`). The beat is only carved where - and while - the voice needs room.
#pragma once

#include "Filters.h"
#include <array>
#include <atomic>

namespace hb
{
struct DuckParams
{
    float depthDb = 4.0f;          // max dip per band, 0 .. 18 dB
    float sensitivityDb = -35.0f;  // vocal band level where ducking starts
    float attackMs = 8.0f;
    float releaseMs = 160.0f;
    float lowHz = 200.0f;          // range of the beat that may be dipped
    float highHz = 6000.0f;
    bool  midOnly = true;          // duck only the centre (keeps the beat's stereo sides intact)
    float mix = 1.0f;              // 0 .. 1, scales the depth
    float outputGainDb = 0.0f;
};

class DuckEngine
{
public:
    static constexpr int kBands = 8;
    static constexpr std::array<float, kBands> bandFreqs { 150.f, 260.f, 450.f, 780.f, 1350.f, 2300.f, 4000.f, 7000.f };
    static constexpr float kQ = 1.8f;

    void prepare (double sr)
    {
        sampleRate = sr;
        for (int b = 0; b < kBands; ++b)
        {
            const double f = bandFreqs[(size_t) b];
            detect[(size_t) b].setup (sr, f, kQ);
            applyA[(size_t) b].setup (sr, f, kQ);
            applyB[(size_t) b].setup (sr, f, kQ);
            detEnv[(size_t) b].setup (1.0f, 60.0f, sr);
        }
        outGain.setTime (20.0f, sr);
        setParams (params);
        outGain.reset (outGain.target);
        reset();
    }

    void reset()
    {
        for (auto& f : detect) f.reset();
        for (auto& f : applyA) f.reset();
        for (auto& f : applyB) f.reset();
        for (auto& e : detEnv) e.reset();
        gainDb.fill (0.0f);
    }

    void setParams (const DuckParams& p)
    {
        params = p;
        attack  = msToCoeff (std::max (0.5f, p.attackMs), sampleRate);
        release = msToCoeff (std::max (5.0f, p.releaseMs), sampleRate);
        for (int b = 0; b < kBands; ++b)
            weights[(size_t) b] = bandWeight (bandFreqs[(size_t) b], p.lowHz, p.highHz);
        outGain.setTarget (dbToGain (p.outputGainDb));
    }

    /** Soft range: bands outside low..high fade out over ~3/4 octave. */
    static float bandWeight (float f, float lo, float hi) noexcept
    {
        const float below = std::max (0.0f, std::log2 (lo / f));
        const float above = std::max (0.0f, std::log2 (f / hi));
        return clamp01 (1.0f - below / 0.75f) * clamp01 (1.0f - above / 0.75f);
    }

    /** Process the beat in place. `right` may be nullptr for a mono beat.
        `scLeft` / `scRight` are the vocal sidechain (either may be nullptr). */
    void process (float* left, float* right, const float* scLeft, const float* scRight, int numSamples) noexcept
    {
        const float depth = params.depthDb * clamp01 (params.mix);
        float scPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sc = 0.0f;
            if (scLeft && scRight) sc = 0.5f * (scLeft[i] + scRight[i]);
            else if (scLeft)       sc = scLeft[i];
            else if (scRight)      sc = scRight[i];
            scPeak = std::max (scPeak, std::fabs (sc));

            std::array<float, kBands> g {};
            for (size_t b = 0; b < (size_t) kBands; ++b)
            {
                const float env = detEnv[b].process (detect[b].process (sc).bpNorm);
                const float s = clamp01 ((gainToDb (env) - params.sensitivityDb) / 18.0f);
                const float target = depth * weights[b] * s;                     // dB of reduction
                const float c = target > gainDb[b] ? attack : release;
                gainDb[b] = target + c * (gainDb[b] - target);
                g[b] = dbToGain (-gainDb[b]);
            }

            const float og = outGain.next();
            if (right == nullptr)
            {
                left[i] = applyBands (applyA, g, left[i]) * og;
            }
            else if (params.midOnly)
            {
                const float mid = 0.5f * (left[i] + right[i]);
                const float side = 0.5f * (left[i] - right[i]);
                const float m = applyBands (applyA, g, mid);
                left[i] = (m + side) * og;
                right[i] = (m - side) * og;
            }
            else
            {
                left[i]  = applyBands (applyA, g, left[i]) * og;
                right[i] = applyBands (applyB, g, right[i]) * og;
            }
        }

        for (size_t b = 0; b < (size_t) kBands; ++b)
            bandReductionDb[b].store (gainDb[b], std::memory_order_relaxed);
        sidechainPeak.store (std::max (sidechainPeak.load (std::memory_order_relaxed), scPeak), std::memory_order_relaxed);
    }

    std::array<std::atomic<float>, kBands> bandReductionDb {};
    std::atomic<float> sidechainPeak { 0.0f };

private:
    /** Cascade of SVF peaking cuts whose depth can change every sample without clicks. */
    static float applyBands (std::array<SVF, kBands>& filters, const std::array<float, kBands>& g, float x) noexcept
    {
        for (size_t b = 0; b < (size_t) kBands; ++b)
            if (g[b] < 0.9999f)
                x += (g[b] - 1.0f) * filters[b].process (x).bpNorm;
            else
                filters[b].process (x);   // keep the filter state running
        return x;
    }

    double sampleRate = 48000.0;
    DuckParams params;
    std::array<SVF, kBands> detect, applyA, applyB;
    std::array<EnvelopeFollower, kBands> detEnv;
    std::array<float, kBands> gainDb {}, weights {};
    float attack = 0.0f, release = 0.0f;
    Smoothed outGain;
};

} // namespace hb
