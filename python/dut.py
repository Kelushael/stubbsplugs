"""Devices under test — synthetic stand-ins for the real plugins.

On the Mac these get replaced by `auprobe`, which renders the sweep through an
actual Audio Unit. Here they let us prove the profiler nulls to the floor before
it ever touches Logic. Each exposes render(x) -> y at the module sample rate.
"""
import numpy as np
from scipy.signal import sosfilt


def _rbj_peak(fs, f0, gain_db, q):
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2 * q)
    cw = np.cos(w0)
    b = [1 + alpha * A, -2 * cw, 1 - alpha * A]
    a = [1 + alpha / A, -2 * cw, 1 - alpha / A]
    return [b[0] / a[0], b[1] / a[0], b[2] / a[0], 1.0, a[1] / a[0], a[2] / a[0]]


def _rbj_shelf(fs, f0, gain_db, q, high):
    A = 10 ** (gain_db / 40)
    w0 = 2 * np.pi * f0 / fs
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / 2 * np.sqrt((A + 1 / A) * (1 / q - 1) + 2)
    tsa = 2 * np.sqrt(A) * alpha
    if high:
        b = [A * ((A + 1) + (A - 1) * cw + tsa),
             -2 * A * ((A - 1) + (A + 1) * cw),
             A * ((A + 1) + (A - 1) * cw - tsa)]
        a = [(A + 1) - (A - 1) * cw + tsa,
             2 * ((A - 1) - (A + 1) * cw),
             (A + 1) - (A - 1) * cw - tsa]
    else:
        b = [A * ((A + 1) - (A - 1) * cw + tsa),
             2 * A * ((A - 1) - (A + 1) * cw),
             A * ((A + 1) - (A - 1) * cw - tsa)]
        a = [(A + 1) + (A - 1) * cw + tsa,
             -2 * ((A - 1) + (A + 1) * cw),
             (A + 1) + (A - 1) * cw - tsa]
    return [b[0] / a[0], b[1] / a[0], b[2] / a[0], 1.0, a[1] / a[0], a[2] / a[0]]


class EQ:
    """Pure LTI: four-band parametric. The clean case — should null to the floor."""

    def __init__(self, fs):
        self.sos = np.array([
            _rbj_shelf(fs, 100, 4.0, 0.7, high=False),
            _rbj_peak(fs, 750, -3.5, 1.2),
            _rbj_peak(fs, 4200, 5.0, 1.8),
            _rbj_shelf(fs, 11000, 2.5, 0.7, high=True),
        ])

    def render(self, x):
        return sosfilt(self.sos, x)


class Drive:
    """Memoryless odd-order saturation. Unity small-signal slope."""

    def __init__(self, drive=3.0):
        self.drive = drive

    def render(self, x):
        return np.tanh(self.drive * x) / self.drive


class ChannelStrip:
    """EQ then Drive — a cascade (Wiener). The honestly-hard case."""

    def __init__(self, fs, drive=3.0):
        self.eq = EQ(fs)
        self.sat = Drive(drive)

    def render(self, x):
        return self.sat.render(self.eq.render(x))
