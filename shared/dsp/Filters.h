// Detroit Hallelujah Blue - filters
#pragma once

#include "HBMath.h"
#include <complex>

namespace hb
{
enum class FilterType { Flat, LowPass, HighPass, BandPass, Peak, LowShelf, HighShelf };

/** Normalised biquad coefficients (a0 == 1). */
struct BiquadCoeffs
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;

    /** Magnitude response in dB at a frequency. Used to draw the EQ curve. */
    double magnitudeDb (double freq, double sampleRate) const noexcept
    {
        const double w = 2.0 * kPi * freq / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w);
        const std::complex<double> z2 = z1 * z1;
        const auto num = (double) b0 + (double) b1 * z1 + (double) b2 * z2;
        const auto den = 1.0 + (double) a1 * z1 + (double) a2 * z2;
        const double mag = std::abs (num / den);
        return 20.0 * std::log10 (std::max (mag, 1.0e-9));
    }
};

/** RBJ "Audio EQ Cookbook" designs. */
inline BiquadCoeffs designBiquad (FilterType type, double sr, double freq, double q, double gainDb) noexcept
{
    BiquadCoeffs c;
    if (type == FilterType::Flat || sr <= 0.0)
        return c;

    freq = std::min (std::max (freq, 10.0), sr * 0.49);
    q = std::max (q, 0.05);

    const double A    = std::pow (10.0, gainDb / 40.0);
    const double w0   = 2.0 * kPi * freq / sr;
    const double cosw = std::cos (w0);
    const double sinw = std::sin (w0);
    const double alpha = sinw / (2.0 * q);
    const double sqA  = std::sqrt (A);

    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (type)
    {
        case FilterType::LowPass:
            b0 = (1 - cosw) * 0.5; b1 = 1 - cosw; b2 = (1 - cosw) * 0.5;
            a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
            break;
        case FilterType::HighPass:
            b0 = (1 + cosw) * 0.5; b1 = -(1 + cosw); b2 = (1 + cosw) * 0.5;
            a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
            break;
        case FilterType::BandPass:
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0 = 1 + alpha; a1 = -2 * cosw; a2 = 1 - alpha;
            break;
        case FilterType::Peak:
            b0 = 1 + alpha * A; b1 = -2 * cosw; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * cosw; a2 = 1 - alpha / A;
            break;
        case FilterType::LowShelf:
            b0 = A * ((A + 1) - (A - 1) * cosw + 2 * sqA * alpha);
            b1 = 2 * A * ((A - 1) - (A + 1) * cosw);
            b2 = A * ((A + 1) - (A - 1) * cosw - 2 * sqA * alpha);
            a0 = (A + 1) + (A - 1) * cosw + 2 * sqA * alpha;
            a1 = -2 * ((A - 1) + (A + 1) * cosw);
            a2 = (A + 1) + (A - 1) * cosw - 2 * sqA * alpha;
            break;
        case FilterType::HighShelf:
            b0 = A * ((A + 1) + (A - 1) * cosw + 2 * sqA * alpha);
            b1 = -2 * A * ((A - 1) + (A + 1) * cosw);
            b2 = A * ((A + 1) + (A - 1) * cosw - 2 * sqA * alpha);
            a0 = (A + 1) - (A - 1) * cosw + 2 * sqA * alpha;
            a1 = 2 * ((A - 1) - (A + 1) * cosw);
            a2 = (A + 1) - (A - 1) * cosw - 2 * sqA * alpha;
            break;
        case FilterType::Flat:
            break;
    }

    c.b0 = (float) (b0 / a0); c.b1 = (float) (b1 / a0); c.b2 = (float) (b2 / a0);
    c.a1 = (float) (a1 / a0); c.a2 = (float) (a2 / a0);
    return c;
}

/** Transposed direct form II biquad. */
struct Biquad
{
    BiquadCoeffs c;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return y;
    }
};

/** Topology-preserving-transform state variable filter (Zavalishin / Simper).
    Stable under fast modulation, used for the dynamic (de-esser / ducking) bands. */
struct SVF
{
    struct Out { float lp, bp, hp, bpNorm; };

    float a1 = 0, a2 = 0, a3 = 0, k = 1;
    float ic1 = 0, ic2 = 0;

    void setup (double sr, double freq, double q) noexcept
    {
        freq = std::min (std::max (freq, 10.0), sr * 0.49);
        const double g = std::tan (kPi * freq / sr);
        k  = (float) (1.0 / std::max (q, 0.05));
        a1 = (float) (1.0 / (1.0 + g * (g + k)));
        a2 = (float) (g * a1);
        a3 = (float) (g * a2);
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    Out process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return { v2, v1, x - k * v1 - v2, k * v1 };
    }
};

} // namespace hb
