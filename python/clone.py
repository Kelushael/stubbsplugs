"""Reconstruct a processor from a captured profile. This is the math the Logic
AU runs: FIR convolution for the linear character, a waveshaper lookup for the
static nonlinearity. Stack however the original was ordered.
"""
import numpy as np
from scipy.signal import fftconvolve


class LinearClone:
    def __init__(self, ir):
        self.ir = np.asarray(ir, dtype=np.float64)

    def render(self, x):
        return fftconvolve(x, self.ir)[: len(x)]


class CurveClone:
    def __init__(self, xg, yg):
        self.xg = np.asarray(xg)
        self.yg = np.asarray(yg)

    def render(self, x):
        return np.interp(x, self.xg, self.yg)


class Clone:
    """linear-then-curve (Wiener) or curve-then-linear (Hammerstein)."""

    def __init__(self, ir=None, curve=None, order="linear_first"):
        self.lin = LinearClone(ir) if ir is not None else None
        self.crv = CurveClone(*curve) if curve is not None else None
        self.order = order

    def render(self, x):
        y = x
        if self.order == "curve_first" and self.crv:
            y = self.crv.render(y)
        if self.lin:
            y = self.lin.render(y)
        if self.order == "linear_first" and self.crv:
            y = self.crv.render(y)
        return y
