// Detroit Hallelujah Blue - shared DSP utilities
// Portable C++17, no JUCE dependency, so the DSP can be unit tested on its own.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hb
{
constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }

inline float gainToDb (float g, float floorDb = -120.0f) noexcept
{
    return g > 1.0e-6f ? 20.0f * std::log10 (g) : floorDb;
}

/** One-pole smoothing coefficient for a time constant in milliseconds. */
inline float msToCoeff (float ms, double sampleRate) noexcept
{
    if (ms <= 0.0f || sampleRate <= 0.0)
        return 0.0f;
    return (float) std::exp (-1.0 / (ms * 0.001 * sampleRate));
}

/** Simple one-pole parameter smoother (used for gains to avoid zipper noise). */
struct Smoothed
{
    float current = 0.0f, target = 0.0f, coeff = 0.0f;

    void reset (float v) noexcept            { current = target = v; }
    void setTime (float ms, double sr) noexcept { coeff = msToCoeff (ms, sr); }
    void setTarget (float t) noexcept        { target = t; }
    float next() noexcept                    { current = target + coeff * (current - target); return current; }
};

/** Peak envelope follower with separate attack and release. */
struct EnvelopeFollower
{
    float env = 0.0f, attack = 0.0f, release = 0.0f;

    void setup (float attackMs, float releaseMs, double sr) noexcept
    {
        attack  = msToCoeff (attackMs, sr);
        release = msToCoeff (releaseMs, sr);
    }

    void reset() noexcept { env = 0.0f; }

    float process (float x) noexcept
    {
        const float a = std::fabs (x);
        const float c = a > env ? attack : release;
        env = a + c * (env - a);
        return env;
    }
};

inline float clamp01 (float x) noexcept { return std::min (1.0f, std::max (0.0f, x)); }

} // namespace hb
