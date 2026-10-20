# SubXtend

A simple psychoacoustic bass enhancer plugin (VST3 / AU / Standalone) in the spirit of BassXtend.

Instead of boosting the sub (which eats headroom fast), SubXtend **generates harmonics** of the
low end (2×, 3×, 4×, 5× the fundamental). Your ear reconstructs the "missing fundamental" from
those harmonics, so the bass sounds fuller and translates to small speakers, while the actual
sub energy stays the same or can be turned **down**.

## Controls

| Control | Range | What it does |
|---|---|---|
| **Root** | 32–256 Hz | Crossover. Everything below it is the "low band" that harmonics are generated from. Set it just above your bass/kick fundamental. |
| **Intensity** | −24…+24 dB | Level of the generated harmonics. 0 dB is loudness-matched to the original low band. |
| **Sub** | Off…0 dB | Level of the original low band. **This is the headroom knob**: pull it down a few dB and use Intensity to keep the perceived bass. |
| **Gain Trim** | −30…0 dB | Output trim for A/B level matching. |
| **Low Band In/Out** | toggle | Out = hear only the generated harmonics, for setting Root/Intensity. |
| **Meters / Headroom** | — | Peak in/out, and how many dB of peak level you've gained (click to reset). |

Zero latency, no oversampling. Harmonics are band-limited to roughly 1–5.5× Root, so aliasing
isn't an issue.

## Quick workflow for headroom problems

1. Put SubXtend on the bass (or kick, or the low-end bus).
2. Set **Root** a bit above the fundamental (e.g. 60–100 Hz for most bass).
3. Flip **Low Band Out** briefly to hear what the harmonics sound like; adjust Root to taste.
4. Flip it back **In**, then lower **Sub** by 3–6 dB and raise **Intensity** until the bass
   sounds as big as before. Watch the **Headroom** readout go positive.

## Building

Requires CMake ≥ 3.22 and a C++17 compiler (Visual Studio 2022 on Windows, Xcode on macOS).
JUCE is downloaded automatically.

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Output is in `build/SubXtend_artefacts/Release/`. Copy `SubXtend.vst3` to:
- Windows: `C:\Program Files\Common Files\VST3\`
- macOS: `~/Library/Audio/Plug-Ins/VST3/` (and `SubXtend.component` to `~/Library/Audio/Plug-Ins/Components/`)

Or push this repo to GitHub: the included workflow (`.github/workflows/build.yml`) builds
Windows, macOS and Linux versions and attaches them as downloadable artifacts.

## Code layout

- `Source/BassEngine.h` – all the DSP (crossover, harmonic generator, loudness matching). No JUCE dependency.
- `Source/PluginProcessor.*` – parameters and host glue.
- `Source/PluginEditor.*` – UI.
- `tests/engine_test.cpp` – DSP sanity tests (`ctest --test-dir build`).
