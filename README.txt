═══════════════════════════════════════════════════════════════
  NEONFORGE — Plugin Profiler & Cloner
  For macOS + Logic Pro (Audio Unit)
═══════════════════════════════════════════════════════════════

WHAT THIS IS
------------
NeonForge hosts any Audio Unit plugin, fires a sweep through it,
and clones its sound into its own parametric engine (EQ, dynamics,
saturation). The clone runs as a native AU in Logic Pro with real
knobs your friend can keep tweaking.

WHAT'S ON THIS USB
------------------
  Install NeonForge.command   ←  DOUBLE-CLICK THIS ON MAC
  au/                         ←  JUCE AU plugin source code
  python/                     ←  Profiler engine (Python)
  profiles/                   ←  Sample .neon profiles

INSTALL INSTRUCTIONS (Mac)
--------------------------
1. Plug the USB into a Mac.
2. Double-click  "Install NeonForge.command"
3. If prompted, install Xcode Command Line Tools (one-time).
   If prompted, install CMake (one-time, get the macOS .dmg
   from cmake.org).
4. The script builds the AU and installs it to:
      ~/Library/Audio/Plug-Ins/Components/NeonForge.component
5. Restart Logic Pro. Open Logic's Plug-in Manager if needed
   to scan new Audio Units.

USING THE PLUGIN
----------------
1. Insert NeonForge on any track in Logic.
2. Hit "Scan Audio Units" → pick the target plugin you want
   to clone.
3. Dial the target plugin to the sound you like.
4. Hit "AI ASSIST: CAPTURE TARGET".
5. NeonForge profiles it and switches to Cloned mode with
   derived knobs (Threshold, Ratio, Attack, Release, EQ, etc).
6. Save the profile as a .neon file to reuse later.

PYTHON PROFILER (advanced)
--------------------------
The Python scripts in the python/ folder run the same derivation
algorithms offline. You can use them to generate .neon profiles
from recorded audio files without opening Logic.

SUPPORT
-------
If the installer fails, open Terminal and run:
    cd /Volumes/YOUR_USB_NAME/neonforge/au/build
    cmake ..
    cmake --build . --config Release
Then copy NeonForge.component from build/NeonForge_artefacts/
to ~/Library/Audio/Plug-Ins/Components/

═══════════════════════════════════════════════════════════════
