from analog_discovery import AnalogDiscovery
import matplotlib.pyplot as plt


sampling_freq = 1_000_000
measure_cycles = 25
sending_freq = 40_000.0
measure_time = 10e-3 # s -> 10ms
buffer_size = int(sampling_freq*1.0/sending_freq*measure_cycles)
print("Sample frequency:", sampling_freq)
print("Measurement duration:", measure_time)
print("Buffer size:", buffer_size)

device = AnalogDiscovery()

print("Opening port...")
device.open()

device.full_reset()

print("Initializing device...")
device.init_input(sampling_frequency=sampling_freq,
            buffer_size=buffer_size,
            offset=0.0,
            amplitude_range=1.0)

print("Sending pulses...")
device.generate(channel=1,
                offset=0.0,
                frequency=sending_freq,
                amplitude=1.0,
                symmetry=50,
                wait=0,
                run_time=0,
                repeat=0)

print("Measuring...")
data = device.record(1)
device.reset_input()
device.close()

data_vals, times = data
import numpy as np
arr = np.array(data_vals)
print("samples:", len(arr))
print("min, max, mean, std:", arr.min(), arr.max(), arr.mean(), arr.std())

# Plot full trace (time on x, voltage on y)
plt.figure(figsize=(10,4))
plt.plot(times, data_vals)
plt.xlabel("Time (s)")
plt.ylabel("Voltage (V)")
plt.title("Analog Discovery capture — channel 1")
plt.grid(True)
plt.tight_layout()
plt.show()