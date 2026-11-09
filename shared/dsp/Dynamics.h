// Detroit Hallelujah Blue - dynamics processors
#pragma once

#include "Filters.h"
#include <vector>

namespace hb
{
//==============================================================================
/** Plosive reducer: watches the band below ~110 Hz. When a burst of low energy
    dominates the rest of the voice (a P or B hitting the capsule), that band is
    turned down quickly, then released. Ordinary voice body passes untouched. */
class PlosiveReducer
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        lowpass.c = designBiquad (FilterType::LowPass, sr, 110.0, 0.707, 0.0);
        envLow.setup (0.3f, 50.0f, sr);
        envHigh.setup (0.3f, 50.0f, sr);
        gainAttack  = msToCoeff (1.0f, sr);
        gainRelease = msToCoeff (90.0f, sr);
        reset();
    }

    void reset() { lowpass.reset(); envLow.reset(); envHigh.reset(); gain = 1.0f; }

    /** amount: 0 (off) .. 10 (strong). */
    void setAmount (float a) noexcept { amount = std::min (10.0f, std::max (0.0f, a)); }

    float process (float x) noexcept
    {
        const float low  = lowpass.process (x);
        const float high = x - low;
        const float eL = envLow.process (low);
        const float eH = envHigh.process (high);

        float target = 1.0f;
        if (amount > 0.0f)
        {
            const float thrDb     = -14.0f - 2.0f * amount;   // -16 .. -34 dBFS
            const float dominance = 1.5f - 0.05f * amount;    // low band must beat the rest by this ratio
            const float maxDepth  = 2.4f * amount;            // up to 24 dB
            const float levelDb   = gainToDb (eL);
            if (levelDb > thrDb && eL > eH * dominance)
                target = dbToGain (-std::min (maxDepth, (levelDb - thrDb) * 0.9f));
        }

        const float c = target < gain ? gainAttack : gainRelease;
        gain = target + c * (gain - target);
        return high + gain * low;
    }

    float getReductionDb() const noexcept { return -gainToDb (gain); }

private:
    double sampleRate = 48000.0;
    Biquad lowpass;
    EnvelopeFollower envLow, envHigh;
    float gain = 1.0f, gainAttack = 0, gainRelease = 0, amount = 5.0f;
};

//==============================================================================
/** Downward expander / gate with range, hold and hysteresis.
    The detector is high-passed at 150 Hz so rumble and desk thumps don't open it. */
class Gate
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        keyFilter.c = designBiquad (FilterType::HighPass, sr, 150.0, 0.707, 0.0);
        detector.setup (0.5f, 40.0f, sr);
        openCoeff = msToCoeff (1.0f, sr);
        holdSamples = (int) (0.04 * sr);
        setParams (enabled, thresholdDb, releaseMs, rangeDb);
        reset();
    }

    void reset() { keyFilter.reset(); detector.reset(); gain = 1.0f; open = true; holdCounter = 0; }

    void setParams (bool on, float thrDb, float relMs, float rangeDbIn)
    {
        enabled = on; thresholdDb = thrDb; releaseMs = relMs; rangeDb = rangeDbIn;
        closeCoeff = msToCoeff (std::max (5.0f, relMs), sampleRate);
        rangeGain  = dbToGain (-std::max (0.0f, rangeDb));
    }

    float process (float x) noexcept
    {
        const float env = detector.process (keyFilter.process (x));
        if (! enabled)
            return x;

        const float envDb = gainToDb (env);
        if (envDb > thresholdDb)
        {
            open = true;
            holdCounter = holdSamples;
        }
        else if (envDb < thresholdDb - 4.0f)   // 4 dB hysteresis
        {
            if (holdCounter > 0) --holdCounter;
            else open = false;
        }

        const float target = open ? 1.0f : rangeGain;
        const float c = target > gain ? openCoeff : closeCoeff;
        gain = target + c * (gain - target);
        return x * gain;
    }

    float getReductionDb() const noexcept { return -gainToDb (gain); }

private:
    double sampleRate = 48000.0;
    Biquad keyFilter;
    EnvelopeFollower detector;
    bool enabled = true, open = true;
    float thresholdDb = -55.0f, releaseMs = 150.0f, rangeDb = 18.0f;
    float gain = 1.0f, openCoeff = 0, closeCoeff = 0, rangeGain = 0.125f;
    int holdSamples = 0, holdCounter = 0;
};

//==============================================================================
/** Two-character vocal compressor.
    Smooth  (opto-style): RMS detection, wide soft knee, program-dependent release.
    Punch   (FET-style):  peak detection, fast attack, firmer knee, light harmonic edge. */
class Compressor
{
public:
    enum class Mode { Smooth = 0, Punch = 1 };

    struct Settings
    {
        Mode  mode = Mode::Smooth;
        float amount = 4.0f;       // 0..10, beginner control: sets threshold + auto makeup
        bool  manual = false;      // advanced: use the knobs below instead of mode defaults
        float ratio = 4.0f, attackMs = 5.0f, releaseMs = 150.0f;
        float makeupDb = 0.0f;     // added on top of auto makeup
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        rmsCoeff = msToCoeff (5.0f, sr);
        memoryCoeff = msToCoeff (1200.0f, sr);
        set (settings);
        reset();
    }

    void reset() { rms = 0.0f; grDb = 0.0f; memory = 0.0f; counter = 0; }

    void set (const Settings& s)
    {
        settings = s;
        const bool smooth = s.mode == Mode::Smooth;

        ratio     = s.manual ? s.ratio     : (smooth ? 3.0f  : 4.0f);
        attackMs  = s.manual ? s.attackMs  : (smooth ? 10.0f : 0.5f);
        releaseMs = s.manual ? s.releaseMs : (smooth ? 250.0f : 90.0f);
        kneeDb    = smooth ? 10.0f : 3.0f;

        const float amt = std::min (10.0f, std::max (0.0f, s.amount));
        thresholdDb = -3.0f * amt;                                     // 0 .. -30 dB
        const float autoMakeup = (1.0f - 1.0f / ratio) * (-thresholdDb) * 0.45f;
        makeupGain = dbToGain (autoMakeup + s.makeupDb);

        attackCoeff = msToCoeff (attackMs, sampleRate);
        releaseCoeff = msToCoeff (releaseMs, sampleRate);
    }

    float process (float x) noexcept
    {
        const bool smooth = settings.mode == Mode::Smooth;

        float levelDb;
        if (smooth)
        {
            rms = x * x + rmsCoeff * (rms - x * x);
            levelDb = 10.0f * std::log10 (std::max (rms, 1.0e-12f)) + 3.0f; // +3: RMS of a sine ~ peak
        }
        else
        {
            levelDb = gainToDb (std::fabs (x));
        }

        // static curve with soft knee
        const float over = levelDb - thresholdDb;
        const float slope = 1.0f - 1.0f / ratio;
        float target = 0.0f;
        if (2.0f * over <= -kneeDb)          target = 0.0f;
        else if (2.0f * std::fabs (over) < kneeDb)
            target = slope * (over + kneeDb * 0.5f) * (over + kneeDb * 0.5f) / (2.0f * kneeDb);
        else                                  target = slope * over;

        // program-dependent release for the opto mode: long, heavy reduction releases slower
        if (smooth && ++counter >= 32)
        {
            counter = 0;
            const float relMs = releaseMs * (0.3f + 2.2f * memory);
            releaseCoeff = msToCoeff (relMs, sampleRate);
        }

        const float c = target > grDb ? attackCoeff : releaseCoeff;
        grDb = target + c * (grDb - target);
        memory = clamp01 (grDb / 10.0f) + memoryCoeff * (memory - clamp01 (grDb / 10.0f));

        float y = x * dbToGain (-grDb) * makeupGain;

        if (! smooth)   // FET-style edge, grows with gain reduction
        {
            const float a = std::min (grDb / 12.0f, 1.0f) * 0.15f;
            y = (1.0f - a) * y + a * std::tanh (y * 1.5f) / 1.5f;
        }
        return y;
    }

    float getReductionDb() const noexcept { return grDb; }

private:
    double sampleRate = 48000.0;
    Settings settings;
    float ratio = 3, attackMs = 10, releaseMs = 250, kneeDb = 10, thresholdDb = -12, makeupGain = 1;
    float attackCoeff = 0, releaseCoeff = 0, rmsCoeff = 0, memoryCoeff = 0;
    float rms = 0, grDb = 0, memory = 0;
    int counter = 0;
};

//==============================================================================
/** De-esser: detects energy in a band around the S frequency and dips a wide bell
    over the sibilance range only while S sounds are present. */
class DeEsser
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        env.setup (0.5f, 40.0f, sr);
        gainAttack  = msToCoeff (0.5f, sr);
        gainRelease = msToCoeff (60.0f, sr);
        setParams (amount, frequency);
        reset();
    }

    void reset() { detect.reset(); reduce.reset(); env.reset(); gain = 1.0f; }

    void setParams (float amt, float freq)
    {
        amount = std::min (10.0f, std::max (0.0f, amt));
        frequency = freq;
        detect.setup (sampleRate, freq, 1.4);
        reduce.setup (sampleRate, freq * 1.1, 0.6);   // wide bell: covers the whole S band
    }

    float process (float x) noexcept
    {
        const float band = detect.process (x).bpNorm;
        const float sBand = reduce.process (x).bpNorm;
        const float e = env.process (band);

        float target = 1.0f;
        if (amount > 0.0f)
        {
            const float thrDb = -8.0f - 3.0f * amount;           // -11 .. -38 dBFS
            const float overDb = gainToDb (e) - thrDb;
            if (overDb > 0.0f)
                target = dbToGain (-std::min (overDb * 0.75f, 3.0f + 1.2f * amount));
        }
        const float c = target < gain ? gainAttack : gainRelease;
        gain = target + c * (gain - target);
        return x + (gain - 1.0f) * sBand;   // dynamic bell cut, exact depth at the centre
    }

    float getReductionDb() const noexcept { return -gainToDb (gain); }

private:
    double sampleRate = 48000.0;
    SVF detect, reduce;
    EnvelopeFollower env;
    float amount = 4.0f, frequency = 6500.0f, gain = 1.0f, gainAttack = 0, gainRelease = 0;
};

//==============================================================================
/** Stereo-linked brickwall limiter.
    With lookahead L the gain is computed as: sliding minimum over L samples ->
    release smoothing -> L-sample moving average, and the audio is delayed by L-1
    samples. That guarantees the gain has reached its target when the peak arrives.
    With L = 1 (low latency mode) it is an instant-attack limiter with zero delay. */
class Limiter
{
public:
    void prepare (double sr, int lookaheadSamples)
    {
        sampleRate = sr;
        L = std::max (1, lookaheadSamples);
        delayL.assign ((size_t) L, 0.0f);
        delayR.assign ((size_t) L, 0.0f);
        box.assign ((size_t) L, 1.0f);
        dqVal.assign ((size_t) L + 1, 1.0f);
        dqIdx.assign ((size_t) L + 1, 0);
        releaseCoeff = msToCoeff (120.0f, sr);
        reset();
    }

    void reset()
    {
        std::fill (delayL.begin(), delayL.end(), 0.0f);
        std::fill (delayR.begin(), delayR.end(), 0.0f);
        std::fill (box.begin(), box.end(), 1.0f);
        boxSum = (double) L;
        pos = boxPos = 0;
        head = tail = count = 0;
        n = 0;
        relState = 1.0f;
        lastGain = 1.0f;
    }

    int getLatency() const noexcept { return L - 1; }
    void setCeilingDb (float db) noexcept { ceiling = dbToGain (db); }

    void process (float& l, float& r) noexcept
    {
        const float peak = std::max (std::fabs (l), std::fabs (r));
        const float req = peak > ceiling ? ceiling / peak : 1.0f;

        const float m = slidingMin (req);
        relState = m < relState ? m : m + releaseCoeff * (relState - m);

        boxSum += (double) relState - (double) box[(size_t) boxPos];
        box[(size_t) boxPos] = relState;
        boxPos = (boxPos + 1) % L;
        const float g = (float) (boxSum / (double) L);
        lastGain = g;

        delayL[(size_t) pos] = l;
        delayR[(size_t) pos] = r;
        const int readPos = (pos + 1) % L;   // oldest sample: L-1 samples ago
        pos = readPos;

        l = std::min (ceiling, std::max (-ceiling, delayL[(size_t) readPos] * g));
        r = std::min (ceiling, std::max (-ceiling, delayR[(size_t) readPos] * g));
    }

    float getReductionDb() const noexcept { return -gainToDb (lastGain); }

private:
    float slidingMin (float v) noexcept
    {
        const int cap = L + 1;
        while (count > 0)
        {
            const int back = (tail - 1 + cap) % cap;
            if (dqVal[(size_t) back] >= v) { tail = back; --count; }
            else break;
        }
        dqVal[(size_t) tail] = v;
        dqIdx[(size_t) tail] = n;
        tail = (tail + 1) % cap;
        ++count;

        while (count > 0 && dqIdx[(size_t) head] <= n - L)
        {
            head = (head + 1) % cap;
            --count;
        }
        ++n;
        return dqVal[(size_t) head];
    }

    double sampleRate = 48000.0;
    int L = 1, pos = 0, boxPos = 0;
    std::vector<float> delayL, delayR, box, dqVal;
    std::vector<int64_t> dqIdx;
    int head = 0, tail = 0, count = 0;
    int64_t n = 0;
    double boxSum = 1.0;
    float relState = 1.0f, releaseCoeff = 0.0f, ceiling = 0.891f, lastGain = 1.0f;
};

} // namespace hb
