// Detroit Hallelujah Blue - preamp saturation
#pragma once

#include "Filters.h"
#include <vector>

namespace hb
{
inline double logCosh (double x) noexcept
{
    const double a = std::fabs (x);
    return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994530942;
}

/** Waveshaper with first-order antiderivative anti-aliasing (ADAA).
    Cuts most of the aliasing a plain waveshaper creates, at almost no CPU cost. */
class AdaaShaper
{
public:
    enum class Curve { Tanh, HardClip };

    void setCurve (Curve c, double biasIn) noexcept { curve = c; bias = biasIn; reset(); }
    void reset() noexcept { xPrev = 0.0; fPrev = F (0.0); }

    /** The plain (non anti-aliased) curve. */
    double f (double x) const noexcept
    {
        if (curve == Curve::Tanh)
            return std::tanh (x + bias) - std::tanh (bias);
        return clip (x + bias) - clip (bias);
    }

    float process (float in) noexcept
    {
        const double x = in;
        const double Fx = F (x);
        const double d = x - xPrev;
        const double y = std::fabs (d) < 1.0e-5 ? f (0.5 * (x + xPrev)) : (Fx - fPrev) / d;
        xPrev = x;
        fPrev = Fx;
        return (float) y;
    }

private:
    static double clip (double x) noexcept { return std::min (1.0, std::max (-1.0, x)); }
    static double clipAntideriv (double x) noexcept
    {
        return std::fabs (x) <= 1.0 ? 0.5 * x * x : std::fabs (x) - 0.5;
    }

    double F (double x) const noexcept
    {
        if (curve == Curve::Tanh)
            return logCosh (x + bias) - std::tanh (bias) * x;
        return clipAntideriv (x + bias) - clip (bias) * x;
    }

    Curve curve = Curve::Tanh;
    double bias = 0.0, xPrev = 0.0, fPrev = 0.0;
};

//==============================================================================
/** 2x oversampler using a linear-phase windowed-sinc halfband FIR.
    Latency is reported in base-rate samples. */
class Oversampler2x
{
public:
    static constexpr int kTaps = 47;

    Oversampler2x()
    {
        const int M = (kTaps - 1) / 2;
        double sum = 0.0;
        for (int i = 0; i < kTaps; ++i)
        {
            const double t = i - M;
            const double sinc = t == 0.0 ? 1.0 : std::sin (kPi * 0.5 * t) / (kPi * 0.5 * t);
            const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * i / (kTaps - 1))
                                  + 0.08 * std::cos (4.0 * kPi * i / (kTaps - 1)); // Blackman
            h[i] = sinc * w;
            sum += h[i];
        }
        for (auto& v : h) v /= sum;
        reset();
    }

    void reset()
    {
        std::fill (std::begin (upHist), std::end (upHist), 0.0f);
        std::fill (std::begin (downHist), std::end (downHist), 0.0f);
        upPos = downPos = 0;
    }

    /** Group delay of up + down filters, in base-rate samples. */
    static constexpr int getLatency() noexcept { return (kTaps - 1) / 2; }

    void upsample (float x, float& o0, float& o1) noexcept
    {
        o0 = push (upHist, upPos, 2.0f * x);
        o1 = push (upHist, upPos, 0.0f);
    }

    float downsample (float a, float b) noexcept
    {
        // keep the even-phase output so the total delay is a whole number of base samples
        const float y = push (downHist, downPos, a);
        push (downHist, downPos, b);
        return y;
    }

private:
    // history stored twice so the convolution reads one contiguous block
    float push (float* hist, int& p, float v) noexcept
    {
        p = (p == 0 ? kTaps - 1 : p - 1);
        hist[p] = v;
        hist[p + kTaps] = v;
        double acc = 0.0;
        const float* s = hist + p;
        for (int i = 0; i < kTaps; ++i)
            acc += h[i] * s[i];
        return (float) acc;
    }

    double h[kTaps] {};
    float upHist[2 * kTaps] {}, downHist[2 * kTaps] {};
    int upPos = 0, downPos = 0;
};

//==============================================================================
/** Preamp with three flavours.
    Clean  - symmetric soft saturation, only colours hot peaks.
    Warm   - asymmetric (even harmonics) with transformer-style low bloom and soft top.
    Gritty - mid-focused hard clipping for an aggressive edge. */
class Preamp
{
public:
    enum class Flavor { Clean = 0, Warm = 1, Gritty = 2 };

    void prepare (double sr)
    {
        sampleRate = sr;
        setParams (flavor, drive, oversample);
        reset();
    }

    void reset()
    {
        shaper.reset(); os.reset();
        preEmph.reset(); deEmph.reset(); bloom.reset(); soften.reset();
    }

    void setParams (Flavor fl, float driveIn, bool oversampleIn)
    {
        const bool changed = fl != flavor || oversampleIn != oversample;
        flavor = fl;
        drive = std::min (10.0f, std::max (0.0f, driveIn));
        oversample = oversampleIn;

        const double rate = oversample ? sampleRate * 2.0 : sampleRate;
        double bias = 0.0, maxDriveDb = 12.0;
        switch (flavor)
        {
            case Flavor::Clean:  shaperCurve = AdaaShaper::Curve::Tanh;     bias = 0.0;  maxDriveDb = 12.0; break;
            case Flavor::Warm:   shaperCurve = AdaaShaper::Curve::Tanh;     bias = 0.25; maxDriveDb = 18.0; break;
            case Flavor::Gritty: shaperCurve = AdaaShaper::Curve::HardClip; bias = 0.08; maxDriveDb = 24.0; break;
        }
        if (changed || bias != currentBias)
        {
            shaper.setCurve (shaperCurve, bias);
            currentBias = bias;
            if (changed) os.reset();
        }

        preGain = dbToGain ((float) maxDriveDb * drive / 10.0f);

        // gain compensation: a -12 dBFS sine should come out at roughly the same level
        const double ref = 0.25 * kHeadroom;
        const double driven = 0.5 * (shaper.f (ref * preGain) - shaper.f (-ref * preGain));
        compGain = (float) (ref / std::max (driven, 1.0e-6));

        const bool gritty = flavor == Flavor::Gritty;
        preEmph.c = designBiquad (gritty ? FilterType::Peak : FilterType::Flat, rate, 1500.0, 0.8, 3.0 + 0.3 * drive);
        deEmph.c  = designBiquad (gritty ? FilterType::Peak : FilterType::Flat, rate, 1500.0, 0.8, -(3.0 + 0.3 * drive) * 0.7);

        const bool warm = flavor == Flavor::Warm;
        bloom.c  = designBiquad (warm ? FilterType::LowShelf : FilterType::Flat,  sampleRate, 90.0, 0.7, 0.15 * drive);
        soften.c = designBiquad (warm ? FilterType::HighShelf : FilterType::Flat, sampleRate, 14000.0, 0.7, -0.12 * drive);
    }

    int getLatency() const noexcept { return oversample ? Oversampler2x::getLatency() : 0; }

    float process (float x) noexcept
    {
        float y;
        if (oversample)
        {
            float a, b;
            os.upsample (x, a, b);
            a = shape (a);
            b = shape (b);
            y = os.downsample (a, b);
        }
        else
        {
            y = shape (x);
        }
        return soften.process (bloom.process (y));
    }

private:
    static constexpr float kHeadroom = 0.5f;

    float shape (float x) noexcept
    {
        x = preEmph.process (x);
        const float s = shaper.process (x * preGain * kHeadroom) / kHeadroom;
        return deEmph.process (s * compGain);
    }

    double sampleRate = 48000.0, currentBias = -1.0;
    Flavor flavor = Flavor::Warm;
    AdaaShaper::Curve shaperCurve = AdaaShaper::Curve::Tanh;
    float drive = 3.0f, preGain = 1.0f, compGain = 1.0f;
    bool oversample = false;
    AdaaShaper shaper;
    Oversampler2x os;
    Biquad preEmph, deEmph, bloom, soften;
};

} // namespace hb
