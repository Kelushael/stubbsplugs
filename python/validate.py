"""Proof harness: profile a known synthetic plugin, clone it, measure how
perfectly the clone nulls against the original on independent material. Lower
dB = closer to identical. This is what we trust before the Mac side exists.
"""
import numpy as np

import dut as D
import dynamics as DY
from profiler import capture_linear, capture_curve, profile
from clone import LinearClone, CurveClone, Clone

FS = 48000


def null_db(ref, est):
    ref, est = np.asarray(ref), np.asarray(est)
    err = est - ref
    return 20 * np.log10(np.sqrt(np.mean(err ** 2)) / (np.sqrt(np.mean(ref ** 2)) + 1e-20) + 1e-20)


def test_signal(seed=7, dur=3.0, amp=0.3):
    rng = np.random.default_rng(seed)
    n = int(dur * FS)
    t = np.arange(n) / FS
    sig = sum(np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) for f in (55, 196, 440, 1318, 5200))
    sig += 0.5 * rng.standard_normal(n)
    sig /= np.max(np.abs(sig))
    return amp * sig


def main():
    x = test_signal()

    print("NeonForge profiler — null residual vs original (lower is tighter)\n")

    eq = D.EQ(FS)
    ir, _ = profile(eq, FS, name="eq", out_dir="profiles", nonlinear=False)
    res = null_db(eq.render(x), LinearClone(ir).render(x))
    print(f"  [A] linear EQ          clone={res:7.1f} dB   (artifacts -> profiles/)")

    sat = D.Drive(drive=3.0)
    xg, yg = capture_curve(sat, FS)
    res = null_db(sat.render(x), CurveClone(xg, yg).render(x))
    print(f"  [B] memoryless drive   clone={res:7.1f} dB")

    strip = D.ChannelStrip(FS, drive=3.0)
    ir = capture_linear(strip, FS)
    lin = LinearClone(ir)
    lin_only = null_db(strip.render(x), lin.render(x))
    xg, yg = capture_curve(strip, FS, pre=lin.render)
    full = null_db(strip.render(x), Clone(ir=ir, curve=(xg, yg), order="linear_first").render(x))
    print(f"  [C] EQ+drive strip     linear-only={lin_only:6.1f} dB -> +nonlinear={full:6.1f} dB")

    comp = DY.Compressor(FS, thresh=-24, ratio=4, knee=6, attack_ms=5, release_ms=150)
    k = DY.derive(comp, FS)
    clone = DY.ParametricCompressor(FS, k["threshold_db"], k["ratio"], k["knee_db"],
                                    k["attack_ms"], k["release_ms"])
    dyn = dynamic_signal()
    res = null_db(comp.render(dyn), clone.render(dyn))
    print(f"  [D] compressor         clone={res:7.1f} dB   (derived parameters, no code)")
    print(f"      derived  thresh={k['threshold_db']:6.1f} dB  ratio={k['ratio']:.2f}  "
          f"knee={k['knee_db']:.1f} dB  attack={k['attack_ms']:.1f} ms  release={k['release_ms']:.0f} ms")
    print(f"      truth    thresh= -24.0 dB  ratio=4.00  knee=6.0 dB  attack=5.0 ms  release=150 ms")


def dynamic_signal(dur=1.2, f0=1000.0):
    rng = np.random.default_rng(3)
    fs = FS
    levels = rng.uniform(-22, -4, 8)
    parts = [10 ** (L / 20) * np.sin(2 * np.pi * f0 * np.arange(int(dur / 8 * fs)) / fs) for L in levels]
    return np.concatenate(parts)


if __name__ == "__main__":
    main()
