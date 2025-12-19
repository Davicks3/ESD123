import os
import math
import matplotlib.pyplot as plt

FS = 192000.0
NOISE_SAMPLES = 30
W = 8          # window size
J = 64         # coarse jump
K_SIGMA = 10.0  # detection threshold factor


def load_signal(path):
    x = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            s = line.strip().replace(",", ".")
            if not s: continue
            try:
                x.append(float(s))
            except:
                pass
    return x


def mean_std(vals):
    n = len(vals)
    m = sum(vals)/n
    v = sum((a-m)*(a-m) for a in vals)/n
    return m, math.sqrt(v)


def sumsq_window(x, i):
    # sum of squares over x[i..i+W-1]
    s = 0.0
    for k in range(W):
        v = x[i+k]
        s += v*v
    return s


def detect_start(x):
    N = len(x)
    # 1) noise stats
    noise = x[:NOISE_SAMPLES]
    sq = [v*v for v in noise]
    mu_s, sigma_s = mean_std(sq)

    # window-domain stats
    mu_sum = mu_s * W
    sigma_sum = sigma_s * math.sqrt(W)
    T_sum = mu_sum + K_SIGMA * sigma_sum
    print("Threshold: ", T_sum)

    # 2) coarse search
    coarse_checked = 0
    fine_checked = 0

    start = NOISE_SAMPLES
    coarse_hit = -1

    p = start
    while p + W <= N:
        E = sumsq_window(x, p)
        coarse_checked += 1
        if E >= T_sum:
            coarse_hit = p
            break
        p += J

    if coarse_hit < 0:
        return -1, coarse_checked, fine_checked

    # 3) fine search backwards range
    refine_start = max(start, coarse_hit - J)

    best = -1
    for i in range(refine_start, coarse_hit+1):
        E = sumsq_window(x, i)
        fine_checked += 1
        if E >= T_sum:
            best = i
            break

    return best, coarse_checked, fine_checked


def main():
    x = load_signal("signal_noisy.data")
    idx, coarse_n, fine_n = detect_start(x)

    print("Coarse checks:", coarse_n)
    print("Fine checks:  ", fine_n)
    print("Detected index:", idx)
    if idx >= 0:
        print("Detected time:", idx/FS, "s")

    # plotting (optional)
    t = [i/FS for i in range(len(x))]
    energies = [sumsq_window(x, i) if i+W<=len(x) else 0 for i in range(len(x))]

    fig, (ax1, ax2) = plt.subplots(2,1,figsize=(10,6),sharex=True)
    ax1.plot(t, x)
    if idx>=0: ax1.axvline(idx/FS,color='red')
    ax1.set_title("Signal")
    ax1.grid(True)

    ax2.plot(t, energies)
    if idx>=0: ax2.axvline(idx/FS,color='red')
    ax2.set_title("Energy (sum sq)")
    ax2.grid(True)

    plt.show()


if __name__ == "__main__":
    main()