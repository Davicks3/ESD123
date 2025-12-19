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
    Peak finder with your state-machine idea.
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
    rises = 0
    fall_count = 0
    in_post_rise = False

    last = x[start_idx]

    for i in range(start_idx + 1, N):
        v = x[i]
        diff = v - last

        if not in_post_rise:
            # RISING PHASE + detecting first non-rise
            if diff > 0:
                rises += 1
                if x[i] > x[candidate_idx]:
                    candidate_idx = i
            else:
                # equal or fall
                if rises >= 2:
                    in_post_rise = True
                    fall_count = 1 if diff < 0 else 0
                else:
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
                            # merge two close peaks: keep stronger
                            if abs(x[candidate_idx]) > abs(x[last_peak]):
                                filtered_peaks.append(last_peak)
                                peaks[-1] = candidate_idx
                            else:
                                filtered_peaks.append(candidate_idx)
                        else:
                            peaks.append(candidate_idx)
                    else:
                        peaks.append(candidate_idx)

                    if len(peaks) >= max_peaks:
                        break

                    rises = 0
                    fall_count = 0
                    in_post_rise = False
                    candidate_idx = i
            elif diff == 0:
                # plateau: stay in post-rise; does not count as fall
                pass
            else:
                # new rising segment
                rises = 1
                candidate_idx = i
                fall_count = 0
                in_post_rise = False

        last = v

    return peaks, filtered_peaks


# --------- 3-point parabolic refinement ---------
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

    x = load_signal("signal_noisy.data")
    N = len(x)
    print("Loaded", N, "samples")

    start_idx, coarse_n, fine_n = detect_start(x)
    start_time_us = (start_idx / FS) * 1e6
    print("Start index:", start_idx)
    print("Start time:  {:.3f} us".format(start_time_us))
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

    # --------- Sub-sample interpolation and peak-to-peak times ---------
    refined = []
    for p in peaks:
        ref_idx, ref_val = parabolic_refine(x, p)
        t_abs_us = (ref_idx / FS) * 1e6
        t_rel_us = t_abs_us - start_time_us
        coarse_amp = x[p]
        refined.append((p, ref_idx, t_abs_us, t_rel_us, coarse_amp, ref_val))

    # chronological order by refined index
    refined.sort(key=lambda r: r[1])

    # compute time between peaks (using refined times, in us)
    dt_list_us = []
    last_t_us = None
    for _, _, t_abs_us, _, _, _ in refined:
        if last_t_us is None:
            dt_list_us.append(0.0)  # first peak: no previous
        else:
            dt_list_us.append(t_abs_us - last_t_us)
        last_t_us = t_abs_us

    print()
    print("Peaks with sub-sample interpolation (times in us):")
    print("Idx  Coarse_i  Ref_idx      t_abs [us]     t_rel [us]   dT_prev [us]   Coarse_amp      Refined_amp")
    print("----------------------------------------------------------------------------------------------------")
    for k, ((coarse_i, ref_i, t_abs_us, t_rel_us, coarse_amp, ref_amp), dt_us) in enumerate(zip(refined, dt_list_us), start=1):
        print(
            f"{k:3d}  {coarse_i:8d}  {ref_i:9.4f}  "
            f"{t_abs_us:11.3f}  {t_rel_us:11.3f}  {dt_us:12.3f}  "
            f"{coarse_amp:12.8f}  {ref_amp:12.8f}"
        )

    # Optional summary of peak-to-peak
    if len(dt_list_us) > 1:
        valid_dts = [d for d in dt_list_us[1:] if d > 0]
        if valid_dts:
            avg_dt = sum(valid_dts) / len(valid_dts)
            print()
            print("Peak-to-peak stats (us):")
            print("  min dt: {:.3f} us".format(min(valid_dts)))
            print("  max dt: {:.3f} us".format(max(valid_dts)))
            print("  avg dt: {:.3f} us".format(avg_dt))

    # --------- Plot cropping ----------
    if peaks:
        last_peak = peaks[-1]
    else:
        last_peak = start_idx

    plot_start = max(0, start_idx - PLOT_MARGIN)
    plot_end = min(N - 1, last_peak + PLOT_MARGIN)

    xs = x[plot_start:plot_end + 1]
    # time axis in microseconds
    t_us = [((plot_start + i) / FS) * 1e6 for i in range(len(xs))]

    # energy curve in same region (for reference)
    energies = []
    for i in range(plot_start, plot_end + 1):
        if i + W <= N:
            energies.append(sumsq_window(x, i))
        else:
            energies.append(0.0)

    # accepted peaks (coarse) in plotting window
    peak_times_us = []
    peak_vals = []
    for p in peaks:
        if plot_start <= p <= plot_end:
            peak_times_us.append((p / FS) * 1e6)
            peak_vals.append(x[p])

    # refined peaks in plotting window
    ref_times_us = []
    ref_vals = []
    for _, ref_i, t_abs_us, _, _, ref_amp in refined:
        if plot_start <= int(round(ref_i)) <= plot_end:
            ref_times_us.append(t_abs_us)
            ref_vals.append(ref_amp)

    # filtered peaks
    filt_times_us = []
    filt_vals = []
    for p in filtered_peaks:
        if plot_start <= p <= plot_end:
            filt_times_us.append((p / FS) * 1e6)
            filt_vals.append(x[p])

    start_time_us = (start_idx / FS) * 1e6

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 6), sharex=True)

    # top: signal + start + peaks
    ax1.plot(t_us, xs, label="signal")
    if plot_start <= start_idx <= plot_end:
        ax1.axvline(start_time_us, color="red", linestyle="--", label="start")

    if peak_times_us:
        ax1.scatter(peak_times_us, peak_vals, marker="o", label="peaks (coarse)")

    if ref_times_us:
        ax1.scatter(ref_times_us, ref_vals, marker="x", label="peaks (refined)")

    if filt_times_us:
        ax1.scatter(filt_times_us, filt_vals, marker="^", label="filtered peaks")

    ax1.set_ylabel("Amplitude")
    ax1.set_title("Signal (cropped) with detected start and peaks")
    ax1.grid(True)
    ax1.legend()

    # bottom: energy (sum of squares)
    ax2.plot(t_us, energies)
    if plot_start <= start_idx <= plot_end:
        ax2.axvline(start_time_us, color="red", linestyle="--")
    ax2.set_xlabel("Time [us]")
    ax2.set_ylabel("Sum of squares (W=%d)" % W)
    ax2.set_title("Energy (cropped region)")
    ax2.grid(True)

    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()