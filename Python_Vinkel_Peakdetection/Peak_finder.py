from DataFrame import DataFrame
import matplotlib.pyplot as plt
import numpy as np

"""
The idea, is to find the two points nearest to the a peak, then interpolating
the signal near that peak, and finally finding the maximum using Newtons method.

This ensures, that we get the actual peaks of the signal, instead of the sampled peak
which is unlikely to be the actual peak. Another benefit of this, is that we can find
the true delay, instead of the delay +- sample time. This is achieved, since we aren't
restricted by the time of the sample anymore.
"""

class PeakFinder:
    def __init__(self, signal_interval):
        self.prev_err = None
        self.prev_t = None
        self.verbose_debug = False
        self.signal_interval = signal_interval # debug
    
    def find_peaks(self, dataframe: DataFrame):
        peaks = []
        for signal in dataframe.df:
            peaks.append(self._find_peak(signal, dataframe.sample_t))
        
        return peaks
    
    def _find_peak(self, signal, sample_t, support=2, inter_support=5):
        self.debug_signal = signal
        index_peaks = self._identify_peak_indicies(signal, support=support)
        
        peaks = []
        for A, B in index_peaks:
            self.debug_signal_range = (A-inter_support, B+inter_support+1)
            peak = self._find_true_peak(signal[A-inter_support: B+inter_support+1], sample_t, (A-inter_support)*sample_t)
            if peak:
                peaks.append(peak)
        
        return peaks
        
    def show_signal(self, signal, title, interpolate=False, sample_t=1.0, oversample=10):
        """
        Shows:
        1) Full debug signal (self.debug_signal) with highlighted segment and A/B points.
        2) Interpolated (sinc) reconstruction of the current segment (and samples).
        3) Interpolated derivative below it.

        Arguments:
            signal: partial signal segment currently analyzed.
            title: figure title.
            interpolate: if True, also plot interpolated value & derivative.
            sample_t: sample time for proper scaling.
            oversample: oversampling factor for interpolation.
        """
        
        if self.debug_signal_range[0] < self.signal_interval[0] or self.debug_signal_range[1] > self.signal_interval[1]:
            return
        
        
        N = len(signal)
        ts = np.arange(N) * sample_t

        # Identify A/B points (the two middle samples)
        mid = N // 2
        if N % 2 == 0:
            mid_indices = [mid - 1, mid]
        else:
            mid_indices = [mid - 1, mid] if mid > 0 else [0, 1]

        # --- FULL SIGNAL PLOT SETUP ---
        has_full = hasattr(self, "debug_signal") and hasattr(self, "debug_signal_range")
        n_plots = 3 if interpolate and has_full else (2 if interpolate else 1)
        fig, axes = plt.subplots(n_plots, 1, figsize=(9, 3*n_plots), sharex=False)
        if n_plots == 1:
            axes = [axes]
        fig.suptitle(title, fontsize=12)

        ax_idx = 0

        # (1) --- Full signal plot ---
        if has_full:
            full_signal = np.array(self.debug_signal)
            full_t = np.arange(len(full_signal)) * sample_t
            sel_start, sel_end = self.debug_signal_range

            axes[ax_idx].plot(full_t, full_signal, lw=1.2, label="Full signal", color="C0")
            axes[ax_idx].axvspan(sel_start * sample_t, sel_end * sample_t,
                                color="C2", alpha=0.15, label="Analyzed segment")

            # show segment samples (orange)
            seg_t = np.arange(sel_start, sel_end + 1) * sample_t
            seg_vals = full_signal[sel_start:sel_end + 1]
            axes[ax_idx].plot(seg_t, seg_vals, "o", color="orange", label="Segment samples")

            # highlight A/B points in red
            ab_global = [sel_start + mid_indices[0], sel_start + mid_indices[1]]
            axes[ax_idx].plot(np.array(ab_global) * sample_t,
                            full_signal[ab_global], "o", color="red", label="A/B points")

            axes[ax_idx].set_ylabel("Amplitude")
            axes[ax_idx].set_title("Full signal with segment highlight")
            axes[ax_idx].grid(True, alpha=0.3)
            axes[ax_idx].legend(loc="best")
            ax_idx += 1

        # (2) --- Partial/interpolated signal plot ---
        if interpolate:
            tf = np.linspace(0.0, (N - 1) * sample_t, (N - 1) * oversample + 1)
            vf = np.array([self._calc_interpolated_value(signal, sample_t, t) for t in tf])
            df = np.array([self._calc_interpolated_derivative(signal, sample_t, t) for t in tf])

            axes[ax_idx].plot(tf, vf, lw=1.6, label="Interpolated (sinc)", color="C0")
            axes[ax_idx].stem(ts, signal, linefmt="C1-", markerfmt="C1o", basefmt=" ",
                            label="Samples", use_line_collection=True)
            axes[ax_idx].scatter(ts[mid_indices], np.array(signal)[mid_indices],
                                color="red", zorder=5, label="A/B samples")
            axes[ax_idx].set_ylabel("Amplitude")
            axes[ax_idx].set_title("Interpolated segment")
            axes[ax_idx].grid(True, alpha=0.3)
            axes[ax_idx].legend(loc="best")
            ax_idx += 1

            # (3) --- Derivative plot ---
            axes[ax_idx].plot(tf, df, lw=1.6, label="Interpolated derivative", color="C4")
            axes[ax_idx].axhline(0.0, color="k", lw=0.8, alpha=0.6)
            axes[ax_idx].set_xlabel("Time [s]")
            axes[ax_idx].set_ylabel("d/dt")
            axes[ax_idx].grid(True, alpha=0.3)
            axes[ax_idx].legend(loc="best")
        else:
            axes[ax_idx].stem(ts, signal, linefmt="C1-", markerfmt="C1o", basefmt=" ")
            axes[ax_idx].scatter(ts[mid_indices], np.array(signal)[mid_indices],
                                color="red", zorder=5, label="A/B samples")
            axes[ax_idx].set_title("Signal segment")
            axes[ax_idx].set_ylabel("Amplitude")
            axes[ax_idx].grid(True, alpha=0.3)
            axes[ax_idx].legend(loc="best")

        plt.tight_layout()
        plt.show()
        
    def _identify_peak_indicies(self, signal, support=2):
        peaks = []
        for i in range(support, len(signal)-support):
            res = True
            res &= all(signal[j] < signal[j+1] for j in range(i-support, i))
            res &= all(signal[j+1] < signal[j] for j in range(i, i+support))
            if res:
                peaks.append(i)
        out = []
        for i in peaks:
            if signal[i-1] > signal[i+1]:
                out.append((i-1, i))
            else:
                out.append((i, i+1))
        return out
                    
                
    
    def _find_true_peak(self, signal, sample_t, offset_t, err_threshold=1e-4, max_tries=100):
        # Ensure there is a peak to detect.
        A_t = (float(len(signal)) / 2.0 - 1.5) * sample_t
        B_t = (float(len(signal)) / 2.0 + 0.5) * sample_t
        A_der = self._calc_interpolated_derivative(signal, sample_t, A_t)
        B_der = self._calc_interpolated_derivative(signal, sample_t, B_t)
        if A_der < -err_threshold or B_der > err_threshold:
            if self.verbose_debug:
                print("A derivative:", A_der)
                print("B derivative:", B_der)
                self.show_signal(signal, "Derivative wrong", True, sample_t)
            return None
        
        
        self.prev_err = None
        t = (float(len(signal)) / 2.0 - 0.5) * sample_t
        err = 0 - self._calc_interpolated_derivative(signal, sample_t, t)
        try_count = 0
        if self.verbose_debug:
            print("Newtons method")
        
        while abs(err) > err_threshold and try_count < max_tries:
            if self.verbose_debug:
                print(f"#{try_count} t: {t}, err: {err}")
            t = self._calc_newton_iterration(err, t)
            err = 0 - self._calc_interpolated_derivative(signal, sample_t, t)
            try_count += 1
        if self.verbose_debug:
            print(f"#{try_count} t: {t}, err: {err}")
        if try_count >= max_tries:
            if self.verbose_debug:
                print("offset t:", offset_t)
                self.show_signal(signal, "Max iterations", True, sample_t)
            
            return None
        
        return (offset_t+t, self._calc_interpolated_value(signal, sample_t, t))
    
    def _calc_interpolated_value(self, signal, sample_t, t):
        out = 0.0
        for i, val in enumerate(signal):
            out += val * self._calc_interpolated_value_partial(t/sample_t-i)
        return out
    
    def _calc_interpolated_value_partial(self, t):
        if t == 0.0:
            return 1.0
        
        pi_t = np.pi * t
        return np.sin(pi_t)/pi_t
    
    def _calc_interpolated_derivative(self, signal, sample_t, t):
        out = 0.0
        for i, val in enumerate(signal):
            out += val * self._calc_interpolated_derivative_partial(t/sample_t-i)/sample_t
        return out
    
    def _calc_interpolated_derivative_partial(self, t):
        if t == 0.0:
            return 0.0
        
        pi_t = np.pi * t
        return (pi_t*np.cos(pi_t)-np.sin(pi_t))/(pi_t*t)
    
    def _calc_newton_iterration(self, err, t, default_step=1e-8, max_step=1e-6):
        if self.prev_err is None:
            self.prev_err = err
            self.prev_t = t
            return t + default_step
        
        err_diff = err - self.prev_err
        t_diff = t - self.prev_t
        self.prev_err = err
        self.prev_t = t
        
        slope = t_diff / err_diff
        
        new_t = t - err * slope
        step = new_t - t
        if abs(step) > max_step:
            step = max_step if step > 0 else -max_step
            new_t = t + step
        return new_t
    