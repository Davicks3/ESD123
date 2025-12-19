import numpy as np
import math
import matplotlib.pyplot as plt

# -----------------------------
# Utility: custom sinc and safe divide
# -----------------------------
def sinc_pi(x):
    """Return sin(pi*x)/(pi*x), with proper handling at x=0 (limit=1)."""
    out = np.empty_like(x, dtype=float)
    # where x == 0 -> 1
    zero_mask = (x == 0)
    out[zero_mask] = 1.0
    nz = ~zero_mask
    out[nz] = np.sin(np.pi * x[nz]) / (np.pi * x[nz])
    return out

# -----------------------------
# 1) Build the signal
# -----------------------------
fs = 192_000
f0 = 40_000
cycles = 10
duration = cycles / f0
N = int(np.floor(fs * duration))

t = np.arange(N, dtype=float) / fs

# Fast exponential rise
tau = 0.1 * duration
envelope = 1.0 - np.exp(-t / tau)

# Fixed-frequency sinus + noise
rng = np.random.default_rng(0)
signal_clean = envelope * np.sin(2 * np.pi * f0 * t)
noise = 0.2 * rng.standard_normal(N)
x = signal_clean + noise

# -----------------------------
# 2) FIR bandpass (windowed-sinc) designed explicitly
# -----------------------------
def design_fir_bandpass(lowcut, highcut, fs, numtaps=101, normalize_freq=None):
    """Return symmetric odd-length FIR bandpass via windowed-sinc, no np.sinc."""
    if numtaps % 2 == 0:
        numtaps += 1
    M = numtaps // 2
    n = np.arange(-M, M + 1, dtype=float)

    # ideal lowpass at fc: 2*(fc/fs)*sinc(2*(fc/fs)*n)
    def ideal_lp(fc):
        wc = fc / fs  # normalized cycles/sample
        return 2.0 * wc * sinc_pi(2.0 * wc * n)

    h_lp_low = ideal_lp(lowcut)
    h_lp_high = ideal_lp(highcut)
    h_ideal = h_lp_high - h_lp_low  # ideal bandpass

    # Hamming window explicitly
    w = 0.54 - 0.46 * np.cos(2.0 * np.pi * (np.arange(numtaps)) / (numtaps - 1))
    h = h_ideal * w

    # Optional: normalize gain near a frequency (here f0)
    if normalize_freq is not None:
        # frequency response at normalize_freq by direct DTFT of taps
        k = np.arange(numtaps) - M
        ang = -2.0 * np.pi * normalize_freq * k / fs
        resp = np.sum(h * np.exp(1j * ang))
        g = np.real(resp)
        if g != 0:
            h = h / g
    return h

taps = design_fir_bandpass(30_000, 50_000, fs, numtaps=101, normalize_freq=f0)

# -----------------------------
# 3) Manual linear convolution
# -----------------------------
def linear_convolve(x, h):
    """Return the full linear convolution of 1D arrays x and h (no np.convolve)."""
    Nx = len(x)
    Nh = len(h)
    y = np.zeros(Nx + Nh - 1, dtype=float)
    for n in range(Nx):
        y[n:n+Nh] += x[n] * h
    return y

y_full = linear_convolve(x, taps)

# Trim to "same" length (center)
M = (len(taps) - 1)//2
y_same = y_full[M:M+N]

# -----------------------------
# 4) Explicit sinc interpolation (8×)
# -----------------------------
def sinc_interpolate_explicit(x, fs, up_factor=8):
    """
    Ideal-bandlimited interpolation implemented from the definition:
    y(t) = sum_n x[n] * sinc((t - n*Ts)/Ts), with sinc = sin(pi u)/(pi u).
    """
    Nloc = len(x)
    Ts = 1.0 / fs
    fs_new = fs * up_factor
    Ts_new = 1.0 / fs_new

    # time grid: include final original instant n=(Nloc-1)
    M_new = (Nloc - 1) * up_factor + 1
    t_new = np.arange(M_new, dtype=float) * Ts_new

    # Direct O(N^2) evaluation (fine here since N is small)
    y_new = np.zeros(M_new, dtype=float)
    for m in range(M_new):
        tm = t_new[m]
        # Compute contributions from all samples
        # u = (tm - n*Ts)/Ts = tm/Ts - n
        u = (tm / Ts) - np.arange(Nloc, dtype=float)
        y_new[m] = np.sum(x * sinc_pi(u))
    return t_new, y_new, fs_new

t_up, y_up, fs_up = sinc_interpolate_explicit(y_same, fs, up_factor=8)

# -----------------------------
# 5) Plots
# -----------------------------
plt.figure()
plt.plot(t, x)
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
plt.title("Original Noisy Signal @ 192 kHz")
plt.grid(True)
plt.show()

plt.figure()
plt.plot(t, y_same)
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
plt.title("Filtered Signal (Manual FIR 30–50 kHz) @ 192 kHz")
plt.grid(True)
plt.show()

plt.figure()
plt.plot(t_up, y_up)
plt.xlabel("Time (s)")
plt.ylabel("Amplitude")
plt.title(f"Sinc-Interpolated Signal (8×) @ {fs_up/1000:.0f} kHz (Explicit Implementation)")
plt.grid(True)
plt.show()
