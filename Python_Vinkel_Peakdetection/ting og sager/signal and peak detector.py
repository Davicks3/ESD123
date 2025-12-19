#!/usr/bin/env python3
import os
import math
import matplotlib.pyplot as plt

FS = 192000.0
NOISE_SAMPLES = 10
W = 8          # window size for sum-of-squares
J = 64         # coarse jump
K_SIGMA = 10.0  # threshold factor

N_PEAKS = 50
MIN_DIST = 4      # min distance between peaks (samples)
PLOT_MARGIN = 20  # extra samples before start and after last peak


def load_signal(path):
    x = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip().replace(",", ".")
            if not s:
                continue
            try:
                x.append(float(s))
            except ValueError:
                pass
    return x


def mean_std(vals):
    n = len(vals)
    if n == 0:
        return 0.0, 0.0
    m = sum(vals) / n
    v = sum((a - m) * (a - m) for a in vals) / n
    return m, math.sqrt(v)


def sumsq_window(x, i):
    s = 0.0
    for k in range(W):
        v = x[i + k]
        s += v * v
    return s


def detect_start(x):
    """
    Fast start detection using sum of squares with coarse + fine search.
    Returns: (start_idx, coarse_checked, fine_checked)
    """
    N = len(x)
    if N < NOISE_SAMPLES + W:
        return -1, 0, 0

    # 1) noise stats on per-sample squared noise
    noise = x[:NOISE_SAMPLES]
    sq = [v * v for v in noise]
    mu_s, sigma_s = mean_std(sq)

    # 2) scale to window sum-of-squares domain
    mu_sum = mu_s * W
    sigma_sum = sigma_s * math.sqrt(W)
    T_sum = mu_sum + K_SIGMA * sigma_sum

    # 3) coarse search
    coarse_checked = 0
    fine_checked = 0

    start_search = NOISE_SAMPLES
    coarse_hit = -1

    p = start_search
    while p + W <= N:
        E = sumsq_window(x, p)
        coarse_checked += 1
        if E >= T_sum:
            coarse_hit = p
            break
        p += J

    if coarse_hit < 0:
        return -1, coarse_checked, fine_checked

    # 4) fine search in [coarse_hit - J, coarse_hit]
    refine_start = max(start_search, coarse_hit - J)
    best = -1
    for i in range(refine_start, coarse_hit + 1):
        if i + W > N:
            break
        E = sumsq_window(x, i)
        fine_checked += 1
        if E >= T_sum:
            best = i
            break

    return best, coarse_checked, fine_checked


def find_peaks_state_machine(x, start_idx, max_peaks=N_PEAKS, min_dist=MIN_DIST):
    """
    Peak finder with your state-machine idea:

    - Need at least 2 rises (diff > 0).
    - After that, need at least 2 FALLS (diff < 0) to confirm a peak.
    - Equal samples (diff == 0) on top plateau do NOT count as falls.
      They also do not move the candidate peak -> first equal wins.
    - Min distance enforced via merging:
        If new_peak - last_peak < min_dist:
            keep the one with larger |amplitude|,
            push the other one into filtered_peaks.
    Returns:
        peaks          : list of accepted peak indices
        filtered_peaks : list of rejected/merged peak indices
    """
    N = len(x)
    if start_idx < 1 or start_idx >= N:
        return [], []

    peaks = []
    filtered_peaks = []

    candidate_idx = start_idx
    rises = 0         # number of consecutive rises
    fall_count = 0    # number of consecutive falls after plateau
    in_post_rise = False  # True after we've seen enough rises and get first non-rise

    last = x[start_idx]

    for i in range(start_idx + 1, N):
        v = x[i]
        diff = v - last

        if not in_post_rise:
            # RISING PHASE + detecting first non-rise
            if diff > 0:
                # strict rise: update candidate only when value actually increases
                rises += 1
                if x[i] > x[candidate_idx]:
                    candidate_idx = i
                # still haven't started post-rise
            else:
                # equal or fall
                if rises >= 2:
                    # we've had our rises, now entering post-rise region
                    in_post_rise = True
                    # only strict falls count
                    fall_count = 1 if diff < 0 else 0
                else:
                    # not enough rises, reset state
                    rises = 0
                    fall_count = 0
                    in_post_rise = False
                    candidate_idx = i
        else:
            # POST-RISE PHASE: waiting for at least 2 FALLS (diff < 0)
            if diff < 0:
                fall_count += 1
                if fall_count >= 2:
                    # Confirm peak at candidate_idx
                    if peaks:
                        last_peak = peaks[-1]
                        d = candidate_idx - last_peak
                        if d < min_dist:
                            # merge two close peaks: keep stronger, save weaker as filtered
                            if abs(x[candidate_idx]) > abs(x[last_peak]):
                                # new is stronger, old one is filtered
                                filtered_peaks.append(last_peak)
                                peaks[-1] = candidate_idx
                            else:
                                # old is stronger, new one is filtered
                                filtered_peaks.append(candidate_idx)
                        else:
                            peaks.append(candidate_idx)
                    else:
                        peaks.append(candidate_idx)

                    if len(peaks) >= max_peaks:
                        break

                    # reset for next peak search
                    rises = 0
                    fall_count = 0
                    in_post_rise = False
                    candidate_idx = i
            elif diff == 0:
                # equal: stay in post-rise, but does NOT count as a fall
                # candidate stays as first sample at plateau
                pass
            else:
                # diff > 0: new rising segment starts
                rises = 1
                candidate_idx = i
                fall_count = 0
                in_post_rise = False

        last = v

    return peaks, filtered_peaks


# --------- NEW: parabolic peak refinement ---------
def parabolic_refine(x, idx):
    """
    Refine a peak index using 3-point parabolic interpolation.

    Uses samples (idx-1, idx, idx+1).
    Returns:
        (ref_idx, ref_val)  with float index and interpolated amplitude.
    """
    N = len(x)
    if idx <= 0 or idx >= N - 1:
        return float(idx), float(x[idx])

    y_m1 = x[idx - 1]
    y_0 = x[idx]
    y_p1 = x[idx + 1]

    denom = (y_m1 - 2.0 * y_0 + y_p1)
    if denom == 0.0:
        return float(idx), float(y_0)

    # Offset of parabola vertex relative to idx
    offset = 0.5 * (y_m1 - y_p1) / denom
    ref_idx = idx + offset

    # Evaluate parabola at ref_idx using local coordinates (x=0 at idx)
    a = 0.5 * (y_m1 - 2.0 * y_0 + y_p1)
    b = 0.5 * (y_p1 - y_m1)
    xloc = offset
    ref_val = a * xloc * xloc + b * xloc + y_0

    return float(ref_idx), float(ref_val)


def main():
    if not os.path.exists("signal_clean.data"):
        print("signal_clean.data not found")
        return

    x = load_signal("signal_clean.data")
    N = len(x)
    print("Loaded", N, "samples")

    start_idx, coarse_n, fine_n = detect_start(x)
    print("Start index:", start_idx)
    print("Coarse checks:", coarse_n)
    print("Fine checks:  ", fine_n)

    if start_idx < 0:
        print("No start detected.")
        return

    # --------- Peak detection (state machine) ---------
    peaks, filtered_peaks = find_peaks_state_machine(x, start_idx)
    print("Number of accepted peaks:", len(peaks))
    print("Number of filtered peaks:", len(filtered_peaks))
    print("First few accepted peaks:", peaks[:10])
    print("First few filtered peaks:", filtered_peaks[:10])

    # --------- Sub-sample interpolation and ordered list ---------
    t_start = start_idx / FS

    refined = []
    for p in peaks:
        ref_idx, ref_val = parabolic_refine(x, p)
        t_abs = ref_idx / FS
        t_rel = t_abs - t_start
        coarse_amp = x[p]
        refined.append((p, ref_idx, t_abs, t_rel, coarse_amp, ref_val))

    # Ensure chronological order by refined index
    refined.sort(key=lambda r: r[1])

    print()
    print("Peaks with sub-sample interpolation (chronological):")
    print("Idx  Coarse_i  Ref_idx      t_abs [s]     t_rel [s]     Coarse_amp      Refined_amp")
    print("---------------------------------------------------------------------------------------")
    for k, (coarse_i, ref_i, t_abs, t_rel, coarse_amp, ref_amp) in enumerate(refined, start=1):
        print(
            f"{k:3d}  {coarse_i:8d}  {ref_i:9.4f}  "
            f"{t_abs:11.8f}  {t_rel:11.8f}  "
            f"{coarse_amp:12.8f}  {ref_amp:12.8f}"
        )

    # --------- Plot cropping ----------
    if peaks:
        last_peak = peaks[-1]
    else:
        last_peak = start_idx

    plot_start = max(0, start_idx - PLOT_MARGIN)
    plot_end = min(N - 1, last_peak + PLOT_MARGIN)

    xs = x[plot_start:plot_end + 1]
    t = [(plot_start + i) / FS for i in range(len(xs))]

    # energy curve in same region (for reference)
    energies = []
    for i in range(plot_start, plot_end + 1):
        if i + W <= N:
            energies.append(sumsq_window(x, i))
        else:
            energies.append(0.0)

    # shift peak and start positions into local plotting indices
    peak_times = []
    peak_vals = []
    for p in peaks:
        if plot_start <= p <= plot_end:
            peak_times.append(p / FS)
            peak_vals.append(x[p])

    # Optional: refined peak positions in plot range
    ref_times = []
    ref_vals = []
    for _, ref_i, t_abs, _, _, ref_amp in refined:
        # check if the *nearest* integer index falls in plot range
        if plot_start <= int(round(ref_i)) <= plot_end:
            ref_times.append(t_abs)
            ref_vals.append(ref_amp)

    filt_times = []
    filt_vals = []
    for p in filtered_peaks:
        if plot_start <= p <= plot_end:
            filt_times.append(p / FS)
            filt_vals.append(x[p])

    start_time = start_idx / FS

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 6), sharex=True)

    # top: signal + start + peaks
    ax1.plot(t, xs, label="signal")
    if plot_start <= start_idx <= plot_end:
        ax1.axvline(start_time, color="red", linestyle="--", label="start")

    # accepted peaks (coarse)
    if peak_times:
        ax1.scatter(peak_times, peak_vals, marker="o", label="peaks (coarse)")

    # refined peaks
    if ref_times:
        ax1.scatter(ref_times, ref_vals, marker="x", label="peaks (refined)")

    # filtered peaks
    if filt_times:
        ax1.scatter(filt_times, filt_vals, marker="^", label="filtered peaks")

    ax1.set_ylabel("Amplitude")
    ax1.set_title("Signal (cropped) with detected start and peaks")
    ax1.grid(True)
    ax1.legend()

    # bottom: energy (sum of squares)
    ax2.plot(t, energies)
    if plot_start <= start_idx <= plot_end:
        ax2.axvline(start_time, color="red", linestyle="--")
    ax2.set_xlabel("Time [s]")
    ax2.set_ylabel("Sum of squares (W=%d)" % W)
    ax2.set_title("Energy (cropped region)")
    ax2.grid(True)

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()