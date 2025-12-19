#!/usr/bin/env python3
import math

# ==========================
# CONFIGURATION
# ==========================

LUT_SIZE = 1024              # Number of samples for 0..π/2
HEADER_FILE = "LUT.h"
ARRAY_NAME = "SIN_LUT"
ARRAY_TYPE = "int16_t"
USE_PROGMEM = True           # Set False if you don't want PROGMEM
HEADER_GUARD = "_SIN_LUT_H_"

# ==========================
# GENERATE VALUES (Q15)
# ==========================

values = []
for i in range(LUT_SIZE):
    # Angle range 0 .. pi/2
    angle = (math.pi * 0.5) * (i / (LUT_SIZE - 1))
    s = math.sin(angle)
    # convert to Q15 fixed-point
    q15 = int(round(s * 32767.0))
    # Clamp
    if q15 > 32767:
        q15 = 32767
    if q15 < -32768:
        q15 = -32768
    values.append(q15)

print(f"Generated LUT with {LUT_SIZE} entries.")
print(f"First sample: {values[0]}, last: {values[-1]}")


# ==========================
# FORMAT C ARRAY
# ==========================

def format_array(vals, per_line=8):
    lines = []
    line = []
    for i, v in enumerate(vals):
        line.append(str(v))
        if (i + 1) % per_line == 0:
            lines.append(", ".join(line))
            line = []
    if line:
        lines.append(", ".join(line))
    return ",\n    ".join(lines)


array_body = format_array(values, per_line=8)


# ==========================
# BUILD HEADER CONTENT
# ==========================

h = []

h.append(f"#ifndef {HEADER_GUARD}")
h.append(f"#define {HEADER_GUARD}")
h.append("")
h.append("#include <stdint.h>")
if USE_PROGMEM:
    h.append("#include <pgmspace.h>")
h.append("")
h.append(f"#define SIN_LUT_SIZE {LUT_SIZE}")
h.append("")
storage = "PROGMEM " if USE_PROGMEM else ""
h.append(f"static const {ARRAY_TYPE} {ARRAY_NAME}[SIN_LUT_SIZE] {storage}= {{")
h.append(f"    {array_body}")
h.append("};")
h.append("")
h.append(f"#endif // {HEADER_GUARD}")
h.append("")

# ==========================
# WRITE FILE
# ==========================

with open(HEADER_FILE, "w") as f:
    f.write("\n".join(h))

print(f"Wrote {HEADER_FILE}")