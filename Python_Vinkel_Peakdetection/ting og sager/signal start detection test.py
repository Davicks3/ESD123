import os
import math
import time
import matplotlib.pyplot as plt

FS = 192000.0  # sampling frequency


def load_signal(path: str):
    """Load samples from 'signal.data'. One value per line, comma decimal."""
    samples = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            s = s.replace(",", ".")
            try:
                v = float(s)
                samples.append(v)
            except ValueError:
                pass
    return samples


def mean_and_std(values):
    n = len(values)
    if n < 1:
        return 0.0, 0.0
    m = 0.0
    for v in values:
        m += v
    m /= float(n)
    var = 0.0
    for v in values:
        d = v - m
        var += d * d
    var /= float(n)
    return m, math.sqrt(var)


# =========================
# Method 1: Envelope detector
# =========================

def detect_start_envelope(samples,
                          noise_len=300,
                          alpha=0.9,
                          k_sigma=4.0,
                          consec_needed=4):
    """
    Envelope onset detector:
       1) DC remove from first noise_len samples
       2) e[n] = α e[n-1] + (1-α)*|x[n]|
       3) adaptive threshold: mean + kσ noise
       4) require consec matches + positive slope

    Returns: (start_idx, env_array, dc_removed_samples)
             Execution time (µs)
    """
    N = len(samples)
    if N <= noise_len:
        return -1, [0.0]*N, samples[:], 0.0

    # timing start
    t0 = time.perf_counter()

    # 1) DC remove
    dc_mean, _ = mean_and_std(samples[:noise_len])
    x0 = [s - dc_mean for s in samples]

    # 2) Envelope
    env = [0.0]*N
    e_prev = 0.0
    for i, x in enumerate(x0):
        a = abs(x)
        e = alpha * e_prev + (1.0 - alpha) * a
        env[i] = e
        e_prev = e

    # 3) Noise stats
    env_noise = env[:noise_len]
    env_mean, env_std = mean_and_std(env_noise)
    threshold = env_mean + k_sigma * env_std
    low_threshold = env_mean + 2.0 * env_std

    # 4) Detect
    consec = 0
    candidate = -1
    prev_e = env[noise_len - 1]

    for i in range(noise_len, N):
        e = env[i]
        slope = e - prev_e
        prev_e = e

        if e > threshold and slope > 0.0:
            if consec == 0:
                candidate = i
            consec += 1

            if consec >= consec_needed:
                # backward refine
                j = candidate
                while j > noise_len and env[j - 1] > low_threshold:
                    j -= 1
                dt_us = (time.perf_counter() - t0) * 1e6
                return j, env, x0, dt_us
        else:
            consec = 0
            candidate = -1

    dt_us = (time.perf_counter() - t0) * 1e6
    return -1, env, x0, dt_us


# =========================
# Method 2: Sliding RMS
# =========================

def detect_start_rms(samples,
                     noise_len=300,
                     window=32,
                     k_sigma=4.0,
                     consec_needed=4):
    """
    Sliding RMS onset detector:
       1) DC remove
       2) RMS[n] = sqrt(sum(x[k]^2) / window)
       3) threshold
       4) require slope>0 and consec hits

    Returns: (start_idx, rms_array, dc_removed_samples)
             Execution time (µs)
    """
    N = len(samples)
    if N <= noise_len or N < window:
        return -1, [0.0]*N, samples[:], 0.0

    t0 = time.perf_counter()

    # 1) DC
    dc_mean, _ = mean_and_std(samples[:noise_len])
    x0 = [s - dc_mean for s in samples]

    # 2) Sliding RMS
    rms = [0.0]*N
    sumsq = 0.0
    for i in range(N):
        v = x0[i]
        sumsq += v*v
        if i >= window:
            old = x0[i-window]
            sumsq -= old*old

        if i >= window - 1:
            rms[i] = math.sqrt(sumsq/float(window))
        else:
            rms[i] = 0.0

    # 3) noise stats on RMS
    start_noise_idx = window - 1
    end_noise_idx = noise_len - 1
    if end_noise_idx <= start_noise_idx:
        end_noise_idx = start_noise_idx + 1

    rms_noise = rms[start_noise_idx:end_noise_idx]
    rms_mean, rms_std = mean_and_std(rms_noise)
    threshold = rms_mean + k_sigma * rms_std
    low_threshold = rms_mean + 2.0 * rms_std

    # 4) detect
    prev_r = rms[start_noise_idx]
    consec = 0
    candidate = -1

    for i in range(start_noise_idx+1, N):
        r = rms[i]
        slope = r - prev_r
        prev_r = r

        if r > threshold and slope > 0.0:
            if consec == 0:
                candidate = i
            consec += 1
            if consec >= consec_needed:
                j = candidate
                while j > start_noise_idx and rms[j-1] > low_threshold:
                    j -= 1
                dt_us = (time.perf_counter() - t0) * 1e6
                return j, rms, x0, dt_us
        else:
            consec = 0
            candidate = -1

    dt_us = (time.perf_counter() - t0) * 1e6
    return -1, rms, x0, dt_us


# =========================
# Plot helper
# =========================

def plot_detection(time, signal, idx, title):
    plt.figure()
    plt.plot(time, signal)
    if 0 <= idx < len(time):
        plt.axvline(time[idx], linestyle="--")
    plt.xlabel("Time [s]")
    plt.ylabel("Amplitude")
    plt.title(title)
    plt.grid(True)


# =========================
# MAIN
# =========================

def main():
    path = "signal_noisy.data"
    if not os.path.exists(path):
        print("File not found:", path)
        return

    samples = load_signal(path)
    N = len(samples)
    print("Loaded", N, "samples")

    time_axis = [i/FS for i in range(N)]

    # Envelope
    start_env, env, x_env, t_env = detect_start_envelope(samples)
    print(f"Envelope method: index={start_env}, time={t_env:.2f} µs")
    plot_detection(time_axis, x_env, start_env, "Envelope start detection")

    # RMS
    start_rms, rms, x_rms, t_rms = detect_start_rms(samples)
    print(f"RMS method: index={start_rms}, time={t_rms:.2f} µs")
    plot_detection(time_axis, x_rms, start_rms, "RMS start detection")

    plt.show()


if __name__ == "__main__":
    main()