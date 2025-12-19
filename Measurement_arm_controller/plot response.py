import csv
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # needed for 3D plotting

# ==========================
# CONFIG – EDIT THESE
# ==========================

CSV_PATH = "response_banger.csv"      # path to your saved measurement CSV

# These should match what you used in MeasurementArm
# Example: yaw_range = 90, pitch_range = 90, steps = 5 -> -90..90 in 5° steps
YAW_RANGE_DEG = 30      # symmetric range around 0, e.g. ±90°
PITCH_RANGE_DEG = 30    # symmetric range around 0, e.g. ±90°

# Offsets you mentioned
PITCH_OFFSET_DEG = -9
YAW_OFFSET_DEG = 0

# Normalize radius so max = 1 (usually nice for directivity balloons)
NORMALIZE_RADIUS = True


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

values = load_grid(CSV_PATH)   # shape: (n_yaw, n_pitch)
n_yaw, n_pitch = values.shape
print("Grid shape:", values.shape)

# ==========================
# BUILD ANGLE GRIDS
# ==========================

# Yaw: -YAW_RANGE .. +YAW_RANGE
yaw_angles = np.linspace(-YAW_RANGE_DEG, YAW_RANGE_DEG, n_yaw) + YAW_OFFSET_DEG
# Pitch: -PITCH_RANGE .. +PITCH_RANGE
pitch_angles = np.linspace(-PITCH_RANGE_DEG, PITCH_RANGE_DEG, n_pitch) + PITCH_OFFSET_DEG

# Meshgrid: Y = yaw (around speaker), P = pitch (up/down)
Y, P = np.meshgrid(yaw_angles, pitch_angles, indexing="ij")  # shape (n_yaw, n_pitch)

# ==========================
# CONVERT TO SPHERICAL -> CARTESIAN
# ==========================
# Convention (you can tweak if your angles are defined differently):
# - yaw = azimuth angle θ in the horizontal plane (0° = front, + to one side)
# - pitch = elevation angle φ (0° = horizontal, + up, - down)

theta = np.deg2rad(Y)      # azimuth
phi = np.deg2rad(P)        # elevation

R = values.copy()

if NORMALIZE_RADIUS:
    finite_vals = R[np.isfinite(R)]
    if finite_vals.size > 0:
        R = R / np.max(finite_vals)

# Spherical to Cartesian:
# x = r cosφ cosθ
# y = r cosφ sinθ
# z = r sinφ
x = R * np.cos(phi) * np.cos(theta)
y = R * np.cos(phi) * np.sin(theta)
z = R * np.sin(phi)

# ==========================
# PLOT 3D SURFACE
# ==========================

fig = plt.figure(figsize=(10, 8))
ax = fig.add_subplot(111, projection='3d')

# Plot surface; mask NaNs if any
mask = np.isfinite(R)
x_plot = np.ma.masked_where(~mask, x)
y_plot = np.ma.masked_where(~mask, y)
z_plot = np.ma.masked_where(~mask, z)
r_plot = np.ma.masked_where(~mask, R)

surf = ax.plot_surface(
    x_plot, y_plot, z_plot,
    facecolors=plt.cm.viridis(r_plot),
    linewidth=0, antialiased=True, shade=True
)

# Equal aspect ratio: make it look like a sphere-ish shape
max_range = np.nanmax(np.abs([x, y, z]))
for axis in "xyz":
    getattr(ax, f"set_{axis}lim")([-max_range, max_range])

ax.set_xlabel("X")
ax.set_ylabel("Y")
ax.set_zlabel("Z")
ax.set_title("Speaker Directivity (3D)")

# Add colorbar using a dummy mappable
mappable = plt.cm.ScalarMappable(cmap='viridis')
mappable.set_array(R[np.isfinite(R)])
cbar = plt.colorbar(mappable, shrink=0.6, pad=0.1)
cbar.set_label("Normalized magnitude")

plt.tight_layout()
plt.show()