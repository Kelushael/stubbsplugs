"""Dynamics tier — derive a compressor's *parameters* from its sound alone.

Run the target, measure the static dB-in/dB-out curve (settled tones) and the
gain-reduction step response (attack/release). Then fit interpretable knobs:
threshold, ratio, knee, attack, release. No source code, no parameter list —
just the audio. The clone is a real parametric compressor set to those knobs.
"""
import numpy as np
from scipy.optimize import least_squares
from scipy.ndimage import uniform_filter1d

EPS = 1e-9


def comp_gain_db(level_db, thresh, ratio, knee):
    """Soft-knee downward-compression law -> gain to apply (<= 0 dB)."""
    over = level_db - thresh
    lo = over <= -knee / 2
    hi = over >= knee / 2
    g = (1 / ratio - 1) * over                      # full compression slope
    g = np.where(hi, g, (1 / ratio - 1) * (over + knee / 2) ** 2 / (2 * knee + EPS))
    return np.where(lo, 0.0, g)


def _smooth_db(rdb, aA, aR):
    """One-pole detector in the dB domain: fast coef rising, slow coef falling."""
    env = np.empty_like(rdb)
    e = rdb[0]
    for n in range(len(rdb)):
        c = aA if rdb[n] > e else aR
        e = c * e + (1 - c) * rdb[n]
        env[n] = e
    return env


class Compressor:
    """Synthetic device under test — the thing we pretend not to know."""

    def __init__(self, fs, thresh=-24.0, ratio=4.0, knee=6.0, attack_ms=5.0, release_ms=150.0):
        self.fs = fs
        self.thresh, self.ratio, self.knee = thresh, ratio, knee
        self.aA = np.exp(-1 / (attack_ms * 1e-3 * fs))
        self.aR = np.exp(-1 / (release_ms * 1e-3 * fs))

    def render(self, x):
        rdb = 20 * np.log10(np.abs(x) + EPS)
        envdb = _smooth_db(rdb, self.aA, self.aR)
        gdb = comp_gain_db(envdb, self.thresh, self.ratio, self.knee)
        return x * 10 ** (gdb / 20)


class ParametricCompressor:
    """The clone — identical topology, driven by the *derived* knobs."""

    def __init__(self, fs, thresh, ratio, knee, attack_ms, release_ms):
        self.thresh, self.ratio, self.knee = thresh, ratio, knee
        self.aA = np.exp(-1 / (max(attack_ms, 1e-3) * 1e-3 * fs))
        self.aR = np.exp(-1 / (max(release_ms, 1e-3) * 1e-3 * fs))

    def render(self, x):
        rdb = 20 * np.log10(np.abs(x) + EPS)
        envdb = _smooth_db(rdb, self.aA, self.aR)
        gdb = comp_gain_db(envdb, self.thresh, self.ratio, self.knee)
        return x * 10 ** (gdb / 20)


def _tone(level_db, fs, dur, f0=200.0):
    """Square carrier: |x| is constant, so the level detector sees a clean
    step instead of per-cycle ripple — time constants come out unbiased."""
    a = 10 ** (level_db / 20)
    t = np.arange(int(dur * fs)) / fs
    return a * np.sign(np.sin(2 * np.pi * f0 * t) + 1e-12)


def capture_static(dut, fs, levels=None, dur=0.4):
    """Settled gain at each drive level -> the compression curve."""
    if levels is None:
        levels = np.linspace(-60, -2, 30)
    gains = []
    for L in levels:
        x = _tone(L, fs, dur)
        y = dut.render(x)
        tail = slice(int(0.6 * len(x)), None)
        gains.append(20 * np.log10(np.sqrt(np.mean(y[tail] ** 2)) /
                                   (np.sqrt(np.mean(x[tail] ** 2)) + EPS) + EPS))
    return np.array(levels), np.array(gains)


def _gain_db(x, y, win):
    """Applied gain over time via a one-carrier-period sliding RMS (exact for a
    steady sine regardless of phase, so no ripple and no edge ringing)."""
    px = uniform_filter1d(x * x, win)
    py = uniform_filter1d(y * y, win)
    return 10 * np.log10(py / (px + EPS) + EPS)


def _tau_63(gdb, fs, i0, i1):
    """Time for the gain-reduction step to cover 63.2% of its travel."""
    start = np.median(gdb[max(0, i0 - int(0.02 * fs)):i0])
    final = np.median(gdb[i1 - int(0.05 * fs):i1])
    target = start + 0.632 * (final - start)
    seg = gdb[i0:i1]
    hit = np.where(seg <= target)[0] if final < start else np.where(seg >= target)[0]
    k = hit[0] if len(hit) else len(seg) - 1
    return max(k / fs, 1 / fs) * 1000  # ms


def capture_timing(dut, fs, lo_db, hi_db, f0=1000.0):
    """Continuous-phase carrier, amplitude stepped up then down between two
    compressed levels; read attack & release off the gain envelope."""
    durs, amps = [0.2, 0.4, 0.7], [lo_db, hi_db, lo_db]
    n = [int(d * fs) for d in durs]
    t = np.arange(sum(n)) / fs
    env = np.concatenate([np.full(ni, 10 ** (a / 20)) for ni, a in zip(n, amps)])
    x = env * np.sign(np.sin(2 * np.pi * f0 * t) + 1e-12)
    y = dut.render(x)
    g = _gain_db(x, y, int(round(fs / f0)))
    attack = _tau_63(g, fs, n[0], n[0] + n[1])
    release = _tau_63(g, fs, n[0] + n[1], sum(n))
    return attack, release


def fit_compressor(levels, gains):
    """Least-squares solve for the knobs behind the measured curve."""
    def resid(p):
        return comp_gain_db(levels, *p) - gains
    p0 = [-30.0, 2.0, 3.0]
    lo, hi = [-72, 1.0, 0.0], [0, 40.0, 24.0]
    p = least_squares(resid, p0, bounds=(lo, hi)).x
    return {"threshold_db": p[0], "ratio": p[1], "knee_db": p[2]}


def derive(dut, fs):
    levels, gains = capture_static(dut, fs)
    knobs = fit_compressor(levels, gains)
    a, r = capture_timing(dut, fs, knobs["threshold_db"] + 8, knobs["threshold_db"] + 18)
    knobs["attack_ms"], knobs["release_ms"] = a, r
    return knobs
