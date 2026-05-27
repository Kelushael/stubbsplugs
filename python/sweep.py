"""Exponential sine sweep + regularized deconvolution.

The sweep is the excitation. Deconvolving the captured response against the
known excitation recovers the device's linear impulse response — its full
magnitude/phase fingerprint in one shot.
"""
import numpy as np


def ess(f1, f2, dur, fs, amp=0.5, fade=0.02):
    n = int(round(dur * fs))
    t = np.arange(n) / fs
    w1, w2 = 2 * np.pi * f1, 2 * np.pi * f2
    L = dur / np.log(w2 / w1)
    K = w1 * L
    x = np.sin(K * (np.exp(t / L) - 1.0))
    nf = int(fade * fs)
    if nf > 0:
        win = np.hanning(2 * nf)
        x[:nf] *= win[:nf]
        x[-nf:] *= win[nf:]
    return (amp * x).astype(np.float64)


def deconvolve(x, y, reg=1e-9, ir_len=None):
    n = len(x) + len(y)
    nfft = 1 << int(np.ceil(np.log2(n)))
    X = np.fft.rfft(x, nfft)
    Y = np.fft.rfft(y, nfft)
    # Regularize where the sweep has no energy so we don't divide by ~0.
    eps = reg * np.max(np.abs(X) ** 2)
    H = Y * np.conj(X) / (np.abs(X) ** 2 + eps)
    h = np.fft.irfft(H, nfft)
    if ir_len:
        h = h[:ir_len].copy()
        tail = max(1, ir_len // 16)
        h[-tail:] *= np.hanning(2 * tail)[tail:]
    return h
