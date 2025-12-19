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
# =======================

REAL_COLOR = "tab:blue"    # matplotlib default blue
MEAS_COLOR = "tab:orange"  # matplotlib default orange


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


# ---------- Compensation function (value-in, value-out) ----------
def compensate_measured_value(
    y_meas: np.ndarray,
    # measured fit in t-domain: y_m(t)=y0m + Am*(1-exp(-km*t))
    y0m: float, Am: float, km: float,
    # error fit in t-domain: e(t)=e0 + Ae*(1-exp(-ke*t))
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
    frac = np.clip(frac, 0.0, 0.999999)  # keep log safe

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

    # Fit real/measured in original x-domain
    y0r_x, Ar_x, kr_x = fit_sat_exp(xr, yr, K_RANGE, K_STEPS)
    y0m_x, Am_x, km_x = fit_sat_exp(xm, ym, K_RANGE, K_STEPS)

    # Shift each so its fitted curve crosses y=0 at t=0
    x0r = x_intercept_sat(y0r_x, Ar_x, kr_x) or float(xr.min())
    x0m = x_intercept_sat(y0m_x, Am_x, km_x) or float(xm.min())

    tr = xr - x0r
    tm = xm - x0m
    
    print("\n=== REGRESSIONS (x-domain) ===")

    print_sat_exp(
        "Real signal regression (x-domain)",
        y0r_x, Ar_x, kr_x, var="x"
    )

    print_sat_exp(
        "Measured signal regression (x-domain)",
        y0m_x, Am_x, km_x, var="x"
    )

    # Domains
    tfit = np.linspace(T_MIN, T_MAX, FIT_PTS)
    tg = np.linspace(T_MIN, T_MAX, ERR_PTS)

    # Fits in t-domain (evaluate using x = t + x0)
    real_fit_t = f_sat(y0r_x, Ar_x, kr_x, tfit + x0r)
    meas_fit_t = f_sat(y0m_x, Am_x, km_x, tfit + x0m)

    # Error function (already a function; we will NOT fit/plot an error-fit line in Figure 1)
    err = f_sat(y0r_x, Ar_x, kr_x, tg + x0r) - f_sat(y0m_x, Am_x, km_x, tg + x0m)

    # Fit sat-exp to error (needed for compensation)
    e0, Ae, ke = fit_sat_exp(tg, err, K_RANGE_ERR, K_STEPS_ERR)

    # Convert measured fit to standard t-domain form for inversion:
    # y_m(t) = f_sat(y0m_x, Am_x, km_x, t + x0m)
    # => y_m(t) = y0m_t + Am_t*(1-exp(-km*t))
    y0m_t = float(f_sat(y0m_x, Am_x, km_x, np.array([x0m]))[0])
    Am_t = float(Am_x * np.exp(-km_x * x0m))
    km_t = float(km_x)
    
    print("\n=== REGRESSIONS (t-domain) ===")

    print_sat_exp(
        "Real signal regression (t-domain)",
        f_sat(y0r_x, Ar_x, kr_x, np.array([x0r]))[0],
        Ar_x * np.exp(-kr_x * x0r),
        kr_x,
        var="t"
    )

    print_sat_exp(
        "Measured signal regression (t-domain)",
        y0m_t, Am_t, km_t, var="t"
    )

    # Compensate measured samples using ONLY measured values
    y_comp = compensate_measured_value(ym, y0m_t, Am_t, km_t, e0, Ae, ke)

    # Final residual error after compensation (on measured sample times)
    # Compare to real-fit curve at those same "t" sample positions
    y_real_at_tm = f_sat(y0r_x, Ar_x, kr_x, tm + x0r)
    final_err = y_real_at_tm - y_comp

    # Masks for plotting only t in [0,2]
    mr = (tr >= T_MIN) & (tr <= T_MAX)
    mm = (tm >= T_MIN) & (tm <= T_MAX)

    # =======================
    # FIGURE 1: signals + error
    # =======================
    fig1, (ax_sig, ax_err) = plt.subplots(
        2, 1, sharex=True, figsize=(9, 7),
        gridspec_kw={"height_ratios": [3, 1]}
    )

    # Top: samples + fits (colors exactly as requested)
    ax_sig.scatter(tr[mr], yr[mr], s=18, color=REAL_COLOR, label="Drone hastighed - målinger")
    ax_sig.plot(tfit, real_fit_t, lw=2, color=REAL_COLOR, label="Drone hastighed - regression")

    ax_sig.scatter(tm[mm], ym[mm], s=18, color=MEAS_COLOR, label="Hjul hastighed - målinger")
    ax_sig.plot(tfit, meas_fit_t, lw=2, color=MEAS_COLOR, label="Hjul hastighed - regression")

    ax_sig.set_xlim(T_MIN, T_MAX)
    ax_sig.set_ylabel("Hastighed [m/s]")
    ax_sig.set_title("Drone og hjul hastighed")
    ax_sig.grid(True)
    ax_sig.legend(loc="best")

    # Bottom: error ONLY (no error fit)
    ax_err.plot(tg, err, lw=2, label="Fejl (forskel mellem drone- og hjul hastigheds regression)")
    ax_err.set_xlim(T_MIN, T_MAX)
    ax_err.set_xlabel("t [s]")
    ax_err.set_ylabel("Hastighed [m/s]")
    ax_err.grid(True)
    ax_err.legend(loc="best")

    plt.tight_layout()
    plt.show()

    # =======================
    # FIGURE 2: compensated vs real + final error
    # =======================
    fig2, (ax_comp, ax_ferr) = plt.subplots(
        2, 1, sharex=True, figsize=(9, 7),
        gridspec_kw={"height_ratios": [3, 1]}
    )

    # Top: real fit (blue) + compensated samples (orange)
    ax_comp.scatter(tm[mm], y_comp[mm], s=30, color=MEAS_COLOR, alpha=1.0, edgecolors="none", antialiased=False, label="Kompenseret hjul hastighed - målinger")
    ax_comp.plot(tfit, real_fit_t, lw=1, color=REAL_COLOR, label="Drone hastighed - regression")

    ax_comp.set_xlim(T_MIN, T_MAX)
    ax_comp.set_ylabel("Hastighed [m/s]")
    ax_comp.set_title("Kompenseret målinger")
    ax_comp.grid(True)
    ax_comp.legend(loc="best")

    # Bottom: final error after compensation (orange points)
    ax_ferr.scatter(tm[mm], final_err[mm], s=18, color=MEAS_COLOR, label="Fejl (forskel mellem drone hastighed regression og kompenseret hjul hastighed målinger)")
    ax_ferr.set_xlim(T_MIN, T_MAX)
    ax_ferr.set_xlabel("t [s]")
    ax_ferr.set_ylabel("Hastighed [m/s]")
    ax_ferr.grid(True)
    ax_ferr.legend(loc="best")

    plt.tight_layout()
    plt.show()

    # =======================
    # Print compensation function (closed form)
    # =======================
    print("\n=== Compensation function (closed form) ===")
    print("Input : y_m (measured value)")
    print("Output: y_comp (compensated value)\n")

    print("t(y_m) = -(1/km) * ln(1 - (y_m - y0m)/Am)")
    print(f"t(y_m) = -(1/{km_t:.6g}) * ln(1 - (y_m - {y0m_t:.6g})/{Am_t:.6g})\n")

    print("e(t) = e0 + Ae*(1 - exp(-ke*t))")
    print(f"e(t) = {e0:.6g} + {Ae:.6g}*(1 - exp(-{ke:.6g}*t))\n")

    print("y_comp(y_m) = y_m + e(t(y_m))")
    print("Expanded:")
    print(
        "y_comp(y_m) = y_m + "
        f"{e0:.6g} + {Ae:.6g}*(1 - exp("
        f"-{ke:.6g} * (-(1/{km_t:.6g}) * ln(1 - (y_m - {y0m_t:.6g})/{Am_t:.6g}))"
        "))"
    )


if __name__ == "__main__":
    main()