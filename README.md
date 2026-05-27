# NeonForge

**Clone any Audio Unit plugin by sound alone.**

NeonForge hosts a target AU, fires an exponential sweep through it, and derives a parametric clone (EQ, dynamics, saturation) that runs as its own Audio Unit in Logic Pro. No source code, no parameter API — just the audio.

![State](https://img.shields.io/badge/macOS-Logic%20Pro%20AU-blue) ![License](https://img.shields.io/badge/license-MIT-green)

---

## What It Does

| Tier | Captured | Output |
|------|----------|--------|
| **Linear** | Sweep → IR | Convolution (EQ, reverb, tone) |
| **Dynamics** | Step response + static curve | Derived threshold, ratio, attack, release |
| **Nonlinear** | Harmonic distortion profile | Parametric saturator |

The clone presents real knobs — threshold, ratio, EQ freq/gain/Q, drive — that your friend can keep tweaking like any native plugin.

---

## One-Click Install (Mac)

1. Download this repo as a ZIP
2. Unzip and double-click **`Install NeonForge.command`**
3. If prompted, install **Xcode Command Line Tools** and **CMake** (one-time)
4. The script builds the AU and drops it into:
   ```
   ~/Library/Audio/Plug-Ins/Components/NeonForge.component
   ```
5. Restart Logic Pro and scan for new Audio Units

---

## How to Use

1. **Insert NeonForge** on a track in Logic Pro
2. **Scan Audio Units** → pick the target plugin you want to clone
3. **Dial the target** to the sound you like
4. Hit **"AI ASSIST: CAPTURE TARGET"**
5. NeonForge profiles it and switches to **Cloned mode** with derived knobs
6. **Save** the profile as a `.neon` file to reuse later

---

## Python Profiler (Offline)

The `python/` folder contains the derivation engine:
- `profiler.py` — sweep capture & IR deconvolution
- `dynamics.py` — compressor parameter fitting from measured curves
- `validate.py` — null-test validation against synthetic targets

Run offline to generate `.neon` profiles without opening Logic.

---

## Project Structure

```
Install NeonForge.command   ← Double-click installer (Mac)
au/                         ← JUCE AU plugin source
├── Source/
│   ├── PluginProcessor.cpp/h   ← DSP + target hosting + capture
│   ├── PluginEditor.cpp/h      ← Shape-shifting UI (Idle → Profiling → Cloned)
│   ├── Profile.cpp/h           ← JSON profile load/save
│   └── CMakeLists.txt
python/                     ← Offline profiler engine
profiles/                   ← Sample .neon presets
```

---

## Requirements

- macOS 11+ (Intel or Apple Silicon)
- Logic Pro or any AU host
- Xcode Command Line Tools
- CMake 3.22+

---

## License

MIT. Built for sovereign plugin cloning.
