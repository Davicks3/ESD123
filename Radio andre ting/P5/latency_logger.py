#!/usr/bin/env python3
import re
import sys
import serial
import matplotlib.pyplot as plt

# ====== CONFIG ======
PORT = "/dev/cu.usbserial-1140"   # Change to your port
BAUD = 115200                     # Change if needed
N_SAMPLES = 10_000                  # Number of latency samples to capture
BINS = 50                         # Histogram bins
SAVE_CSV_PATH = "latencies.csv"              # e.g. "latencies.csv" or None
# =====================

PATTERN = re.compile(r"\[testTimer:\s*([0-9]+)\s*us\]")


def main():
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1.0)
    except serial.SerialException as e:
        print(f"Failed to open serial port {PORT}: {e}")
        sys.exit(1)

    latencies = []

    print(f"Listening on {PORT} @ {BAUD} baud")
    print(f"Target samples: {N_SAMPLES}")
    print("Press Ctrl+C to stop early.\n")

    try:
        while len(latencies) < N_SAMPLES:
            line = ser.readline()
            if not line:
                continue

            text = line.decode(errors="ignore").strip()

            m = PATTERN.search(text)
            if not m:
                # Ignore unrelated / startup prints
                continue

            us = int(m.group(1))
            latencies.append(us)

            if len(latencies) % 10 == 0 or len(latencies) == N_SAMPLES:
                print(f"Captured {len(latencies)}/{N_SAMPLES} samples", end="\r")

    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        ser.close()

    print()  # newline after progress print

    if not latencies:
        print("No latency samples captured. Are you printing [testTimer: ... us] on the ESP32?")
        return

    print(f"Captured {len(latencies)} samples.")
    print(f"Min:  {min(latencies)} us")
    print(f"Max:  {max(latencies)} us")
    print(f"Mean: {sum(latencies)/len(latencies):.2f} us")

    if SAVE_CSV_PATH:
        with open(SAVE_CSV_PATH, "w", encoding="utf-8") as f:
            for v in latencies:
                f.write(f"{v}\n")
        print(f"Saved latencies to {SAVE_CSV_PATH}")

    plt.figure()
    plt.hist(latencies, bins=BINS)
    plt.title("Latency histogram")
    plt.xlabel("Latency [µs]")
    plt.ylabel("Count")
    plt.grid(True, linestyle="--", alpha=0.5)
    plt.tight_layout()
    plt.show()


if __name__ == "__main__":
    main()