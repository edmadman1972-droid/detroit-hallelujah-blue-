// DSP tests - no JUCE needed. Build: see tests/CMakeLists.txt, or
//   c++ -std=c++17 -O2 -I shared/dsp tests/dsp_tests.cpp -o dsp_tests && ./dsp_tests
#include "VocalChain.h"
#include "DuckEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace hb;

static int failures = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf ("  ok    "); } else { std::printf ("  FAIL  "); ++failures; } \
                              std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static constexpr double SR = 48000.0;

static float rms (const std::vector<float>& v, size_t from = 0, size_t to = 0)
{
    if (to == 0) to = v.size();
    double s = 0; for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / double (to - from));
}

static std::vector<float> sine (double f, float amp, double seconds)
{
    std::vector<float> v ((size_t) (seconds * SR));
    for (size_t i = 0; i < v.size(); ++i) v[i] = amp * (float) std::sin (2 * kPi * f * i / SR);
    return v;
}

/** A crude "rap vocal": harmonic voice at 140 Hz with syllable envelope + breath noise. */
static std::vector<float> fakeVocal (double seconds, unsigned seed = 1)
{
    std::mt19937 rng (seed);
    std::normal_distribution<float> n (0.0f, 1.0f);
    std::vector<float> v ((size_t) (seconds * SR));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = i / SR;
        double s = 0;
        for (int h = 1; h <= 20; ++h) s += std::sin (2 * kPi * 140 * h * t) / h;
        const double syll = 0.5 + 0.5 * std::sin (2 * kPi * 4.0 * t);
        v[i] = (float) (0.15 * s * syll) + 0.01f * n (rng);
    }
    return v;
}

static bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

static float peak (const std::vector<float>& v)
{
    float p = 0; for (float x : v) p = std::max (p, std::fabs (x)); return p;
}

int main()
{
    std::printf ("Filters\n");
    {
        auto c = designBiquad (FilterType::Peak, SR, 1000, 1.0, 6.0);
        CHECK (std::fabs (c.magnitudeDb (1000, SR) - 6.0) < 0.05, "peak +6 dB at 1 kHz -> %.2f dB", c.magnitudeDb (1000, SR));
        Biquad b; b.c = c;
        auto s = sine (1000, 0.1f, 0.5);
        for (auto& x : s) x = b.process (x);
        const float measured = gainToDb (rms (s, 4800) / (0.1f / std::sqrt (2.0f)));
        CHECK (std::fabs (measured - 6.0f) < 0.1f, "measured sine gain %.2f dB", measured);
        auto hp = designBiquad (FilterType::HighPass, SR, 120, 0.707, 0);
        CHECK (hp.magnitudeDb (30, SR) < -20, "HPF 120 Hz: %.1f dB at 30 Hz", hp.magnitudeDb (30, SR));
        auto flat = designBiquad (FilterType::HighShelf, SR, 10000, 0.7, 0.0);
        CHECK (std::fabs (flat.magnitudeDb (15000, SR)) < 1e-3, "0 dB shelf is transparent");
    }

    std::printf ("Oversampler latency\n");
    {
        Oversampler2x os;
        std::vector<float> out (200);
        for (int i = 0; i < 200; ++i)
        {
            float a, b; os.upsample (i == 10 ? 1.0f : 0.0f, a, b);
            out[(size_t) i] = os.downsample (a, b);
        }
        int pk = 0; for (int i = 0; i < 200; ++i) if (std::fabs (out[(size_t) i]) > std::fabs (out[(size_t) pk])) pk = i;
        CHECK (pk - 10 == Oversampler2x::getLatency(), "impulse peak delayed %d samples (reported %d)", pk - 10, Oversampler2x::getLatency());
        Oversampler2x os2;
        auto s = sine (1000, 0.5f, 0.2);
        std::vector<float> o (s.size());
        for (size_t i = 0; i < s.size(); ++i) { float a, b; os2.upsample (s[i], a, b); o[i] = os2.downsample (a, b); }
        const float g = gainToDb (rms (o, 2000) / rms (s, 2000));
        CHECK (std::fabs (g) < 0.05f, "1 kHz passes at unity (%.3f dB)", g);
    }

    std::printf ("Limiter\n");
    for (int la : { 1, 144 })
    {
        Limiter lim; lim.prepare (SR, la); lim.setCeilingDb (-1.0f);
        auto s = sine (200, dbToGain (12.0f), 1.0);
        std::vector<float> l = s, r = s;
        for (size_t i = 0; i < s.size(); ++i) lim.process (l[i], r[i]);
        const float pk = gainToDb (peak (l));
        CHECK (pk <= -0.999f, "lookahead %d: +12 dBFS sine peaks at %.3f dBFS (ceiling -1)", la, pk);
        CHECK (lim.getLatency() == la - 1, "latency %d", lim.getLatency());
    }
    {   // lookahead limiter must not clip (output should stay sine-shaped, not squared)
        Limiter lim; lim.prepare (SR, 144); lim.setCeilingDb (-1.0f);
        std::vector<float> l (48000, 0.0f), r (48000, 0.0f);
        l[20000] = r[20000] = 2.0f; // single spike
        for (size_t i = 0; i < l.size(); ++i) lim.process (l[i], r[i]);
        CHECK (std::fabs (l[20143] - dbToGain (-1.0f)) < 0.01f, "spike lands exactly at ceiling after lookahead (%.4f)", l[20143]);
    }

    std::printf ("Plosive reducer\n");
    {
        PlosiveReducer p; p.prepare (SR); p.setAmount (6);
        // plosive: 45 Hz burst, fast decay
        std::vector<float> s ((size_t) (0.3 * SR));
        for (size_t i = 0; i < s.size(); ++i) s[i] = 0.8f * (float) std::sin (2 * kPi * 45 * i / SR) * std::exp (-(float) i / (0.08f * (float) SR));
        float maxRed = 0; for (auto& x : s) { x = p.process (x); maxRed = std::max (maxRed, p.getReductionDb()); }
        CHECK (maxRed > 6.0f, "pop burst reduced by up to %.1f dB", maxRed);

        PlosiveReducer p2; p2.prepare (SR); p2.setAmount (6);
        auto v = fakeVocal (1.0);
        float maxRed2 = 0; for (auto& x : v) { p2.process (x); maxRed2 = std::max (maxRed2, p2.getReductionDb()); }
        CHECK (maxRed2 < 1.0f, "normal voice barely touched (max %.2f dB)", maxRed2);
    }

    std::printf ("De-esser\n");
    {
        DeEsser d; d.prepare (SR); d.setParams (6, 6500);
        auto s = sine (7000, 0.4f, 0.5);
        const float before = rms (s, 4800);
        for (auto& x : s) x = d.process (x);
        const float red = gainToDb (before) - gainToDb (rms (s, 4800));
        CHECK (red > 4.0f, "loud 7 kHz 'S' reduced %.1f dB", red);

        DeEsser d2; d2.prepare (SR); d2.setParams (6, 6500);
        auto lo = sine (500, 0.4f, 0.5);
        const float b2 = rms (lo, 4800);
        for (auto& x : lo) x = d2.process (x);
        const float ch = std::fabs (gainToDb (b2) - gainToDb (rms (lo, 4800)));
        CHECK (ch < 0.5f, "500 Hz voice body unchanged (%.2f dB)", ch);
    }

    std::printf ("Gate\n");
    {
        Gate g; g.prepare (SR); g.setParams (true, -50, 100, 18);
        std::mt19937 rng (3); std::normal_distribution<float> n (0, dbToGain (-70));
        std::vector<float> s ((size_t) SR); for (auto& x : s) x = n (rng);
        const float before = rms (s, 24000);
        for (auto& x : s) x = g.process (x);
        const float red = gainToDb (before) - gainToDb (rms (s, 24000));
        CHECK (std::fabs (red - 18.0f) < 0.5f, "room noise at -70 dB pulled down %.1f dB (range 18)", red);
    }

    std::printf ("Compressor\n");
    for (int mode = 0; mode < 2; ++mode)
    {
        Compressor c; c.prepare (SR);
        Compressor::Settings st; st.mode = (Compressor::Mode) mode; st.amount = 6;
        c.set (st);
        auto s = sine (300, dbToGain (-6), 1.0);
        float gr = 0; for (auto& x : s) { x = c.process (x); gr = c.getReductionDb(); }
        CHECK (gr > 4.0f && gr < 15.0f, "%s mode: -6 dBFS tone gets %.1f dB gain reduction", mode == 0 ? "Smooth" : "Punch", gr);
        CHECK (allFinite (s), "output finite");
    }

    std::printf ("Preamp\n");
    for (int fl = 0; fl < 3; ++fl)
        for (bool os : { false, true })
        {
            Preamp p; p.prepare (SR); p.setParams ((Preamp::Flavor) fl, 6.0f, os);
            auto s = sine (220, 0.25f, 0.5);
            const float before = rms (s, 4800);
            for (auto& x : s) x = p.process (x);
            const float d = gainToDb (rms (s, 4800)) - gainToDb (before);
            CHECK (std::fabs (d) < 2.5f && allFinite (s), "flavor %d os=%d: level change %.2f dB at drive 6", fl, (int) os, d);
        }

    std::printf ("Reverb / delay\n");
    {
        FdnReverb r; r.prepare (SR); r.setParams (1.5f, 10.0f, 0.35f);
        std::vector<float> L ((size_t) (6 * SR)), R (L.size());
        for (size_t i = 0; i < L.size(); ++i) r.process (i == 0 ? 1.0f : 0.0f, L[i], R[i]);
        const float early = rms (L, (size_t) (0.1 * SR), (size_t) (0.4 * SR));
        const float late = rms (L, (size_t) (1.6 * SR), (size_t) (1.9 * SR));
        const float drop = gainToDb (early) - gainToDb (late);
        CHECK (allFinite (L) && allFinite (R), "reverb tail finite");
        CHECK (drop > 35.0f && drop < 80.0f, "tail decays %.1f dB over 1.5 s (T60 1.5 s target ~60)", drop);
        CHECK (rms (R, 4800, 48000) > 1e-4f, "reverb produces stereo output");

        TempoDelay d; d.prepare (SR); d.setParams (120.0, 1, 0.5f);
        std::vector<float> dl ((size_t) (1.2 * SR)), dr (dl.size());
        for (size_t i = 0; i < dl.size(); ++i) d.process (i == 0 ? 1.0f : 0.0f, dl[i], dr[i]);
        size_t pkL = 0, pkR = 0;
        for (size_t i = 0; i < dl.size(); ++i) { if (std::fabs (dl[i]) > std::fabs (dl[pkL])) pkL = i; if (std::fabs (dr[i]) > std::fabs (dr[pkR])) pkR = i; }
        CHECK (std::abs ((int) pkL - 24000) <= 2, "1/4 note at 120 BPM: first echo left at %zu (expect 24000)", pkL);
        CHECK (std::abs ((int) pkR - 48000) <= 4, "second echo right at %zu (expect 48000)", pkR);
    }

    std::printf ("Full vocal chain\n");
    for (bool hq : { false, true })
    {
        VocalChain chain;
        VocalParams p; p.highQuality = hq; p.reverbMix = 0.2f; p.delayMix = 0.15f; p.doublerMix = 0.4f; p.sitInMix = 5;
        chain.prepare (SR, 512);
        chain.setParams (p);
        auto v = fakeVocal (3.0);
        for (auto& x : v) x *= 2.5f;   // drive it hot
        std::vector<float> L (v.size()), R (v.size());
        for (size_t off = 0; off < v.size(); off += 512)
        {
            const int n = (int) std::min<size_t> (512, v.size() - off);
            chain.setParams (p);
            chain.process (v.data() + off, L.data() + off, R.data() + off, n);
        }
        CHECK (allFinite (L) && allFinite (R), "%s: output finite", hq ? "HQ" : "Low latency");
        CHECK (peak (L) <= dbToGain (-1.0f) + 1e-4f && peak (R) <= dbToGain (-1.0f) + 1e-4f,
               "%s: peaks %.2f / %.2f dBFS stay under -1 ceiling", hq ? "HQ" : "Low latency", gainToDb (peak (L)), gainToDb (peak (R)));
        CHECK (rms (L) > 0.01f, "%s: healthy output level %.1f dBFS RMS", hq ? "HQ" : "Low latency", gainToDb (rms (L)));
        CHECK (chain.getLatency() == (hq ? 23 + 143 : 0), "%s: latency %d samples", hq ? "HQ" : "Low latency", chain.getLatency());
        CHECK (rms (L) != rms (R), "%s: effects create stereo", hq ? "HQ" : "Low latency");
    }

    std::printf ("Static EQ curve (for the analyzer overlay)\n");
    {
        VocalParams p; p.micModel = MicModel::Bypass; p.highPass = HighPass::Off; p.lowMidGainDb = 0; p.presenceGainDb = 0; p.airGainDb = 0;
        auto f = VocalChain::designStaticFilters (p, SR);
        CHECK (std::fabs (VocalChain::responseDb (f, SR, 1000)) < 0.01f, "all-neutral settings give a flat curve");
        VocalParams q; q.air = 10;
        auto g = VocalChain::designStaticFilters (q, SR);
        CHECK (VocalChain::responseDb (g, SR, 16000) > 9.0f, "Air 10 lifts 16 kHz by %.1f dB", VocalChain::responseDb (g, SR, 16000));
    }

    std::printf ("Duck engine\n");
    {
        auto runDuck = [] (bool withVocal, float& bandRed1350, float& levelChangeDb)
        {
            DuckEngine d; DuckParams p; p.depthDb = 6; p.midOnly = false;
            d.prepare (SR); d.setParams (p);
            std::mt19937 rng (7); std::normal_distribution<float> n (0, 0.1f);
            std::vector<float> L ((size_t) SR), R (L.size());
            for (size_t i = 0; i < L.size(); ++i) { L[i] = n (rng); R[i] = n (rng); }
            auto vox = sine (1350, withVocal ? 0.3f : 0.0f, 1.0);
            auto dryL = L;
            d.process (L.data(), R.data(), vox.data(), nullptr, (int) L.size());
            bandRed1350 = d.bandReductionDb[4].load();
            // measure the beat's 1350 Hz band before / after
            SVF a, b; a.setup (SR, 1350, 4); b.setup (SR, 1350, 4);
            std::vector<float> x1 (L.size()), x2 (L.size());
            for (size_t i = 0; i < L.size(); ++i) { x1[i] = a.process (dryL[i]).bpNorm; x2[i] = b.process (L[i]).bpNorm; }
            levelChangeDb = gainToDb (rms (x2, 24000)) - gainToDb (rms (x1, 24000));
        };
        float red, change;
        runDuck (false, red, change);
        CHECK (red < 0.01f && std::fabs (change) < 0.05f, "silent sidechain: beat untouched (%.2f dB)", change);
        runDuck (true, red, change);
        CHECK (red > 5.0f && change < -4.0f, "vocal at 1.35 kHz dips that band of the beat %.1f dB (band GR %.1f)", change, red);
    }

    std::printf ("Performance\n");
    {
        VocalChain chain; VocalParams p; p.highQuality = true; p.reverbMix = 0.2f; p.delayMix = 0.2f; p.doublerMix = 0.3f;
        chain.prepare (SR, 512);
        auto v = fakeVocal (10.0);
        std::vector<float> L (v.size()), R (v.size());
        const auto t0 = std::chrono::steady_clock::now();
        for (size_t off = 0; off + 512 <= v.size(); off += 512) { chain.setParams (p); chain.process (v.data() + off, L.data() + off, R.data() + off, 512); }
        const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        CHECK (s < 1.0, "10 s of audio (HQ, all FX) processed in %.3f s = %.1f%% of one core", s, s * 10.0);
    }

    std::printf (failures == 0 ? "\nALL TESTS PASSED\n" : "\n%d TEST(S) FAILED\n", failures);
    return failures == 0 ? 0 : 1;
}
