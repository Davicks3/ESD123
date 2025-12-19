#!/usr/bin/env python3
import re
import sys
import serial
import matplotlib.pyplot as plt

# ====== CONFIG ======
PORT = "/dev/cu.usbserial-1140"   # <-- change this
BAUD = 115200
N_INTERVALS = 100_000
BINS = 50
CSV_PATH = "interval_measurements.csv"
# =====================

PATTERN = re.compile(r"Sent:\s*([0-9]+)")
MICROS_WRAP = 2**32


def main():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1.0)
    except serial.SerialException as e:
        print(f"Could not open {PORT}: {e}")
        sys.exit(1)

    intervals = []
    prev_ts = None

    print(f"Listening on {PORT} @ {BAUD}")
    print(f"Capturing {N_INTERVALS} intervals...\n")

    try:
        while len(intervals) < N_INTERVALS:
            line = ser.readline()
            if not line:
                continue

            text = line.decode(errors="ignore").strip()
            m = PATTERN.search(text)

            if not m:
                continue

            t = int(m.group(1))

            if prev_ts is None:
                prev_ts = t
                continue

            dt = t - prev_ts
            if dt < 0:
                dt += MICROS_WRAP  # micros wrap

            prev_ts = t
            intervals.append(dt)

            if len(intervals) % 100 == 0:
                print(f"Captured {len(intervals)} / {N_INTERVALS}", end="\r")

    except KeyboardInterrupt:
        print("\nStopped manually.")
    finally:
        ser.close()

    print("\nDone.")
    print(f"Min interval = {min(intervals)} us")
    print(f"Max interval = {max(intervals)} us")
    print(f"Mean = {sum(intervals)/len(intervals):.2f} us")

    # Save CSV
    with open(CSV_PATH, "w") as f:
        for v in intervals:
            f.write(f"{v}\n")

    print(f"Saved {len(intervals)} intervals to {CSV_PATH}")

    # Plot histogram
    plt.figure()
    plt.hist(intervals, bins=BINS)
    plt.title("Inter-send Interval Histogram")
    plt.xlabel("Interval [µs]")
    plt.ylabel("Count")
    plt.grid(True, alpha=0.4)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()