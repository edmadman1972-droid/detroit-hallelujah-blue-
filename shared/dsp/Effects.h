// Detroit Hallelujah Blue - vocal effects (doubler, tempo delay, reverb)
#pragma once

#include "Filters.h"
#include <array>
#include <vector>

namespace hb
{
/** Power-of-two circular delay line with fractional (cubic) reads. */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writePos = 0;
    }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; }

    void push (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Integer read: delay >= 1 means the sample pushed `delay` pushes ago. */
    float read (int delay) const noexcept { return buffer[(size_t) ((writePos - delay) & mask)]; }

    /** Hermite-interpolated read, delay in samples (>= 2). */
    float readFrac (float delay) const noexcept
    {
        const int d = (int) delay;
        const float t = delay - (float) d;
        const float xm1 = read (d - 1), x0 = read (d), x1 = read (d + 1), x2 = read (d + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

private:
    std::vector<float> buffer;
    int mask = 0, writePos = 0;
};

//==============================================================================
/** Two-voice doubler: slightly delayed, slowly drifting copies panned left and right. */
class Doubler
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        line.prepare ((int) (0.05 * sr));
        tone.c = designBiquad (FilterType::LowPass, sr, 9500.0, 0.707, 0.0);
        body.c = designBiquad (FilterType::HighPass, sr, 180.0, 0.707, 0.0);
        reset();
    }

    void reset() { line.reset(); tone.reset(); body.reset(); phase1 = 0.0; phase2 = 0.37; }

    void process (float x, float& outL, float& outR) noexcept
    {
        line.push (body.process (tone.process (x)));

        phase1 += 0.37 / sampleRate; if (phase1 >= 1.0) phase1 -= 1.0;
        phase2 += 0.53 / sampleRate; if (phase2 >= 1.0) phase2 -= 1.0;

        const double ms = sampleRate * 0.001;
        const float d1 = (float) ((13.0 + 0.9 * std::sin (2.0 * kPi * phase1)) * ms);
        const float d2 = (float) ((21.0 + 1.3 * std::sin (2.0 * kPi * phase2)) * ms);

        outL = 0.8f * line.readFrac (d1);
        outR = 0.8f * line.readFrac (d2);
    }

private:
    double sampleRate = 48000.0, phase1 = 0.0, phase2 = 0.0;
    DelayLine line;
    Biquad tone, body;
};

//==============================================================================
/** Tempo-synced ping-pong delay with filtered feedback (keeps repeats out of the way). */
class TempoDelay
{
public:
    static constexpr int kNumDivisions = 7;
    static const char* divisionName (int i)
    {
        static const char* names[kNumDivisions] = { "1/2", "1/4", "1/4 D", "1/8", "1/8 D", "1/8 T", "1/16" };
        return names[std::min (std::max (i, 0), kNumDivisions - 1)];
    }
    static double divisionBeats (int i)
    {
        static const double beats[kNumDivisions] = { 2.0, 1.0, 1.5, 0.5, 0.75, 1.0 / 3.0, 0.25 };
        return beats[std::min (std::max (i, 0), kNumDivisions - 1)];
    }

    void prepare (double sr)
    {
        sampleRate = sr;
        const int maxSamples = (int) (sr * 3.2);
        lineL.prepare (maxSamples);
        lineR.prepare (maxSamples);
        for (auto* f : { &hpL, &hpR }) f->c = designBiquad (FilterType::HighPass, sr, 250.0, 0.707, 0.0);
        for (auto* f : { &lpL, &lpR }) f->c = designBiquad (FilterType::LowPass, sr, 5000.0, 0.707, 0.0);
        timeSmooth.setTime (60.0f, sr);
        setParams (bpm, division, feedback);
        timeSmooth.reset (timeSmooth.target);
        reset();
    }

    void reset()
    {
        lineL.reset(); lineR.reset();
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
        yL = yR = 0.0f;
    }

    void setParams (double bpmIn, int divisionIn, float feedbackIn)
    {
        bpm = (bpmIn > 20.0 && bpmIn < 400.0) ? bpmIn : 120.0;
        division = divisionIn;
        feedback = std::min (0.9f, std::max (0.0f, feedbackIn));
        const double seconds = 60.0 / bpm * divisionBeats (division);
        const double samples = std::min (seconds * sampleRate, sampleRate * 3.0);
        timeSmooth.setTarget ((float) std::max (4.0, samples));
    }

    void process (float x, float& outL, float& outR) noexcept
    {
        const float d = timeSmooth.next();
        yL = lineL.readFrac (d);
        yR = lineR.readFrac (d);
        // ping-pong: first echo on the left, second on the right, and so on
        lineL.push (x + feedback * lpL.process (hpL.process (yR)));
        lineR.push (feedback * lpR.process (hpR.process (yL)));
        outL = yL;
        outR = yR;
    }

private:
    double sampleRate = 48000.0, bpm = 120.0;
    int division = 1;
    float feedback = 0.3f, yL = 0.0f, yR = 0.0f;
    DelayLine lineL, lineR;
    Biquad hpL, hpR, lpL, lpR;
    Smoothed timeSmooth;
};

//==============================================================================
/** 8-line feedback delay network reverb with input diffusion and damping. */
class FdnReverb
{
public:
    static constexpr int N = 8;

    void prepare (double sr)
    {
        sampleRate = sr;
        const double scale = sr / 48000.0;
        static const int baseLengths[N] = { 1427, 1789, 1979, 2099, 2311, 2557, 2879, 3221 };
        for (int i = 0; i < N; ++i)
        {
            lengths[(size_t) i] = std::max (8, (int) (baseLengths[i] * scale));
            lines[(size_t) i].prepare (lengths[(size_t) i] + 2);
        }
        static const int apBase[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i)
        {
            apLengths[(size_t) i] = std::max (4, (int) (apBase[i] * scale));
            allpass[(size_t) i].prepare (apLengths[(size_t) i] + 2);
        }
        predelay.prepare ((int) (0.25 * sr));
        inputHp.c = designBiquad (FilterType::HighPass, sr, 200.0, 0.707, 0.0);
        setParams (decaySeconds, predelayMs, damping);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& a : allpass) a.reset();
        predelay.reset();
        inputHp.reset();
        damp.fill (0.0f);
    }

    void setParams (float decaySec, float preMs, float dampingIn)
    {
        decaySeconds = std::min (8.0f, std::max (0.2f, decaySec));
        predelayMs = std::min (240.0f, std::max (0.0f, preMs));
        damping = std::min (0.9f, std::max (0.0f, dampingIn));
        for (int i = 0; i < N; ++i)
            gains[(size_t) i] = (float) std::pow (10.0, -3.0 * lengths[(size_t) i] / (decaySeconds * sampleRate));
        predelaySamples = std::max (1, (int) (predelayMs * 0.001 * sampleRate));
    }

    void process (float x, float& outL, float& outR) noexcept
    {
        predelay.push (inputHp.process (x));
        float in = predelay.read (predelaySamples);

        // Schroeder allpass diffusion
        for (size_t i = 0; i < 4; ++i)
        {
            const float delayed = allpass[i].read (apLengths[i]);
            const float v = in + 0.62f * delayed;
            allpass[i].push (v);
            in = delayed - 0.62f * v;
        }

        std::array<float, N> s {};
        for (size_t i = 0; i < (size_t) N; ++i)
        {
            float y = lines[i].read (lengths[i]);
            damp[i] = y + damping * (damp[i] - y);   // one-pole lowpass in the loop
            s[i] = damp[i] * gains[i];
        }

        // orthogonal Hadamard mix (fast Walsh-Hadamard, normalised)
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = s[(size_t) j], b = s[(size_t) (j + h)];
                    s[(size_t) j] = a + b;
                    s[(size_t) (j + h)] = a - b;
                }
        const float norm = 0.35355339f; // 1/sqrt(8)

        float l = 0.0f, r = 0.0f;
        for (size_t i = 0; i < (size_t) N; ++i)
        {
            const float fb = s[i] * norm;
            const float sign = (i & 1) ? -1.0f : 1.0f;
            lines[i].push (fb + sign * in * 0.5f);
            if (i % 2 == 0) l += damp[i] * ((i & 2) ? -1.0f : 1.0f);
            else            r += damp[i] * ((i & 2) ? -1.0f : 1.0f);
        }
        outL = l * 0.4f;
        outR = r * 0.4f;
    }

private:
    double sampleRate = 48000.0;
    float decaySeconds = 1.4f, predelayMs = 20.0f, damping = 0.35f;
    int predelaySamples = 1;
    std::array<DelayLine, N> lines;
    std::array<int, N> lengths {};
    std::array<float, N> gains {}, damp {};
    std::array<DelayLine, 4> allpass;
    std::array<int, 4> apLengths {};
    DelayLine predelay;
    Biquad inputHp;
};

/** Mid/side width on a stereo pair: 0 = mono, 1 = unchanged, 2 = extra wide. */
inline void applyWidth (float& l, float& r, float width) noexcept
{
    const float mid = 0.5f * (l + r);
    const float side = 0.5f * (l - r) * width;
    l = mid + side;
    r = mid - side;
}

} // namespace hb
