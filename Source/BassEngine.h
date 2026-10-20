#pragma once

// SubXtend DSP engine — psychoacoustic bass extension.
//
// Signal flow (per channel, zero latency, all IIR):
//
//   in ──► LR4 crossover @ Root ──► high band ───────────────────────────────┐
//                │                                                         (+)──► × Trim ──► out
//                └─► low band ──┬──► × Sub level (× Low Band on/off) ─────────┤
//                               └──► harmonic generator ──► band-limit ──► × loudness match × Intensity
//
// The harmonic generator normalises the low band by a fast peak envelope and
// feeds it through a weighted sum of Chebyshev polynomials (T2..T5). For a
// sine of frequency f this yields exactly 2f, 3f, 4f, 5f, whose pitch the ear
// hears as f ("missing fundamental"). The result is re-scaled by the envelope,
// so the harmonics track the dynamics of the original bass.
//
// The harmonics are band-limited to [Root, ~5×Root], and a loudness-weighted
// RMS match keeps "Intensity = 0 dB" perceptually equal to the original low
// band. That's what saves headroom: you can pull the Sub down and let the
// harmonics carry the bass, instead of pushing more sub energy into the mix.

#include <algorithm>
#include <cmath>
#include <vector>

namespace subx
{

constexpr double kPi = 3.14159265358979323846;

inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }

//==============================================================================
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    // RBJ cookbook
    void setLowpass (double fc, double q, double fs)  { set (fc, q, fs, false); }
    void setHighpass (double fc, double q, double fs) { set (fc, q, fs, true); }

    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

private:
    void set (double fc, double q, double fs, bool highpass)
    {
        fc = std::clamp (fc, 5.0, 0.45 * fs);
        const double w0 = 2.0 * kPi * fc / fs;
        const double cw = std::cos (w0), alpha = std::sin (w0) / (2.0 * q);
        const double a0 = 1.0 + alpha;
        const double k  = highpass ? (1.0 + cw) * 0.5 : (1.0 - cw) * 0.5;

        b0 = (float) (k / a0);
        b1 = (float) ((highpass ? -2.0 * k : 2.0 * k) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * cw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
};

// 4th-order Linkwitz-Riley = two cascaded 2nd-order Butterworths.
// LP + HP of the same cutoff sum to a flat (allpass) response.
struct LR4
{
    Biquad s1, s2;

    void reset() { s1.reset(); s2.reset(); }
    void setLowpass (double fc, double fs)  { s1.setLowpass (fc, 0.70710678, fs);  s2.setLowpass (fc, 0.70710678, fs); }
    void setHighpass (double fc, double fs) { s1.setHighpass (fc, 0.70710678, fs); s2.setHighpass (fc, 0.70710678, fs); }
    inline float process (float x) { return s2.process (s1.process (x)); }
};

struct OnePole
{
    float coeff = 0, state = 0;
    void setTime (double seconds, double fs) { coeff = (float) std::exp (-1.0 / (std::max (seconds, 1.0e-5) * fs)); }
    inline float process (float target) { return state = target + coeff * (state - target); }
};

//==============================================================================
struct Params
{
    float rootHz      = 80.0f;   // crossover / lowest generated harmonic   (32 .. 256 Hz)
    float intensityDb = 0.0f;    // harmonics level vs. original low band  (-24 .. +24 dB)
    float subDb       = 0.0f;    // original low band level                (-30 .. 0 dB, -30 = off)
    float trimDb      = 0.0f;    // output trim                            (-30 .. 0 dB)
    bool  lowBandOn   = true;    // false = solo the harmonics (audition)
};

//==============================================================================
class BassEngine
{
public:
    static constexpr float kSubOffDb = -30.0f;

    void prepare (double sampleRate, int numChannels)
    {
        fs = sampleRate;
        chans.assign ((size_t) std::max (1, numChannels), Channel{});

        envRelease = (float) std::exp (-1.0 / (0.030 * fs));   // 30 ms peak-envelope release
        msLow.setTime (0.150, fs);
        msHarm.setTime (0.150, fs);
        matchSmooth.setTime (0.050, fs);
        for (auto* s : { &intensitySmooth, &subSmooth, &trimSmooth })
            s->setTime (0.020, fs);
        rootSmooth.setTime (0.050, fs);

        reset();
    }

    void reset()
    {
        for (auto& c : chans) c.reset();
        msLow.state = msHarm.state = 0.0f;
        matchSmooth.state = 1.0f;
        intensitySmooth.state = dbToGain (params.intensityDb);
        subSmooth.state       = targetSubGain();
        trimSmooth.state      = dbToGain (params.trimDb);
        rootSmooth.state      = params.rootHz;
        updateFilters (params.rootHz);
    }

    void setParams (const Params& p) { params = p; }

    // In-place processing of non-interleaved buffers.
    void process (float* const* data, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, (int) chans.size());
        if (numChannels <= 0) return;

        const float intensityTarget = dbToGain (params.intensityDb);
        const float subTarget       = targetSubGain();
        const float trimTarget      = dbToGain (params.trimDb);
        const float invCh           = 1.0f / (float) numChannels;

        for (int i = 0; i < numSamples; ++i)
        {
            // Glide the crossover and refresh coefficients every 16 samples while it moves.
            const float root = rootSmooth.process (params.rootHz);
            if ((i & 15) == 0 && std::abs (root - currentRoot) > 0.01f)
                updateFilters (root);

            const float intensity = intensitySmooth.process (intensityTarget);
            const float sub       = subSmooth.process (subTarget);
            const float trim      = trimSmooth.process (trimTarget);
            const float match     = matchSmooth.state;

            float lowEnergy = 0.0f, harmEnergy = 0.0f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& c = chans[(size_t) ch];
                const float x = data[ch][i];

                const float low  = c.xoLow.process (x);
                const float high = c.xoHigh.process (x);

                const float harm = c.harmBand (generateHarmonics (c, low));

                // Loudness-weighted measurement of both signals (rough equal-loudness tilt).
                const float wl = c.weightLow.process (low);
                const float wh = c.weightHarm.process (harm);
                lowEnergy  += wl * wl;
                harmEnergy += wh * wh;

                data[ch][i] = trim * (high + sub * low + intensity * match * harm);
            }

            // Linked across channels so the stereo image doesn't shift.
            const float ml = msLow.process (lowEnergy * invCh);
            const float mh = msHarm.process (harmEnergy * invCh);
            const float target = mh > 1.0e-12f ? std::min (std::sqrt (ml / mh), kMaxMatch) : matchSmooth.state;
            matchSmooth.process (target);
        }
    }

private:
    struct Channel
    {
        LR4 xoLow, xoHigh;      // main crossover
        LR4 harmHP;             // remove DC / anything below Root from the harmonics
        LR4 harmLP;             // keep harmonics in the bass register (no fizz)
        Biquad weightLow, weightHarm;
        float env = 0.0f;

        float harmBand (float x) { return harmLP.process (harmHP.process (x)); }

        void reset()
        {
            xoLow.reset(); xoHigh.reset(); harmHP.reset(); harmLP.reset();
            weightLow.reset(); weightHarm.reset();
            env = 0.0f;
        }
    };

    static constexpr float kMaxMatch = 16.0f;   // +24 dB max auto-gain

    float generateHarmonics (Channel& c, float x)
    {
        const float ax = std::abs (x);
        c.env = ax > c.env ? ax : c.env * envRelease;
        if (c.env < 1.0e-7f) return 0.0f;

        const float n  = std::clamp (x / c.env, -1.0f, 1.0f);
        const float n2 = n * n;

        // Chebyshev polynomials: Tk(cos θ) = cos kθ
        const float t2 = 2.0f * n2 - 1.0f;
        const float t3 = n * (4.0f * n2 - 3.0f);
        const float t4 = 8.0f * n2 * (n2 - 1.0f) + 1.0f;
        const float t5 = n * (16.0f * n2 * n2 - 20.0f * n2 + 5.0f);

        // Falling harmonic weights: warm, bass-like, still audible on small speakers.
        return c.env * (1.00f * t2 + 0.65f * t3 + 0.40f * t4 + 0.22f * t5);
    }

    float targetSubGain() const
    {
        if (! params.lowBandOn || params.subDb <= kSubOffDb + 0.01f) return 0.0f;
        return dbToGain (params.subDb);
    }

    void updateFilters (float root)
    {
        currentRoot = root;
        const double lp = std::min (5.5 * root, 0.4 * fs);

        for (auto& c : chans)
        {
            c.xoLow.setLowpass (root, fs);
            c.xoHigh.setHighpass (root, fs);
            c.harmHP.setHighpass (root, fs);
            c.harmLP.setLowpass (lp, fs);
            // The ear is much less sensitive at 40 Hz than at 200 Hz. Measuring both
            // paths through this tilt makes 0 dB Intensity sound roughly as loud as the original.
            c.weightLow.setHighpass (80.0, 0.5, fs);
            c.weightHarm.setHighpass (80.0, 0.5, fs);
        }
    }

    double fs = 44100.0;
    Params params;
    std::vector<Channel> chans;

    float envRelease = 0.999f;
    float currentRoot = -1.0f;
    OnePole msLow, msHarm, matchSmooth;
    OnePole intensitySmooth, subSmooth, trimSmooth, rootSmooth;
};

} // namespace subx
