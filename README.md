# Detroit Hallelujah Blue + Hallelujah Duck

Two plug-ins for recording hip hop vocals in an untreated home studio. Each builds as VST3 on macOS and Windows, plus AU on macOS.

| Plug-in | Goes on | Does |
|---|---|---|
| **Detroit Hallelujah Blue** | your vocal track | Runs the whole vocal chain in one window: mic voicing, room cleanup, preamp, EQ, compressor, effects, limiter. There is also a standalone app. |
| **Hallelujah Duck** | your beat (or instrumental bus) | Listens to your vocal on its sidechain input. It dips only the frequencies of the beat that your voice is using, and only while you're rapping. |

---

## Getting the Mac and Windows installers

### Option A: let GitHub build them (no compilers needed)
1. Create a GitHub repository and push this folder to it.
2. Open the **Actions** tab. The **Build plug-ins** workflow runs on every push. It builds macOS (Apple Silicon and Intel in one binary) and Windows x64, then runs the DSP tests.
3. When the run finishes, download the **DetroitHallelujah-macOS** and **DetroitHallelujah-Windows** artifacts:
   - macOS: `DetroitHallelujah-1.0.0-macOS-Installer.pkg`, plus a zip of the raw bundles
   - Windows: `DetroitHallelujah-1.0.0-Windows-Installer.exe`, plus a zip of the raw bundles
4. Push a tag such as `v1.0.0` and the workflow also creates a GitHub Release with those files attached.

### Option B: build on your own machine
**Mac:** install Xcode (or the Command Line Tools) and CMake (`brew install cmake`), then run:
```bash
bash scripts/build_mac.sh
```
**Windows:** install Visual Studio 2022 or later with "Desktop development with C++", then run:
```powershell
powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1
```
The first build downloads JUCE (version 8.0.12, set with `-DHB_JUCE_TAG=...`). Installers and zips end up in `dist/`.

### Installing by hand
| | macOS | Windows |
|---|---|---|
| VST3 | `/Library/Audio/Plug-Ins/VST3/` | `C:\Program Files\Common Files\VST3\` |
| AU | `/Library/Audio/Plug-Ins/Components/` | n/a |

**Unsigned builds:** these builds are not signed with an Apple Developer ID or a Windows code-signing certificate. On a Mac, right-click the .pkg and choose **Open**. If a DAW refuses the plug-in, run:
`xattr -dr com.apple.quarantine "/Library/Audio/Plug-Ins/Components/Detroit Hallelujah Blue.component"`.
On Windows, click **More info → Run anyway**. To ship to the public, sign and notarize (macOS) and sign (Windows).

---

## Detroit Hallelujah Blue: signal flow

`Input → Plosive → Mic voicing → Gate → Preamp → EQ → Compressor → De-esser → FX → Output → Limiter`

This order differs from the original spec on purpose:
- The plosive reducer comes first, so pops never hit the gate or the preamp.
- The gate's detector ignores everything below 150 Hz, so rumble can't open it.
- The de-esser comes after the compressor, because compression brings S sounds back up.

| Section | Simple view | Advanced view adds |
|---|---|---|
| **Mic: C800G-style** | Your Mic (budget-mic correction), Input, Air, Presence, Warmth | Mic Model (bypass), Proximity |
| **Room Cleanup** | Gate threshold, De-Ess, Plosive | Gate on/off, Release, Range, S frequency |
| **Preamp** | Flavor (Clean / Warm / Gritty), Drive | |
| **EQ** | High-pass (Off/80/120 Hz), Box Cut, Presence, Air + one-click **Dark / Bright / Melodic** | Box Cut and Presence frequencies |
| **Compressor** | Mode (Smooth/Opto or Punch/FET), Amount | Manual controls: Ratio, Attack, Release, Makeup |
| **Vocal FX** | Doubler, Reverb, Delay, Width | Delay time (tempo-synced to your DAW), Feedback, Reverb Decay, Pre-Delay |
| **Mix + Output** | Sit in the Mix macro, Output | Limiter ceiling |

Also included:
- **Analyzer:** live spectrum of the processed vocal, the raw input shown faintly behind it, a gold EQ curve on top, and a Freeze button.
- **Meters:** input, compressor gain reduction and output, each with peak hold (click a meter to reset it). There are also readouts for de-esser, gate, plosive and limiter reduction, plus current latency.
- **Presets:** C800G-style (Default), Dark and Heavy, Bright and Crisp, Melodic, Ad-lib, USB Condenser Fix, Entry XLR Condenser Fix and Dynamic Mic Lift. You can save and delete your own. A/B compare and Copy are included.
- **Mode:**
  - Low Latency (Tracking) has zero added latency.
  - High Quality (Mixing) adds 2x-oversampled saturation and a 3 ms lookahead limiter. That's 166 samples at 48 kHz, and the DAW compensates for it automatically.
- **Effects work like sends:** the dry vocal stays centred at full level. The Mix knobs add the doubler, reverb and delay on top, and Width spreads only those effects.

## Hallelujah Duck: setup

1. Put **Hallelujah Duck** on the beat track or instrumental bus.
2. Send the vocal to the plug-in's **sidechain** input:
   - **FL Studio:** select the vocal's mixer track, right-click the routing arrow under the beat's mixer track and choose **Sidechain to this track**. Then assign that sidechain to the plug-in's sidechain input in the plug-in wrapper's routing settings.
   - **Ableton Live:** open the plug-in's device, enable **Sidechain** and choose the vocal track.
   - **Logic Pro:** open the plug-in window → **Side Chain** menu at the top right → choose the vocal track.
   - **Studio One / Reaper / Cubase:** use the plug-in's sidechain input or pin (Reaper: route the vocal to channels 3/4).
3. Start with Depth at 3 to 6 dB. The display shows each band dipping as you rap. **Mid Only** (the default) leaves the beat's stereo sides untouched.

---

## Project layout
```
shared/dsp/     All audio processing in plain C++17 (no JUCE), unit-tested
shared/ui/      Look and feel, knobs, meters, presets, A/B (JUCE)
plugins/HallelujahBlue/   Vocal channel plug-in
plugins/HallelujahDuck/   Sidechain ducker plug-in
tests/          DSP tests (run with ctest)
scripts/        Build and packaging scripts, Windows installer script
.github/workflows/build.yml   macOS + Windows CI
```

## Not in version 1
- **De-reverb:** spectral de-reverb is the hardest feature to do well. Until then, the Gate's Range control works as a gentle downward expander that tames room tails.
- **Reference tab:** load a commercial track and compare spectra.
- **More mic models** (U87-style, MA-300-style). The Mic Model menu is already in place for them.
- **AAX (Pro Tools):** join Avid's developer program, get the AAX SDK, then configure with `-DHB_AAX_SDK_PATH=/path/to/aax-sdk`. Release builds also need PACE signing.

## Legal notes
- "C800G-style", "Opto" and "FET" describe a sound. Avoid using Sony's, Universal Audio's, Neumann's or Mojave's trademarks or logos in the interface or in marketing.
- JUCE is dual-licensed (AGPLv3 or a commercial licence). Before selling closed-source plug-ins, check JUCE's current licence tiers. The VST3 SDK, and the AAX SDK if you use it, have their own terms.
