#!/usr/bin/env python3
from __future__ import annotations
import numpy as np
import matplotlib.pyplot as plt

# =======================
# USER SETTINGS
# =======================
REAL_CSV = "real.csv"
MEAS_CSV = "measured.csv"
SEP = ";"

T_MIN, T_MAX = 0.0, 2.0
FIT_PTS = 700
ERR_PTS = 900

K_RANGE = (1e-4, 10.0)
K_STEPS = 700

K_RANGE_ERR = (1e-4, 10.0)
K_STEPS_ERR = 700

# Classical LPF tuning
ACC_MAX = 4.0         # [m/s^2] (soft heuristic only; classical LPF can't hard-enforce it)
LPF_TAU_SCALE = 0.20  # <-- IMPORTANT: smaller = less smoothing (try 0.05, 0.10, 0.20)

# Drop initial filtered samples (initial rise / transient)
DROP_FILTERED_N = 2   # remove first N samples from the FILTERED signals before regression/compensation
# =======================

REAL_COLOR = "tab:blue"
MEAS_COLOR = "tab:orange"


def load_xy_csv(path: str, sep: str = ";") -> tuple[np.ndarray, np.ndarray]:
    """Load x,y from CSV with separator sep (supports header or no header)."""
    try:
        data = np.genfromtxt(path, delimiter=sep, names=True, dtype=float, encoding=None)
        if data.dtype.names is None:
            raise ValueError
        x = np.asarray(data[data.dtype.names[0]], dtype=float)
        y = np.asarray(data[data.dtype.names[1]], dtype=float)
    except Exception:
        raw = np.genfromtxt(path, delimiter=sep, dtype=float)
        if raw.ndim != 2 or raw.shape[1] < 2:
            raise ValueError(f"{path}: expected at least 2 columns separated by '{sep}'.")
        x = raw[:, 0].astype(float)
        y = raw[:, 1].astype(float)

    m = np.isfinite(x) & np.isfinite(y)
    x, y = x[m], y[m]
    idx = np.argsort(x)
    return x[idx], y[idx]


def drop_first_n(x: np.ndarray, y: np.ndarray, n: int) -> tuple[np.ndarray, np.ndarray]:
    """Drop first n samples from paired arrays (keeps alignment)."""
    n = int(max(0, n))
    if n == 0:
        return x, y
    if len(x) <= n:
        return x[:0], y[:0]
    return x[n:], y[n:]


# ---------- Saturating exponential model ----------
# y(x) = y0 + A*(1 - exp(-k*x))
def f_sat(y0: float, A: float, k: float, x: np.ndarray) -> np.ndarray:
    return y0 + A * (1.0 - np.exp(-k * x))


def fit_sat_exp(
    x: np.ndarray,
    y: np.ndarray,
    k_range: tuple[float, float],
    k_steps: int
) -> tuple[float, float, float]:
    """
    Fit y = y0 + A*(1-exp(-k*x)) by grid-searching k (log-space)
    and solving (y0, A) via least squares for each k.
    Returns (y0, A, k).
    """
    x = np.asarray(x, float)
    y = np.asarray(y, float)

    if len(x) < 2 or len(y) < 2:
        raise ValueError("fit_sat_exp: need at least 2 points.")

    ks = np.logspace(np.log10(k_range[0]), np.log10(k_range[1]), k_steps)
    ones = np.ones_like(x)

    best = None  # (sse, y0, A, k)
    for k in ks:
        phi = 1.0 - np.exp(-k * x)
        X = np.column_stack([ones, phi])
        (y0, A), *_ = np.linalg.lstsq(X, y, rcond=None)
        yhat = y0 + A * phi
        sse = float(np.sum((y - yhat) ** 2))
        if best is None or sse < best[0]:
            best = (sse, float(y0), float(A), float(k))

    _, y0, A, k = best
    return y0, A, k


def x_intercept_sat(y0: float, A: float, k: float) -> float | None:
    """Solve 0 = y0 + A*(1-exp(-k*x)). Returns None if no real solution."""
    if A == 0:
        return None
    rhs = 1.0 + y0 / A
    if rhs <= 0:
        return None
    return float(-(1.0 / k) * np.log(rhs))


# ---------- Classical low-pass filter (1st order RC) ----------
def lowpass_1pole_irregular(t: np.ndarray, y: np.ndarray, tau: float) -> np.ndarray:
    """
    Classical 1st-order low-pass (RC) for irregular sampling.
    alpha = 1 - exp(-dt/tau)
    """
    t = np.asarray(t, float)
    y = np.asarray(y, float)

    if len(y) == 0 or not np.isfinite(tau) or tau <= 0:
        return y.copy()

    yf = np.empty_like(y)
    yf[0] = y[0]

    for i in range(1, len(y)):
        dt = float(t[i] - t[i - 1])
        if not np.isfinite(dt) or dt <= 0:
            yf[i] = yf[i - 1]
            continue
        alpha = 1.0 - np.exp(-dt / tau)
        yf[i] = yf[i - 1] + alpha * (y[i] - yf[i - 1])

    return yf


# ---------- Compensation function (value-in, value-out) ----------
def compensate_measured_value(
    y_meas: np.ndarray,
    y0m: float, Am: float, km: float,
    e0: float, Ae: float, ke: float
) -> np.ndarray:
    """
    y_comp(y_m) = y_m + e(t(y_m)),
    where t(y_m) is from inverting the measured fit (t-domain).
    """
    y = np.asarray(y_meas, dtype=float)
    if Am == 0 or km == 0:
        return y.copy()

    frac = (y - y0m) / Am
    frac = np.clip(frac, 0.0, 0.999999)

    t = -(1.0 / km) * np.log(1.0 - frac)
    e = e0 + Ae * (1.0 - np.exp(-ke * t))
    return y + e


def print_sat_exp(name: str, y0: float, A: float, k: float, var: str = "x") -> None:
    print(f"\n{name}")
    print(f"y({var}) = y0 + A*(1 - exp(-k*{var}))")
    print(f"y({var}) = {y0:.6g} + {A:.6g}*(1 - exp(-{k:.6g}*{var}))")


def main() -> None:
    # Load
    xr, yr = load_xy_csv(REAL_CSV, SEP)
    xm, ym = load_xy_csv(MEAS_CSV, SEP)

    # ==========================================
    # UNFILTERED fits (used ONLY for Figure 1)
    # ==========================================
    y0r_x, Ar_x, kr_x = fit_sat_exp(xr, yr, K_RANGE, K_STEPS)
    y0m_x, Am_x, km_x = fit_sat_exp(xm, ym, K_RANGE, K_STEPS)

    x0r = x_intercept_sat(y0r_x, Ar_x, kr_x) or float(xr.min())
    x0m = x_intercept_sat(y0m_x, Am_x, km_x) or float(xm.min())

    tr = xr - x0r
    tm = xm - x0m

    print("\n=== REGRESSIONS (x-domain) ===")
    print_sat_exp("Real signal regression (x-domain)", y0r_x, Ar_x, kr_x, var="x")
    print_sat_exp("Measured signal regression (x-domain)", y0m_x, Am_x, km_x, var="x")

    # Domains
    tfit = np.linspace(T_MIN, T_MAX, FIT_PTS)
    tg = np.linspace(T_MIN, T_MAX, ERR_PTS)

    # Fits in t-domain (evaluate using x = t + x0)
    real_fit_t = f_sat(y0r_x, Ar_x, kr_x, tfit + x0r)
    meas_fit_t = f_sat(y0m_x, Am_x, km_x, tfit + x0m)

    # Error function for Figure 1
    err = f_sat(y0r_x, Ar_x, kr_x, tg + x0r) - f_sat(y0m_x, Am_x, km_x, tg + x0m)

    # Masks for plotting only t in [0,2]
    mr = (tr >= T_MIN) & (tr <= T_MAX)
    mm = (tm >= T_MIN) & (tm <= T_MAX)

    # =======================
    # FIGURE 1: MUST REMAIN UNCHANGED
    # =======================
    fig1, (ax_sig, ax_err) = plt.subplots(
        2, 1, sharex=True, figsize=(9, 7),
        gridspec_kw={"height_ratios": [3, 1]}
    )

    ax_sig.scatter(tr[mr], yr[mr], s=18, color=REAL_COLOR, label="Drone hastighed - målinger")
    ax_sig.plot(tfit, real_fit_t, lw=2, color=REAL_COLOR, label="Drone hastighed - regression")

    ax_sig.scatter(tm[mm], ym[mm], s=18, color=MEAS_COLOR, label="Hjul hastighed - målinger")
    ax_sig.plot(tfit, meas_fit_t, lw=2, color=MEAS_COLOR, label="Hjul hastighed - regression")

    ax_sig.set_xlim(T_MIN, T_MAX)
    ax_sig.set_ylabel("Hastighed [m/s]")
    ax_sig.set_title("Drone og hjul hastighed")
    ax_sig.grid(True)
    ax_sig.legend(loc="best")

    ax_err.plot(tg, err, lw=2, label="Fejl (forskel mellem drone- og hjul hastigheds regression)")
    ax_err.set_xlim(T_MIN, T_MAX)
    ax_err.set_xlabel("t [s]")
    ax_err.set_ylabel("Hastighed [m/s]")
    ax_err.grid(True)
    ax_err.legend(loc="best")

    plt.tight_layout()
    plt.show()

    # ============================================================
    # CLASSICAL LPF CHOICE: tie to real regression time constant (preserves exponential shape)
    # ============================================================
    if not np.isfinite(kr_x) or kr_x <= 0:
        raise ValueError("kr_x must be > 0 to design LPF from regression dynamics.")
    tau_dyn = 1.0 / kr_x
    tau_lpf = max(LPF_TAU_SCALE * tau_dyn, 1e-6)

    # Optional: very soft lower bound using ACC_MAX (sanity floor only)
    y0r_t = float(f_sat(y0r_x, Ar_x, kr_x, np.array([x0r]))[0])
    Ar_t = float(Ar_x * np.exp(-kr_x * x0r))
    if np.isfinite(ACC_MAX) and ACC_MAX > 0:
        tau_from_acc_soft = max(abs(Ar_t) / ACC_MAX, 0.0)
        tau_lpf = max(tau_lpf, 0.02 * tau_from_acc_soft)

    fc_lpf = 1.0 / (2.0 * np.pi * tau_lpf)

    print("\n=== CLASSICAL LOW-PASS (1-pole RC) SETTINGS ===")
    print(f"LPF_TAU_SCALE = {LPF_TAU_SCALE:.6g} (smaller = less smoothing)")
    print(f"tau_dyn = 1/kr = {tau_dyn:.6g} s")
    print(f"tau_lpf = {tau_lpf:.6g} s   -> fc ≈ {fc_lpf:.6g} Hz")
    print("Tip: if the filtered curve looks linear, reduce LPF_TAU_SCALE (e.g. 0.10 or 0.05).")

    # Filter BOTH signals in their original shifted time bases (tr/tm)
    tr_idx = np.argsort(tr)
    tr_s = tr[tr_idx]
    yr_s = yr[tr_idx]
    yr_f_s = lowpass_1pole_irregular(tr_s, yr_s, tau_lpf)
    yr_f = np.empty_like(yr)
    yr_f[tr_idx] = yr_f_s

    tm_idx = np.argsort(tm)
    tm_s = tm[tm_idx]
    ym_s = ym[tm_idx]
    ym_f_s = lowpass_1pole_irregular(tm_s, ym_s, tau_lpf)
    ym_f = np.empty_like(ym)
    ym_f[tm_idx] = ym_f_s

    # ============================================================
    # DROP first N filtered samples (initial rise / transient)
    # ============================================================
    xr_filt, yr_filt = drop_first_n(xr, yr_f, DROP_FILTERED_N)
    xm_filt, ym_filt = drop_first_n(xm, ym_f, DROP_FILTERED_N)

    if len(xr_filt) < 2 or len(xm_filt) < 2:
        raise ValueError("Too few samples left after dropping DROP_FILTERED_N.")

    # ===========================
    # NEW regressions on FILTERED samples (after dropping first N)
    # ===========================
    y0r_fx, Ar_fx, kr_fx = fit_sat_exp(xr_filt, yr_filt, K_RANGE, K_STEPS)
    y0m_fx, Am_fx, km_fx = fit_sat_exp(xm_filt, ym_filt, K_RANGE, K_STEPS)

    x0r_f = x_intercept_sat(y0r_fx, Ar_fx, kr_fx) or float(xr_filt.min())
    x0m_f = x_intercept_sat(y0m_fx, Am_fx, km_fx) or float(xm_filt.min())

    tr_f = xr_filt - x0r_f
    tm_f = xm_filt - x0m_f

    mr_f = (tr_f >= T_MIN) & (tr_f <= T_MAX)
    mm_f = (tm_f >= T_MIN) & (tm_f <= T_MAX)

    real_fit_t_f = f_sat(y0r_fx, Ar_fx, kr_fx, tfit + x0r_f)
    meas_fit_t_f = f_sat(y0m_fx, Am_fx, km_fx, tfit + x0m_f)

    # New error function from filtered regressions
    err_f = f_sat(y0r_fx, Ar_fx, kr_fx, tg + x0r_f) - f_sat(y0m_fx, Am_fx, km_fx, tg + x0m_f)
    e0_f, Ae_f, ke_f = fit_sat_exp(tg, err_f, K_RANGE_ERR, K_STEPS_ERR)

    # Convert FILTERED measured regression to standard t-domain form for inversion
    y0m_ft = float(f_sat(y0m_fx, Am_fx, km_fx, np.array([x0m_f]))[0])
    Am_ft = float(Am_fx * np.exp(-km_fx * x0m_f))
    km_ft = float(km_fx)

    # Compensate FILTERED wheel samples (after dropping first N)
    y_comp_f = compensate_measured_value(ym_filt, y0m_ft, Am_ft, km_ft, e0_f, Ae_f, ke_f)

    # Residual error vs FILTERED real regression at measured sample positions
    y_real_at_tm_f = f_sat(y0r_fx, Ar_fx, kr_fx, tm_f + x0r_f)
    final_err_f = y_real_at_tm_f - y_comp_f

    # =======================
    # FIGURE 2 (NEW): filtered samples + new regressions + new error
    # =======================
    fig2, (ax_sig_f, ax_err_f) = plt.subplots(
        2, 1, sharex=True, figsize=(9, 7),
        gridspec_kw={"height_ratios": [3, 1]}
    )

    ax_sig_f.scatter(tr_f[mr_f], yr_filt[mr_f], s=18, color=REAL_COLOR, label="Drone hastighed - filtreret målinger")
    ax_sig_f.plot(tfit, real_fit_t_f, lw=2, color=REAL_COLOR, label="Drone hastighed - ny regression (filtreret)")

    ax_sig_f.scatter(tm_f[mm_f], ym_filt[mm_f], s=18, color=MEAS_COLOR, label="Hjul hastighed - filtreret målinger")
    ax_sig_f.plot(tfit, meas_fit_t_f, lw=2, color=MEAS_COLOR, label="Hjul hastighed - ny regression (filtreret)")

    ax_sig_f.set_xlim(T_MIN, T_MAX)
    ax_sig_f.set_ylabel("Hastighed [m/s]")
    ax_sig_f.set_title("Drone og hjul hastighed (lavpasfiltreret)")
    ax_sig_f.grid(True)
    ax_sig_f.legend(loc="best")

    ax_err_f.plot(tg, err_f, lw=2, label="Fejl (forskel mellem nye regressioner)")
    ax_err_f.set_xlim(T_MIN, T_MAX)
    ax_err_f.set_xlabel("t [s]")
    ax_err_f.set_ylabel("Hastighed [m/s]")
    ax_err_f.grid(True)
    ax_err_f.legend(loc="best")

    plt.tight_layout()
    plt.show()

    # =======================
    # FIGURE 3 (LAST): compensated filtered samples
    # =======================
    fig3, (ax_comp_last, ax_ferr_last) = plt.subplots(
        2, 1, sharex=True, figsize=(9, 7),
        gridspec_kw={"height_ratios": [3, 1]}
    )

    ax_comp_last.scatter(
        tm_f[mm_f], y_comp_f[mm_f], s=30, color=MEAS_COLOR, alpha=1.0,
        edgecolors="none", antialiased=False,
        label="Kompenseret hjul hastighed - filtreret målinger"
    )
    ax_comp_last.plot(tfit, real_fit_t_f, lw=1, color=REAL_COLOR, label="Drone hastighed - ny regression (filtreret)")

    ax_comp_last.set_xlim(T_MIN, T_MAX)
    ax_comp_last.set_ylabel("Hastighed [m/s]")
    ax_comp_last.set_title("Kompenseret målinger (lavpasfiltreret)")
    ax_comp_last.grid(True)
    ax_comp_last.legend(loc="best")

    ax_ferr_last.scatter(
        tm_f[mm_f], final_err_f[mm_f], s=18, color=MEAS_COLOR,
        label="Fejl (drone ny regression - kompenseret filtreret hjul målinger)"
    )
    ax_ferr_last.set_xlim(T_MIN, T_MAX)
    ax_ferr_last.set_xlabel("t [s]")
    ax_ferr_last.set_ylabel("Hastighed [m/s]")
    ax_ferr_last.grid(True)
    ax_ferr_last.legend(loc="best")

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()