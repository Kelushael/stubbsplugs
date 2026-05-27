"""Profile a device: linear IR via sweep, static curve via amplitude ramp,
spectral fingerprint via FFT of the IR. Emits a portable profile the clone
generator (or a Logic AU) can reconstruct from.
"""
import json
from pathlib import Path

import numpy as np
import soundfile as sf

from sweep import ess, deconvolve

BANDS = {
    "sub_bass": (20, 60), "bass": (60, 250), "low_mid": (250, 500),
    "mid": (500, 2000), "high_mid": (2000, 4000), "high": (4000, 10000),
    "air": (10000, 20000),
}


def capture_linear(dut, fs, dur=8.0, amp=0.02, ir_len=1 << 15):
    """Low amplitude so any nonlinearity stays in its linear region — this
    isolates the EQ/phase/reverb character."""
    x = ess(20, 0.99 * fs / 2, dur, fs, amp=amp)
    y = np.asarray(dut.render(x), dtype=np.float64)
    return deconvolve(x, y, ir_len=ir_len)


def capture_curve(dut, fs, f0=110.0, dur=4.0, bins=512, pre=None):
    """Drive a slow tone whose amplitude ramps 0 -> full and read the static
    input->output transfer. Captures memoryless saturation/clip shape.

    `pre` is the estimated linear stage: for a cascade we fit the curve against
    the signal *at the nonlinearity's input* (pre(x)), not the raw probe, so the
    waveshaper isn't double-counting the EQ."""
    n = int(dur * fs)
    t = np.arange(n) / fs
    env = np.linspace(0.0, 0.98, n)
    x = env * np.sin(2 * np.pi * f0 * t)
    y = np.asarray(dut.render(x), dtype=np.float64)
    drive = np.asarray(pre(x), dtype=np.float64) if pre else x
    edges = np.linspace(drive.min(), drive.max(), bins + 1)
    idx = np.clip(np.digitize(drive, edges) - 1, 0, bins - 1)
    grid = (edges[:-1] + edges[1:]) / 2
    out = np.full(bins, np.nan)
    for b in range(bins):
        sel = idx == b
        if sel.any():
            out[b] = np.median(y[sel])
    good = ~np.isnan(out)
    return grid[good], out[good]


def spectral_fingerprint(ir, fs):
    H = np.fft.rfft(ir)
    freqs = np.fft.rfftfreq(len(ir), 1 / fs)
    mag_db = 20 * np.log10(np.abs(H) + 1e-12)
    band = {}
    for name, (lo, hi) in BANDS.items():
        sel = (freqs >= lo) & (freqs <= hi)
        band[name] = float(np.mean(mag_db[sel])) if sel.any() else None
    return freqs, mag_db, band


def profile(dut, fs, name="capture", out_dir=None, nonlinear=True):
    ir = capture_linear(dut, fs)
    freqs, mag_db, band = spectral_fingerprint(ir, fs)
    prof = {
        "name": name, "fs": fs, "ir_len": len(ir),
        "band_response_db": band,
        "nonlinear": None,
    }
    if nonlinear:
        xg, yg = capture_curve(dut, fs)
        prof["nonlinear"] = {"x": xg.tolist(), "y": yg.tolist()}
    if out_dir:
        out_dir = Path(out_dir)
        out_dir.mkdir(parents=True, exist_ok=True)
        sf.write(out_dir / f"{name}_ir.wav", ir.astype(np.float32), fs)
        (out_dir / f"{name}.json").write_text(json.dumps(prof, indent=2))
    return ir, prof
