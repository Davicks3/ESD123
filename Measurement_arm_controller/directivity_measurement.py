from analog_discovery import AnalogDiscovery
import numpy as np
from scipy.signal import butter, sosfiltfilt, find_peaks
import time


class DirectivityMeasurement:
    def __init__(self):
        self.device = AnalogDiscovery()
        self.device.sampling_frequency = 1_000_000
        self.device.buffer_size = int(self.device.sampling_frequency*1/40_000*50) # 50 cycles.
        self.device.open()
        self.device.full_reset()
        self.device.init_input(
            offset=0.0,
            amplitude_range=1.0)
    
    
    def measure(self):
        # Measure background noise
        while True:
            background_data = self.device.record(1)

            # Calculate value for background noise
            background_level = self._calc_peak_to_peak(background_data)

            # Ensure background noise is lower than some threshold
            if background_level < 0.1:
                break
        
        # Generate sender signal
        self._gen_signal()
        
        # Measure recieved signal
        signal_data = self.device.record(1)
        
        # Calculate value for recieved signal
        return self._calc_peak_to_peak(signal_data)
    
    def _calc_peak_to_peak(self,
                        signal_tuple,
                        f_low=35e3,
                        f_high=45e3,
                        order=4,
                        f_main=40e3):
        
        y, t = signal_tuple
        y = np.asarray(y, dtype=float)
        t = np.asarray(t, dtype=float)

        # --- sample rate from times ---
        dt = np.diff(t).mean()
        fs = 1.0 / dt

        # --- bandpass design (Butterworth) ---
        nyq = 0.5 * fs
        low = f_low / nyq
        high = f_high / nyq
        sos = butter(order, [low, high], btype="bandpass", output="sos")

        # zero-phase filtering
        y_filt = sosfiltfilt(sos, y)

        # --- peak / valley detection ---
        # Rough distance: about half a period at the main frequency
        samples_per_period = fs / f_main
        min_distance = max(1, int(0.5 * samples_per_period))

        # Amplitude threshold to ignore noise
        amp_thresh = 0.1 * np.max(np.abs(y_filt))

        # Positive peaks
        peak_idx, _ = find_peaks(y_filt,
                                height=amp_thresh,
                                distance=min_distance)

        # Negative peaks (valleys) -> find peaks in the inverted signal
        valley_idx, _ = find_peaks(-y_filt,
                                height=amp_thresh,
                                distance=min_distance)

        # Make sure we use matching counts so the stats aren’t biased
        n = min(len(peak_idx), len(valley_idx))
        peak_idx = peak_idx[:n]
        valley_idx = valley_idx[:n]

        if n == 0:
            # No peaks found (signal too small / bandpass wrong / etc.)
            return 0.0

        peak_vals = y_filt[peak_idx]
        valley_vals = y_filt[valley_idx]

        mean_peak = np.mean(peak_vals)
        mean_valley = np.mean(valley_vals)

        # your requested definition:
        # average peak-to-peak = |avg(peak)| + |avg(valley)|
        vpp_avg = abs(mean_peak) + abs(mean_valley)
        
        return vpp_avg
    
    def _gen_signal(self):
        self.device.generate(channel=1,
                offset=0.0,
                frequency=40_000,
                amplitude=1.5,
                symmetry=50,
                wait=0.3, # meas time: around 0.3s
                run_time=0.05,
                repeat=1)
    
    def close(self):
        self.device.close()
    
if __name__ == "__main__":
    obj = DirectivityMeasurement()
    print(obj.measure())
    obj.close()