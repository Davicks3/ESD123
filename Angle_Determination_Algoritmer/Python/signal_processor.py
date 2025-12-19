import numpy as np
import matplotlib.pyplot as plt
from typing import Tuple, Optional
import warnings


class SignalProcessor:
    """
    Object-oriented signal processor for delay estimation using cross-correlation.
    
    Generates two similar signals with random delay, applies noise, filters,
    interpolates, and estimates delay via cross-correlation.
    """
    
    def __init__(self, fs: int = 192_000, f0: int = 40_000, cycles: int = 10, 
                 noise_level: float = 0.2, random_seed: Optional[int] = None):
        """
        Initialize the signal processor.
        
        Args:
            fs: Sampling frequency (Hz)
            f0: Signal frequency (Hz)
            cycles: Number of cycles in the signal
            noise_level: Noise amplitude relative to signal
            random_seed: Random seed for reproducibility
        """
        self.fs = fs
        self.f0 = f0
        self.cycles = cycles
        self.noise_level = noise_level
        self.duration = cycles / f0
        self.N = int(np.floor(fs * self.duration))
        
        # Set up random number generator
        self.rng = np.random.default_rng(random_seed)
        
        # Time vector
        self.t = np.arange(self.N, dtype=float) / fs
        
        # Envelope parameters
        self.tau = 0.1 * self.duration
        self.envelope = 1.0 - np.exp(-self.t / self.tau)
        
        # Storage for generated signals
        self.signal1_clean = None
        self.signal2_clean = None
        self.signal1_noisy = None
        self.signal2_noisy = None
        self.signal1_filtered = None
        self.signal2_filtered = None
        self.signal1_interpolated = None
        self.signal2_interpolated = None
        self.true_delay = None
        self.estimated_delay = None
        self.correlation_error = None
        
    @staticmethod
    def sinc_pi(x: np.ndarray) -> np.ndarray:
        """Return sin(pi*x)/(pi*x), with proper handling at x=0 (limit=1)."""
        out = np.empty_like(x, dtype=float)
        zero_mask = (x == 0)
        out[zero_mask] = 1.0
        nz = ~zero_mask
        out[nz] = np.sin(np.pi * x[nz]) / (np.pi * x[nz])
        return out
    
    def generate_base_signal(self) -> np.ndarray:
        """Generate clean base signal with envelope."""
        return self.envelope * np.sin(2 * np.pi * self.f0 * self.t)
    
    def add_noise(self, signal: np.ndarray) -> np.ndarray:
        """Add Gaussian noise to signal."""
        noise = self.noise_level * self.rng.standard_normal(len(signal))
        return signal + noise
    
    def apply_delay(self, signal: np.ndarray, delay_samples: int) -> np.ndarray:
        """Apply delay to signal by shifting samples."""
        if delay_samples <= 0:
            return signal
        
        delayed_signal = np.zeros_like(signal)
        if delay_samples < len(signal):
            delayed_signal[delay_samples:] = signal[:-delay_samples]
        
        return delayed_signal
    
    def generate_signal_pair(self, max_delay_us: float = 145.0) -> Tuple[float, int]:
        """
        Generate two similar signals with random delay between them.
        Second signal is recorded for max_delay longer to capture the delayed version.
        
        Args:
            max_delay_us: Maximum delay in microseconds (0 to max_delay_us)
            
        Returns:
            Tuple of (true_delay_us, delay_samples)
        """
        # Generate base clean signal
        base_signal = self.generate_base_signal()
        
        # Extend both signals with 20 zero samples after the original signal
        base_signal_extended = np.concatenate([base_signal, np.zeros(20)])
        
        # Generate random delay (0 to max_delay_us)
        delay_us = self.rng.uniform(0, max_delay_us)
        delay_samples = int(np.round(delay_us * 1e-6 * self.fs))
        max_delay_samples = int(np.round(max_delay_us * 1e-6 * self.fs))
        
        # Signal 1: Extended signal with zeros
        self.signal1_clean = base_signal_extended.copy()
        
        # Update time vector to match the extended signal length
        self.t = np.arange(len(base_signal_extended), dtype=float) / self.fs
        
        # Signal 2: Extended to record max_delay longer, then apply actual delay
        # First extend the base signal by max_delay_samples
        extended_time = np.arange(len(base_signal_extended) + max_delay_samples, dtype=float) / self.fs
        extended_envelope = 1.0 - np.exp(-extended_time / self.tau)
        extended_base_signal = extended_envelope * np.sin(2 * np.pi * self.f0 * extended_time)
        
        # Apply the actual delay by shifting
        self.signal2_clean = np.zeros(len(extended_base_signal))
        self.signal2_clean[delay_samples:delay_samples + len(base_signal_extended)] = base_signal_extended
        
        # Add noise to both signals
        self.signal1_noisy = self.add_noise(self.signal1_clean)
        self.signal2_noisy = self.add_noise(self.signal2_clean)
        
        # Store true delay and extended parameters
        self.true_delay = delay_us
        self.max_delay_samples = max_delay_samples
        
        return delay_us, delay_samples
    
    def design_fir_bandpass(self, lowcut: float, highcut: float, numtaps: int = 101, 
                           normalize_freq: Optional[float] = None, fs: Optional[float] = None) -> np.ndarray:
        """
        Design FIR bandpass filter using windowed-sinc method.
        
        Args:
            lowcut: Low cutoff frequency (Hz)
            highcut: High cutoff frequency (Hz) 
            numtaps: Number of filter taps (odd number)
            normalize_freq: Frequency to normalize gain at (Hz)
            fs: Sampling frequency (Hz), defaults to self.fs
            
        Returns:
            Filter coefficients
        """
        # Use provided sampling rate or default to original fs
        sampling_rate = fs if fs is not None else self.fs
        
        if numtaps % 2 == 0:
            numtaps += 1
        M = numtaps // 2
        n = np.arange(-M, M + 1, dtype=float)

        # Ideal lowpass at fc: 2*(fc/fs)*sinc(2*(fc/fs)*n)
        def ideal_lp(fc):
            wc = fc / sampling_rate  # normalized cycles/sample
            return 2.0 * wc * self.sinc_pi(2.0 * wc * n)

        h_lp_low = ideal_lp(lowcut)
        h_lp_high = ideal_lp(highcut)
        h_ideal = h_lp_high - h_lp_low  # ideal bandpass

        # Hamming window
        w = 0.54 - 0.46 * np.cos(2.0 * np.pi * (np.arange(numtaps)) / (numtaps - 1))
        h = h_ideal * w

        # Optional: normalize gain near a frequency
        if normalize_freq is not None:
            k = np.arange(numtaps) - M
            ang = -2.0 * np.pi * normalize_freq * k / sampling_rate
            resp = np.sum(h * np.exp(1j * ang))
            g = np.real(resp)
            if g != 0:
                h = h / g
        return h
    
    @staticmethod
    def linear_convolve(x: np.ndarray, h: np.ndarray) -> np.ndarray:
        """Manual linear convolution without np.convolve."""
        Nx = len(x)
        Nh = len(h)
        y = np.zeros(Nx + Nh - 1, dtype=float)
        for n in range(Nx):
            y[n:n+Nh] += x[n] * h
        return y
    
    def apply_bandpass_filter(self, signal: np.ndarray, lowcut: float = 38_000, 
                             highcut: float = 42_000, fs: Optional[float] = None) -> np.ndarray:
        """Apply bandpass filter to signal."""
        # Use provided sampling rate or default to original fs
        sampling_rate = fs if fs is not None else self.fs
        
        taps = self.design_fir_bandpass(lowcut, highcut, numtaps=101, 
                                       normalize_freq=self.f0, fs=sampling_rate)
        
        # Full convolution
        y_full = self.linear_convolve(signal, taps)
        
        # Trim to "same" length (center)
        M = (len(taps) - 1) // 2
        y_same = y_full[M:M+len(signal)]
        
        return y_same
    
    def sinc_interpolate(self, x: np.ndarray, up_factor: int = 8) -> Tuple[np.ndarray, np.ndarray, float]:
        """
        Ideal sinc interpolation for upsampling.
        
        Args:
            x: Input signal
            up_factor: Upsampling factor
            
        Returns:
            Tuple of (time_vector, interpolated_signal, new_sampling_rate)
        """
        Nloc = len(x)
        Ts = 1.0 / self.fs
        fs_new = self.fs * up_factor
        Ts_new = 1.0 / fs_new

        # Time grid: include final original instant n=(Nloc-1)
        M_new = (Nloc - 1) * up_factor + 1
        t_new = np.arange(M_new, dtype=float) * Ts_new

        # Direct evaluation
        y_new = np.zeros(M_new, dtype=float)
        for m in range(M_new):
            tm = t_new[m]
            u = (tm / Ts) - np.arange(Nloc, dtype=float)
            y_new[m] = np.sum(x * self.sinc_pi(u))
        
        return t_new, y_new, fs_new
    
    def process_signals(self, up_factor: int = 8) -> None:
        """
        Apply complete signal processing pipeline to both signals.
        
        Args:
            up_factor: Interpolation upsampling factor
        """
        if self.signal1_noisy is None or self.signal2_noisy is None:
            raise ValueError("Must generate signal pair first using generate_signal_pair()")
        
        # Apply interpolation first
        t1_up, signal1_interpolated_temp, fs_up = self.sinc_interpolate(self.signal1_noisy, up_factor)
        t2_up, signal2_interpolated_temp, _ = self.sinc_interpolate(self.signal2_noisy, up_factor)
        
        # Store interpolated sampling rate
        self.fs_interpolated = fs_up
        
        # Apply bandpass filtering to interpolated signals using interpolated sampling rate
        self.signal1_filtered = self.apply_bandpass_filter(signal1_interpolated_temp, lowcut=38_000, highcut=42_000, fs=fs_up)
        self.signal2_filtered = self.apply_bandpass_filter(signal2_interpolated_temp, lowcut=38_000, highcut=42_000, fs=fs_up)
        
        # The final interpolated signals are the filtered versions
        self.signal1_interpolated = self.signal1_filtered
        self.signal2_interpolated = self.signal2_filtered
        
        # Store interpolated time vector and sampling rate
        self.t_interpolated = t1_up
        self.fs_interpolated = fs_up
    
    def estimate_delay_by_correlation(self, max_search_samples: Optional[int] = None) -> Tuple[float, float, int]:
        """
        Estimate delay between signals using cross-correlation by signal subtraction.
        Only consider the length of the first signal for comparison.
        
        Args:
            max_search_samples: Maximum number of samples to search (default: max delay range)
            
        Returns:
            Tuple of (estimated_delay_us, correlation_error, optimal_shift_samples)
        """
        if self.signal1_interpolated is None or self.signal2_interpolated is None:
            raise ValueError("Must process signals first using process_signals()")
        
        sig1 = self.signal1_interpolated
        sig2 = self.signal2_interpolated
        sig1_len = len(sig1)
        
        # Default search range based on the max delay used in signal generation
        if max_search_samples is None:
            if hasattr(self, 'max_delay_samples'):
                # Convert original max delay samples to interpolated samples
                up_factor = self.fs_interpolated / self.fs
                max_search_samples = int(self.max_delay_samples * up_factor)
            else:
                max_search_samples = len(sig1) // 4
        
        min_error = float('inf')
        best_shift = 0
        errors = []
        shifts = []
        
        # Search over possible delays (shift represents how much to shift sig2)
        for shift in range(0, max_search_samples + 1):
            # Extract portion of sig2 starting at 'shift' with length of sig1
            if shift + sig1_len <= len(sig2):
                sig2_portion = sig2[shift:shift + sig1_len]
                
                # Calculate mean squared error between sig1 and this portion of sig2
                error = np.mean((sig1 - sig2_portion) ** 2)
                errors.append(error)
                shifts.append(shift)
                
                if error < min_error:
                    min_error = error
                    best_shift = shift
        
        # Convert shift in interpolated samples back to delay in microseconds
        delay_samples_interpolated = best_shift  # positive shift means sig2 is delayed
        delay_samples_original = delay_samples_interpolated / (self.fs_interpolated / self.fs)
        estimated_delay_us = delay_samples_original / self.fs * 1e6
        
        # Store results
        self.estimated_delay = estimated_delay_us
        self.correlation_error = min_error
        self.correlation_errors = np.array(errors)
        self.correlation_shifts = np.array(shifts)
        
        return estimated_delay_us, min_error, best_shift
    
    def get_delay_estimation_results(self) -> dict:
        """
        Get complete delay estimation results.
        
        Returns:
            Dictionary containing true delay, estimated delay, error, and accuracy metrics
        """
        if self.true_delay is None or self.estimated_delay is None:
            raise ValueError("Must run complete pipeline first")
        
        delay_error_us = abs(self.estimated_delay - self.true_delay)
        
        # Handle zero delay case: use absolute error thresholds instead of percentages
        if self.true_delay == 0:
            # For zero delay, accuracy based on how close estimated delay is to zero
            # Consider <1 μs error as excellent, <5 μs as good, etc.
            if delay_error_us < 1.0:
                estimation_accuracy = 95.0  # Excellent
            elif delay_error_us < 2.0:
                estimation_accuracy = 85.0  # Very good
            elif delay_error_us < 5.0:
                estimation_accuracy = 75.0  # Good
            elif delay_error_us < 10.0:
                estimation_accuracy = 60.0  # Fair
            else:
                estimation_accuracy = max(0, 50 - delay_error_us)  # Poor, decreasing with error
            delay_error_percent = float('inf')  # Undefined for zero delay
        else:
            # Normal percentage calculation for non-zero delays
            delay_error_percent = (delay_error_us / self.true_delay) * 100
            estimation_accuracy = max(0, 100 - delay_error_percent)
        
        return {
            'true_delay_us': self.true_delay,
            'estimated_delay_us': self.estimated_delay,
            'delay_error_us': delay_error_us,
            'delay_error_percent': delay_error_percent,
            'correlation_error': self.correlation_error,
            'estimation_accuracy': estimation_accuracy
        }
    
    def plot_signals(self, show_all_stages: bool = True) -> None:
        """
        Plot signals at different processing stages.
        
        Args:
            show_all_stages: If True, show all processing stages; if False, show only final results
        """
        if show_all_stages:
            # Original noisy signals
            plt.figure(figsize=(15, 10))
            
            plt.subplot(2, 3, 1)
            plt.plot(self.t, self.signal1_noisy, 'b-', label='Signal 1', alpha=0.7)
            # Signal 2 is longer, create extended time vector for plotting
            t2_extended = np.arange(len(self.signal2_noisy), dtype=float) / self.fs
            plt.plot(t2_extended, self.signal2_noisy, 'r-', label='Signal 2 (extended)', alpha=0.7)
            plt.xlabel('Time (s)')
            plt.ylabel('Amplitude')
            plt.title('Original Noisy Signals')
            plt.legend()
            plt.grid(True)
            
            # Filtered signals
            plt.subplot(2, 3, 2)
            plt.plot(self.t, self.signal1_filtered, 'b-', label='Signal 1 Filtered', alpha=0.7)
            # Signal 2 filtered is also longer
            t2_filt_extended = np.arange(len(self.signal2_filtered), dtype=float) / self.fs
            plt.plot(t2_filt_extended, self.signal2_filtered, 'r-', label='Signal 2 Filtered (extended)', alpha=0.7)
            plt.xlabel('Time (s)')
            plt.ylabel('Amplitude')
            plt.title('Bandpass Filtered Signals (30-50 kHz)')
            plt.legend()
            plt.grid(True)
            
            # Interpolated signals
            plt.subplot(2, 3, 3)
            plt.plot(self.t_interpolated, self.signal1_interpolated, 'b-', label='Signal 1 Interp', alpha=0.7)
            # Signal 2 interpolated is longer, create its time vector
            t2_interp_extended = np.arange(len(self.signal2_interpolated), dtype=float) / self.fs_interpolated
            plt.plot(t2_interp_extended, self.signal2_interpolated, 'r-', label='Signal 2 Interp (extended)', alpha=0.7)
            plt.xlabel('Time (s)')
            plt.ylabel('Amplitude')
            plt.title(f'Interpolated Signals ({self.fs_interpolated/1000:.0f} kHz)')
            plt.legend()
            plt.grid(True)
            
            # Correlation errors vs shift
            plt.subplot(2, 3, 4)
            if hasattr(self, 'correlation_shifts'):
                # Convert shifts to delay in microseconds
                delay_us_shifts = self.correlation_shifts / (self.fs_interpolated / self.fs) / self.fs * 1e6
                plt.plot(delay_us_shifts, self.correlation_errors, 'g.-')
                plt.axvline(x=self.true_delay, color='r', linestyle='--', label=f'True: {self.true_delay:.1f} μs')
                plt.axvline(x=self.estimated_delay, color='b', linestyle='--', label=f'Est: {self.estimated_delay:.1f} μs')
                plt.xlabel('Delay (μs)')
                plt.ylabel('MSE')
                plt.title('Cross-correlation Error vs Delay')
                plt.legend()
                plt.grid(True)
            
            # Results summary
            plt.subplot(2, 3, 5)
            results = self.get_delay_estimation_results()
            text_str = f"""Results Summary:
True Delay: {results['true_delay_us']:.2f} μs
Estimated: {results['estimated_delay_us']:.2f} μs
Error: {results['delay_error_us']:.2f} μs
Error %: {results['delay_error_percent']:.1f}%
Accuracy: {results['estimation_accuracy']:.1f}%
Corr Error: {results['correlation_error']:.2e}"""
            plt.text(0.1, 0.5, text_str, transform=plt.gca().transAxes, fontsize=10,
                    verticalalignment='center', bbox=dict(boxstyle='round', facecolor='lightblue'))
            plt.axis('off')
            plt.title('Estimation Results')
            
            # Aligned signals after delay correction
            plt.subplot(2, 3, 6)
            # Apply the estimated delay shift to signal 2 for alignment
            estimated_delay_samples_interp = int(self.estimated_delay * 1e-6 * self.fs_interpolated)
            
            # Extract Signal 1 
            sig1_for_comparison = self.signal1_interpolated
            
            # Extract corresponding portion of Signal 2 with estimated delay shift
            if estimated_delay_samples_interp + len(sig1_for_comparison) <= len(self.signal2_interpolated):
                sig2_shifted = self.signal2_interpolated[estimated_delay_samples_interp:
                                                       estimated_delay_samples_interp + len(sig1_for_comparison)]
                
                # Create time vector for comparison (relative to Signal 1's timebase)
                t_compare = self.t_interpolated[:len(sig1_for_comparison)] * 1e6  # Convert to μs
                
                # Plot aligned signals
                plt.plot(t_compare, sig1_for_comparison, 'b-', label='Signal 1', alpha=0.8, linewidth=1.5)
                plt.plot(t_compare, sig2_shifted, 'r--', label='Signal 2 (delay corrected)', alpha=0.8, linewidth=1.5)
                
                # Calculate and show alignment quality
                mse = np.mean((sig1_for_comparison - sig2_shifted) ** 2)
                correlation_coeff = np.corrcoef(sig1_for_comparison, sig2_shifted)[0, 1]
                
                plt.xlabel('Time (μs)')
                plt.ylabel('Amplitude')
                plt.title(f'Aligned Signals\nMSE: {mse:.2e}, Corr: {correlation_coeff:.3f}')
                plt.legend()
                plt.grid(True, alpha=0.3)
                
                # Add text box with delay info
                textstr = f'Est. Delay: {self.estimated_delay:.2f} μs\nTrue Delay: {self.true_delay:.2f} μs'
                props = dict(boxstyle='round', facecolor='wheat', alpha=0.8)
                plt.text(0.02, 0.98, textstr, transform=plt.gca().transAxes, fontsize=9,
                        verticalalignment='top', bbox=props)
            else:
                plt.text(0.5, 0.5, 'Insufficient signal length\nfor delay correction display', 
                        ha='center', va='center', transform=plt.gca().transAxes)
                plt.title('Aligned Signals (Display Error)')
                plt.axis('off')
            
            plt.tight_layout()
            plt.show()
        
        else:
            # Simple comparison plot
            plt.figure(figsize=(12, 4))
            
            plt.subplot(1, 2, 1)
            plt.plot(self.t_interpolated, self.signal1_interpolated, 'b-', label='Signal 1', alpha=0.7)
            # Signal 2 is longer, create its time vector  
            t2_interp_extended = np.arange(len(self.signal2_interpolated), dtype=float) / self.fs_interpolated
            plt.plot(t2_interp_extended, self.signal2_interpolated, 'r-', label='Signal 2 (extended)', alpha=0.7)
            plt.xlabel('Time (s)')
            plt.ylabel('Amplitude')
            plt.title('Final Processed Signals')
            plt.legend()
            plt.grid(True)
            
            plt.subplot(1, 2, 2)
            results = self.get_delay_estimation_results()
            labels = ['True Delay', 'Estimated Delay']
            values = [results['true_delay_us'], results['estimated_delay_us']]
            colors = ['red', 'blue']
            
            bars = plt.bar(labels, values, color=colors, alpha=0.7)
            plt.ylabel('Delay (μs)')
            plt.title(f'Delay Comparison\n(Error: {results["delay_error_us"]:.2f} μs)')
            
            # Add value labels on bars
            for bar, value in zip(bars, values):
                plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + max(values)*0.01,
                        f'{value:.2f} μs', ha='center', va='bottom')
            
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.show()
    
    def run_complete_analysis(self, max_delay_us: float = 145.0, up_factor: int = 8,
                             plot_results: bool = True, verbose: bool = True) -> dict:
        """
        Run complete signal processing and delay estimation pipeline.
        
        Args:
            max_delay_us: Maximum random delay in microseconds
            up_factor: Interpolation upsampling factor
            plot_results: Whether to plot results
            verbose: Whether to print results
            
        Returns:
            Dictionary with complete results
        """
        # Step 1: Generate signal pair with random delay
        true_delay, delay_samples = self.generate_signal_pair(max_delay_us)
        
        # Step 2: Process signals (filter and interpolate)
        self.process_signals(up_factor)
        
        # Step 3: Estimate delay using cross-correlation
        estimated_delay, corr_error, best_shift = self.estimate_delay_by_correlation()
        
        # Step 4: Get results
        results = self.get_delay_estimation_results()
        
        if verbose:
            print(f"Signal Processing Complete:")
            print(f"  True Delay: {results['true_delay_us']:.3f} μs")
            print(f"  Estimated Delay: {results['estimated_delay_us']:.3f} μs")
            print(f"  Error: {results['delay_error_us']:.3f} μs ({results['delay_error_percent']:.2f}%)")
            print(f"  Estimation Accuracy: {results['estimation_accuracy']:.2f}%")
            print(f"  Correlation Error: {results['correlation_error']:.2e}")
        
        if plot_results:
            self.plot_signals(show_all_stages=True)
        
        return results