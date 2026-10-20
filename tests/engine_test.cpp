// Standalone sanity test for the DSP engine (no JUCE needed):
//   g++ -O2 -std=c++17 -I Source tests/engine_test.cpp -o engine_test && ./engine_test
#include "BassEngine.h"
#include <cstdio>

using namespace subx;

static constexpr double fs = 48000.0;

static std::vector<float> render (const Params& p, std::vector<float> sig)
{
    BassEngine e;
    e.setParams (p);
    e.prepare (fs, 1);
    for (size_t pos = 0; pos < sig.size(); pos += 512)
    {
        float* d = sig.data() + pos;
        e.process (&d, 1, (int) std::min<size_t> (512, sig.size() - pos));
    }
    return sig;
}

static double peakDb (const std::vector<float>& s, size_t from)
{
    float pk = 0;
    for (size_t i = from; i < s.size(); ++i) pk = std::max (pk, std::abs (s[i]));
    return 20.0 * std::log10 (pk + 1e-12);
}

// Amplitude of frequency f via correlation (Goertzel-style)
static double toneDb (const std::vector<float>& s, double f, size_t from)
{
    double re = 0, im = 0;
    for (size_t i = from; i < s.size(); ++i)
    {
        re += s[i] * std::cos (2 * kPi * f * (double) i / fs);
        im += s[i] * std::sin (2 * kPi * f * (double) i / fs);
    }
    const double amp = 2.0 * std::sqrt (re * re + im * im) / (double) (s.size() - from);
    return 20.0 * std::log10 (amp + 1e-12);
}

int main()
{
    const size_t n = (size_t) fs * 2, settle = (size_t) fs;  // measure the 2nd second
    int failures = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FAIL", what); failures += ok ? 0 : 1; };

    // 50 Hz sine at -6 dBFS + a 1 kHz tone
    std::vector<float> in (n);
    for (size_t i = 0; i < n; ++i)
        in[i] = 0.5f * (float) std::sin (2 * kPi * 50 * (double) i / fs) + 0.1f * (float) std::sin (2 * kPi * 1000 * (double) i / fs);

    Params p;  p.rootHz = 60;

    // 1. Harmonics fully off → LR4 crossover sums flat
    {
        Params q = p;  q.intensityDb = -200;
        auto out = render (q, in);
        std::printf ("Crossover only:  50 Hz %.2f dB, 1 kHz %.2f dB (in: %.2f / %.2f)\n",
                     toneDb (out, 50, settle), toneDb (out, 1000, settle), toneDb (in, 50, settle), toneDb (in, 1000, settle));
        check (std::abs (toneDb (out, 50, settle) - toneDb (in, 50, settle)) < 0.2, "crossover is transparent at 50 Hz");
        check (std::abs (toneDb (out, 1000, settle) - toneDb (in, 1000, settle)) < 0.2, "crossover is transparent at 1 kHz");
    }

    // 2. Default settings → harmonics appear at 100/150/200/250 Hz
    {
        auto out = render (p, in);
        std::printf ("Default:  ");
        for (double f : { 50.0, 100.0, 150.0, 200.0, 250.0, 300.0 }) std::printf ("%g Hz %.1f dB  ", f, toneDb (out, f, settle));
        std::printf ("\n");
        check (toneDb (out, 100, settle) > -30, "2nd harmonic generated");
        check (toneDb (out, 150, settle) > -40, "3rd harmonic generated");
    }

    // 3. Headroom: replace sub with harmonics
    {
        Params q = p;  q.subDb = BassEngine::kSubOffDb;  q.intensityDb = 0;
        auto out = render (q, in);
        std::printf ("Sub off + harmonics: peak %.2f dBFS (input %.2f dBFS)\n", peakDb (out, settle), peakDb (in, settle));
        check (peakDb (out, settle) < peakDb (in, settle) - 1.0, "peak level reduced (headroom saved)");
    }

    // 4. Low Band off → solo harmonics, no 50 Hz
    {
        Params q = p;  q.lowBandOn = false;  q.rootHz = 150;
        auto out = render (q, in);
        check (toneDb (out, 50, settle) < toneDb (in, 50, settle) - 20, "Low Band off removes the fundamental");
    }

    // 5. Silence stays silent, no NaNs
    {
        auto out = render (p, std::vector<float> (n, 0.0f));
        bool clean = true;
        for (float v : out) clean &= std::isfinite (v) && std::abs (v) < 1e-6f;
        check (clean, "silence in → silence out");
    }

    std::printf (failures ? "\n%d FAILURE(S)\n" : "\nAll tests passed\n", failures);
    return failures ? 1 : 0;
}
