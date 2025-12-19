import math

N = 1024
print("#pragma once")
print("#include <stdint.h>")
print()
print(f"#define SIN_LUT_SIZE {N}")
print()
print("// Quarter-wave sine lookup table: 0 .. PI/2")
print(f"static const float SIN_LUT[{N}] = {{")

for i in range(N):
    theta = (math.pi / 2.0) * i / (N - 1)
    value = math.sin(theta)
    if i < N - 1:
        print(f"    {value:.9f},")
    else:
        print(f"    {value:.9f}")

print("};")