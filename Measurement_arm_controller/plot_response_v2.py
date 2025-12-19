import csv
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D

# ==========================
# CONFIG – EDIT THESE
# ==========================

CSV_PATH = "response_banger.csv"      # path to your saved measurement CSV

YAW_RANGE_DEG = 30       # symmetric range around 0
PITCH_RANGE_DEG = 30

PITCH_OFFSET_DEG = -9
YAW_OFFSET_DEG = 0


# ==========================
# LOAD CSV
# ==========================

def load_grid(path):
    grid = []
    with open(path, "r", newline="") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue
            vals = []
            for x in row:
                x = x.strip()
                if x == "" or x.lower() == "none":
                    vals.append(np.nan)
                else:
                    vals.append(float(x))
            grid.append(vals)
    return np.array(grid, dtype=float)

values = load_grid(CSV_PATH)   # shape: (n_yaw, n_pitch), values in volts
n_yaw, n_pitch = values.shape
print("Grid shape:", values.shape)

# ==========================
# BUILD ANGLE GRIDS
# ==========================

yaw_angles = np.linspace(-YAW_RANGE_DEG, YAW_RANGE_DEG, n_yaw) + YAW_OFFSET_DEG
pitch_angles = np.linspace(-PITCH_RANGE_DEG, PITCH_RANGE_DEG, n_pitch) + PITCH_OFFSET_DEG

# Meshgrid in angle space
YAW, PITCH = np.meshgrid(yaw_angles, pitch_angles, indexing="ij")  # same shape as values

# ==========================
# CONVERT VOLTAGE → dB (max = 0 dB)
# ==========================

finite_vals = values[np.isfinite(values)]
Vmax = np.max(finite_vals)
dB = 20 * np.log10(values / Vmax)     # max value becomes 0 dB

# ==========================
# 3D SURFACE PLOT: angles vs dB
# ==========================

fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')

mask = np.isfinite(dB)
YAW_plot = np.ma.masked_where(~mask, YAW)
PITCH_plot = np.ma.masked_where(~mask, PITCH)
dB_plot = np.ma.masked_where(~mask, dB)

surf = ax.plot_surface(
    YAW_plot,            # X-axis: yaw angle (deg)
    PITCH_plot,          # Y-axis: pitch angle (deg)
    dB_plot,             # Z-axis: level (dB)
    cmap='viridis',
    linewidth=0,
    antialiased=True,
    shade=True
)

ax.set_xlabel("Yaw (°)")
ax.set_ylabel("Pitch (°)")
ax.set_zlabel("Level (dB re max)")
ax.set_title("Speaker Directivity (max = 0 dB)")

# Limits in angles & dB
ax.set_xlim(yaw_angles.min(), yaw_angles.max())
ax.set_ylim(pitch_angles.min(), pitch_angles.max())
ax.set_zlim(np.nanmin(dB), 0)   # top at 0 dB

# Colorbar in dB
mappable = plt.cm.ScalarMappable(cmap='viridis')
mappable.set_array(dB[np.isfinite(dB)])
cbar = plt.colorbar(mappable, shrink=0.6, pad=0.1)
cbar.set_label("Level (dB re max)")

plt.tight_layout()
plt.show()


# ==========================
# ANGULAR RING STATISTICS
# ==========================

# Angular distance from center (0°,0°) in degrees
angle_radius = np.sqrt(YAW**2 + PITCH**2)

# Define rings in degrees: 5–10, 10–15, 15–20, 20–25, 25–30
bin_edges = np.arange(5, 31, 5)  # [5, 10, 15, 20, 25, 30]

print("Ring stats (distance from center in degrees):")
print("  Ring [deg]    Npts   max[dB]   median[dB]   min[dB]")
for r1, r2 in zip(bin_edges[:-1], bin_edges[1:]):
    # mask points that fall into this ring and are finite
    mask = (angle_radius >= r1) & (angle_radius < r2) & np.isfinite(dB)
    vals = dB[mask]
    if vals.size == 0:
        print(f"  {r1:2.0f}–{r2:2.0f}       0      n/a        n/a        n/a")
        continue

    ring_max = np.max(vals)
    ring_min = np.min(vals)
    ring_median = np.median(vals)

    print(f"  {r1:2.0f}–{r2:2.0f}   {vals.size:5d}   {ring_max:8.2f}   {ring_median:10.2f}   {ring_min:8.2f}")