#!/usr/bin/env python3
"""
Comprehensive delay estimation test.

Runs multiple trials across different delays to analyze performance patterns
and provide detailed statistics on estimation accuracy.
"""

from signal_processor import SignalProcessor
import numpy as np
import matplotlib.pyplot as plt
try:
    from scipy import signal as scipy_signal
except ImportError:
    scipy_signal = None
    print("Warning: scipy not available, some debug features will be limited")


def debug_plot_problematic_case(processor, results, trial_number):
    """
    Create detailed debug plot for problematic cases.
    
    Args:
        processor: SignalProcessor instance with processed signals
        results: Results dictionary from run_complete_analysis
        trial_number: Trial number for identification
    """
    print(f"   📊 Plotting debug information for trial {trial_number}...")
    
    fig = plt.figure(figsize=(16, 12))
    fig.suptitle(f'DEBUG: Problematic Case - Trial {trial_number}\n'
                f'True: {results["true_delay_us"]:.3f} μs, '
                f'Estimated: {results["estimated_delay_us"]:.3f} μs, '
                f'Error: {results["delay_error_us"]:.3f} μs', fontsize=14, y=0.98)
    
    # 1. Original noisy signals
    plt.subplot(3, 4, 1)
    plt.plot(processor.t, processor.signal1_noisy, 'b-', label='Signal 1', alpha=0.7)
    t2_extended = np.arange(len(processor.signal2_noisy), dtype=float) / processor.fs
    plt.plot(t2_extended, processor.signal2_noisy, 'r-', label='Signal 2 (extended)', alpha=0.7)
    plt.xlabel('Time (s)')
    plt.ylabel('Amplitude')
    plt.title('Original Noisy Signals')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 2. Filtered signals (now interpolated then filtered)
    plt.subplot(3, 4, 2)
    # Since filtering now happens after interpolation, use interpolated time vectors
    plot_samples_filt = min(1000, len(processor.signal1_filtered))
    t1_filt = processor.t_interpolated[:plot_samples_filt] if hasattr(processor, 't_interpolated') else np.arange(plot_samples_filt) / processor.fs_interpolated
    plt.plot(t1_filt, processor.signal1_filtered[:plot_samples_filt], 'b-', label='Signal 1 Filtered', alpha=0.7)
    
    t2_filt_extended = np.arange(len(processor.signal2_filtered), dtype=float) / processor.fs_interpolated
    plt.plot(t2_filt_extended[:plot_samples_filt], processor.signal2_filtered[:plot_samples_filt], 'r-', label='Signal 2 Filtered', alpha=0.7)
    plt.xlabel('Time (s)')
    plt.ylabel('Amplitude')
    plt.title('Bandpass Filtered Signals (First 1000 samples)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 3. Interpolated signals (zoomed to show detail)
    plt.subplot(3, 4, 3)
    # Show first portion for clarity
    plot_samples = min(1000, len(processor.signal1_interpolated))
    plt.plot(processor.t_interpolated[:plot_samples] * 1e6, 
            processor.signal1_interpolated[:plot_samples], 'b-', label='Signal 1 Interp', alpha=0.8)
    t2_interp_extended = np.arange(len(processor.signal2_interpolated), dtype=float) / processor.fs_interpolated
    plt.plot(t2_interp_extended[:plot_samples] * 1e6, 
            processor.signal2_interpolated[:plot_samples], 'r-', label='Signal 2 Interp', alpha=0.8)
    plt.xlabel('Time (μs)')
    plt.ylabel('Amplitude')
    plt.title('Interpolated Signals (First 1000 samples)')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 4. Correlation error curve
    plt.subplot(3, 4, 4)
    if hasattr(processor, 'correlation_shifts') and hasattr(processor, 'correlation_errors'):
        # Convert shifts to delay in microseconds
        delay_us_shifts = processor.correlation_shifts / (processor.fs_interpolated / processor.fs) / processor.fs * 1e6
        plt.plot(delay_us_shifts, processor.correlation_errors, 'g.-', markersize=4)
        plt.axvline(x=results['true_delay_us'], color='r', linestyle='--', linewidth=2, label=f'True: {results["true_delay_us"]:.1f} μs')
        plt.axvline(x=results['estimated_delay_us'], color='b', linestyle='--', linewidth=2, label=f'Est: {results["estimated_delay_us"]:.1f} μs')
        plt.xlabel('Delay (μs)')
        plt.ylabel('MSE')
        plt.title('Cross-correlation Error Curve')
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # Find and mark the minimum
        min_idx = np.argmin(processor.correlation_errors)
        plt.plot(delay_us_shifts[min_idx], processor.correlation_errors[min_idx], 'go', markersize=8, label='Minimum')
    else:
        plt.text(0.5, 0.5, 'Correlation data not available', ha='center', va='center', transform=plt.gca().transAxes)
        plt.title('Cross-correlation Error Curve')
    
    # 5. Aligned signals comparison
    plt.subplot(3, 4, 5)
    estimated_delay_samples_interp = int(results['estimated_delay_us'] * 1e-6 * processor.fs_interpolated)
    sig1_for_comparison = processor.signal1_interpolated
    
    if estimated_delay_samples_interp + len(sig1_for_comparison) <= len(processor.signal2_interpolated):
        sig2_shifted = processor.signal2_interpolated[estimated_delay_samples_interp:
                                                   estimated_delay_samples_interp + len(sig1_for_comparison)]
        
        # Show first 500 samples for clarity
        plot_samples = min(500, len(sig1_for_comparison))
        t_compare = processor.t_interpolated[:plot_samples] * 1e6
        
        plt.plot(t_compare, sig1_for_comparison[:plot_samples], 'b-', label='Signal 1', alpha=0.8, linewidth=1.5)
        plt.plot(t_compare, sig2_shifted[:plot_samples], 'r--', label='Signal 2 (delay corrected)', alpha=0.8, linewidth=1.5)
        
        mse = np.mean((sig1_for_comparison - sig2_shifted) ** 2)
        correlation_coeff = np.corrcoef(sig1_for_comparison, sig2_shifted)[0, 1]
        
        plt.xlabel('Time (μs)')
        plt.ylabel('Amplitude')
        plt.title(f'Aligned Signals (First 500 samples)\nMSE: {mse:.2e}, Corr: {correlation_coeff:.3f}')
        plt.legend()
        plt.grid(True, alpha=0.3)
    else:
        plt.text(0.5, 0.5, 'Cannot show alignment\n(insufficient signal length)', 
                ha='center', va='center', transform=plt.gca().transAxes)
        plt.title('Aligned Signals')
    
    # 6. Signal power spectral density
    plt.subplot(3, 4, 6)
    if scipy_signal is not None:
        # Compute PSD for both filtered signals
        f1, psd1 = scipy_signal.welch(processor.signal1_filtered, processor.fs)
        f2, psd2 = scipy_signal.welch(processor.signal2_filtered[:len(processor.signal1_filtered)], processor.fs)
        
        plt.semilogy(f1/1000, psd1, 'b-', label='Signal 1', alpha=0.7)
        plt.semilogy(f2/1000, psd2, 'r-', label='Signal 2', alpha=0.7)
        plt.xlabel('Frequency (kHz)')
        plt.ylabel('PSD')
        plt.title('Power Spectral Density')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.xlim(20, 60)  # Focus on signal band
    else:
        # Simple FFT-based PSD
        fft1 = np.abs(np.fft.fft(processor.signal1_filtered))**2
        fft2 = np.abs(np.fft.fft(processor.signal2_filtered[:len(processor.signal1_filtered)]))**2
        freqs = np.fft.fftfreq(len(processor.signal1_filtered), 1/processor.fs)
        
        # Only positive frequencies
        pos_mask = freqs > 0
        plt.semilogy(freqs[pos_mask]/1000, fft1[pos_mask], 'b-', label='Signal 1', alpha=0.7)
        plt.semilogy(freqs[pos_mask]/1000, fft2[pos_mask], 'r-', label='Signal 2', alpha=0.7)
        plt.xlabel('Frequency (kHz)')
        plt.ylabel('FFT Power')
        plt.title('FFT-based PSD')
        plt.legend()
        plt.grid(True, alpha=0.3)
        plt.xlim(20, 60)
    
    # 7. Signal energy over time
    plt.subplot(3, 4, 7)
    # Windowed energy calculation
    window_size = len(processor.signal1_filtered) // 20
    energy1 = []
    energy2 = []
    time_windows = []
    
    for i in range(0, len(processor.signal1_filtered) - window_size, window_size//2):
        energy1.append(np.mean(processor.signal1_filtered[i:i+window_size]**2))
        sig2_window = processor.signal2_filtered[i:i+window_size] if i+window_size <= len(processor.signal2_filtered) else processor.signal2_filtered[i:]
        energy2.append(np.mean(sig2_window**2))
        time_windows.append((i + window_size//2) / processor.fs * 1000)  # Convert to ms
    
    plt.plot(time_windows, energy1, 'b.-', label='Signal 1 Energy', alpha=0.7)
    plt.plot(time_windows, energy2, 'r.-', label='Signal 2 Energy', alpha=0.7)
    plt.xlabel('Time (ms)')
    plt.ylabel('Windowed Energy')
    plt.title('Signal Energy Over Time')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 8. Detailed correlation around minimum
    plt.subplot(3, 4, 8)
    if hasattr(processor, 'correlation_shifts') and hasattr(processor, 'correlation_errors'):
        # Find region around minimum for detailed view
        min_idx = np.argmin(processor.correlation_errors)
        search_range = min(50, len(processor.correlation_errors)//4)
        
        start_idx = max(0, min_idx - search_range)
        end_idx = min(len(processor.correlation_errors), min_idx + search_range)
        
        detail_shifts = delay_us_shifts[start_idx:end_idx]
        detail_errors = processor.correlation_errors[start_idx:end_idx]
        
        plt.plot(detail_shifts, detail_errors, 'g.-', markersize=6)
        plt.axvline(x=results['true_delay_us'], color='r', linestyle='--', linewidth=2, label='True')
        plt.axvline(x=results['estimated_delay_us'], color='b', linestyle='--', linewidth=2, label='Estimated')
        plt.xlabel('Delay (μs)')
        plt.ylabel('MSE')
        plt.title('Detailed Correlation Around Minimum')
        plt.legend()
        plt.grid(True, alpha=0.3)
    
    # 9-12: Additional diagnostic plots
    # 9. Residual after alignment
    plt.subplot(3, 4, 9)
    if estimated_delay_samples_interp + len(sig1_for_comparison) <= len(processor.signal2_interpolated):
        residual = sig1_for_comparison[:plot_samples] - sig2_shifted[:plot_samples]
        plt.plot(t_compare, residual, 'k-', alpha=0.7)
        plt.xlabel('Time (μs)')
        plt.ylabel('Residual')
        plt.title(f'Alignment Residual\nRMS: {np.sqrt(np.mean(residual**2)):.3f}')
        plt.grid(True, alpha=0.3)
    else:
        plt.text(0.5, 0.5, 'Cannot compute residual', ha='center', va='center', transform=plt.gca().transAxes)
        plt.title('Alignment Residual')
    
    # 10. SNR analysis
    plt.subplot(3, 4, 10)
    # Estimate noise from high-frequency content
    noise_est1 = np.std(np.diff(processor.signal1_filtered))
    noise_est2 = np.std(np.diff(processor.signal2_filtered[:len(processor.signal1_filtered)]))
    signal_power1 = np.var(processor.signal1_filtered)
    signal_power2 = np.var(processor.signal2_filtered[:len(processor.signal1_filtered)])
    
    snr1_db = 10 * np.log10(signal_power1 / noise_est1**2) if noise_est1 > 0 else 0
    snr2_db = 10 * np.log10(signal_power2 / noise_est2**2) if noise_est2 > 0 else 0
    
    plt.bar(['Signal 1', 'Signal 2'], [snr1_db, snr2_db], color=['blue', 'red'], alpha=0.7)
    plt.ylabel('Estimated SNR (dB)')
    plt.title('Signal Quality Comparison')
    plt.grid(True, alpha=0.3)
    
    # 11. Cross-correlation properties
    plt.subplot(3, 4, 11)
    if hasattr(processor, 'correlation_errors'):
        plt.hist(processor.correlation_errors, bins=20, alpha=0.7, color='green', edgecolor='black')
        plt.axvline(x=results['correlation_error'], color='red', linestyle='--', linewidth=2, label='This case')
        plt.xlabel('MSE')
        plt.ylabel('Frequency')
        plt.title('Correlation Error Distribution')
        plt.legend()
        plt.grid(True, alpha=0.3)
    
    # 12. Problem summary
    plt.subplot(3, 4, 12)
    plt.axis('off')
    
    # Analyze what might be wrong
    problem_analysis = []
    if results['true_delay_us'] == 0:
        problem_analysis.append("• Zero delay case")
        if results['delay_error_us'] > 5:
            problem_analysis.append("• Large absolute error for zero delay")
        if results['estimated_delay_us'] > 10:
            problem_analysis.append("• Significant false delay detection")
    else:
        if results['delay_error_us'] > 10:
            problem_analysis.append("• Large estimation error")
    
    summary_text = f"""PROBLEM ANALYSIS:

True Delay: {results['true_delay_us']:.3f} μs
Estimated: {results['estimated_delay_us']:.3f} μs
Error: {results['delay_error_us']:.3f} μs

Possible Issues:
{chr(10).join(problem_analysis) if problem_analysis else "• Unknown cause"}

Trial Seed: {trial_number}
(Reproducible with same seed)"""
    
    plt.text(0.05, 0.95, summary_text, transform=plt.gca().transAxes, fontsize=10,
             verticalalignment='top', bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.8))
    
    plt.tight_layout(rect=[0, 0, 1, 0.96])  # Leave space for suptitle
    plt.show()
    
    print(f"   ✅ Debug plot complete for trial {trial_number}")


def comprehensive_delay_test(n_trials=100, max_delay_us=145.0, delay_bins=10, debug_threshold=70.0):
    """
    Run comprehensive delay estimation test across multiple trials and delay ranges.
    
    Args:
        n_trials: Number of trials to run
        max_delay_us: Maximum delay to test
        delay_bins: Number of delay bins to analyze patterns
        debug_threshold: Plot detailed debug info for cases with accuracy below this % (set to None to disable)
        
    Returns:
        Dictionary with comprehensive results
    """
    print(f"🧪 Running comprehensive delay test with {n_trials} trials...")
    print(f"   Max delay: {max_delay_us} μs")
    print(f"   Analyzing patterns in {delay_bins} delay bins")
    if debug_threshold is not None:
        print(f"   🔍 Debug mode: Will plot cases with error > {debug_threshold} μs")
    
    # Storage for results
    all_results = {
        'true_delays': [],
        'estimated_delays': [],
        'errors': [],
        'error_percentages': [],
        'accuracies': [],
        'correlation_errors': []
    }
    
    # Run trials
    for trial in range(n_trials):
        if trial % 20 == 0:  # Progress indicator
            print(f"   Progress: {trial}/{n_trials} trials completed")
            
        # Create processor with different random seed for each trial
        processor = SignalProcessor(
            fs=192_000,
            f0=40_000,
            cycles=10,
            noise_level=0.2,
            random_seed=trial
        )
        
        # Run analysis without plotting
        results = processor.run_complete_analysis(
            max_delay_us=max_delay_us,
            up_factor=8,
            plot_results=False,
            verbose=False
        )
        
        # Check if this case needs debugging
        if results['delay_error_us'] > 10:
            print(f"\n🚨 DEBUG: Found problematic case at trial {trial}")
            print(f"   Error: {results['delay_error_us']:.3f} μs")
            print(f"   True delay: {results['true_delay_us']:.3f} μs")
            print(f"   Estimated delay: {results['estimated_delay_us']:.3f} μs")
            if results['true_delay_us'] == 0:
                print("   Note: Zero delay case - error based on absolute thresholds")
            print("   Additional estimation data:")
            print(f"     - Correlation error: {results['correlation_error']:.3e}")
            print(f"     - Processor parameters: fs={processor.fs}, f0={processor.f0}, cycles={processor.cycles}, noise_level={processor.noise_level}")
            debug_plot_problematic_case(processor, results, trial)
        
        # Store results
        all_results['true_delays'].append(results['true_delay_us'])
        all_results['estimated_delays'].append(results['estimated_delay_us'])
        all_results['errors'].append(results['delay_error_us'])
        all_results['error_percentages'].append(results['delay_error_percent'])
        all_results['accuracies'].append(results['estimation_accuracy'])
        all_results['correlation_errors'].append(results['correlation_error'])
    
    print(f"   ✅ Completed {n_trials} trials!")
    
    # Convert to numpy arrays for analysis
    for key in all_results:
        all_results[key] = np.array(all_results[key])
    
    # Analyze patterns by delay bins
    delay_analysis = analyze_delay_patterns(all_results, delay_bins, max_delay_us)
    
    # Print summary statistics
    print_summary_statistics(all_results)
    
    # Create comprehensive plots
    plot_comprehensive_results(all_results, delay_analysis, max_delay_us)
    
    return all_results, delay_analysis


def analyze_delay_patterns(results, delay_bins, max_delay_us):
    """Analyze error patterns across different delay ranges."""
    
    # Create delay bins
    bin_edges = np.linspace(0, max_delay_us, delay_bins + 1)
    bin_centers = (bin_edges[:-1] + bin_edges[1:]) / 2
    
    # Assign each trial to a delay bin
    bin_indices = np.digitize(results['true_delays'], bin_edges) - 1
    bin_indices = np.clip(bin_indices, 0, delay_bins - 1)  # Handle edge cases
    
    # Calculate statistics for each bin
    bin_stats = {
        'bin_centers': bin_centers,
        'bin_edges': bin_edges,
        'mean_errors': [],
        'std_errors': [],
        'mean_accuracies': [],
        'trial_counts': [],
        'max_errors': [],
        'min_errors': []
    }
    
    for bin_idx in range(delay_bins):
        # Find trials in this bin
        mask = bin_indices == bin_idx
        bin_errors = results['errors'][mask]
        bin_accuracies = results['accuracies'][mask]
        
        if len(bin_errors) > 0:
            bin_stats['mean_errors'].append(np.mean(bin_errors))
            bin_stats['std_errors'].append(np.std(bin_errors))
            bin_stats['mean_accuracies'].append(np.mean(bin_accuracies))
            bin_stats['trial_counts'].append(len(bin_errors))
            bin_stats['max_errors'].append(np.max(bin_errors))
            bin_stats['min_errors'].append(np.min(bin_errors))
        else:
            # No trials in this bin
            bin_stats['mean_errors'].append(0)
            bin_stats['std_errors'].append(0)
            bin_stats['mean_accuracies'].append(0)
            bin_stats['trial_counts'].append(0)
            bin_stats['max_errors'].append(0)
            bin_stats['min_errors'].append(0)
    
    # Convert to numpy arrays
    for key in ['mean_errors', 'std_errors', 'mean_accuracies', 'trial_counts', 'max_errors', 'min_errors']:
        bin_stats[key] = np.array(bin_stats[key])
    
    return bin_stats


def print_summary_statistics(results):
    """Print comprehensive summary statistics."""
    
    print(f"\n📊 SUMMARY STATISTICS:")
    print(f"   Total Trials: {len(results['errors'])}")
    print(f"   Mean Error: {np.mean(results['errors']):.3f} ± {np.std(results['errors']):.3f} μs")
    print(f"   Median Error: {np.median(results['errors']):.3f} μs")
    print(f"   Min Error: {np.min(results['errors']):.3f} μs")
    print(f"   Max Error: {np.max(results['errors']):.3f} μs")
    print(f"   Mean Accuracy: {np.mean(results['accuracies']):.1f} ± {np.std(results['accuracies']):.1f}%")
    
    # Percentile analysis
    p25, p75, p90, p95, p99 = np.percentile(results['errors'], [25, 75, 90, 95, 99])
    print(f"   Error Percentiles:")
    print(f"     25th: {p25:.3f} μs")
    print(f"     75th: {p75:.3f} μs") 
    print(f"     90th: {p90:.3f} μs")
    print(f"     95th: {p95:.3f} μs")
    print(f"     99th: {p99:.3f} μs")
    
    # Performance categories
    excellent = np.sum(results['errors'] < 1.0)
    good = np.sum((results['errors'] >= 1.0) & (results['errors'] < 5.0))
    fair = np.sum((results['errors'] >= 5.0) & (results['errors'] < 10.0))
    poor = np.sum(results['errors'] >= 10.0)
    
    print(f"   Performance Distribution:")
    print(f"     Excellent (<1 μs): {excellent} trials ({100*excellent/len(results['errors']):.1f}%)")
    print(f"     Good (1-5 μs): {good} trials ({100*good/len(results['errors']):.1f}%)")
    print(f"     Fair (5-10 μs): {fair} trials ({100*fair/len(results['errors']):.1f}%)")
    print(f"     Poor (>10 μs): {poor} trials ({100*poor/len(results['errors']):.1f}%)")


def plot_comprehensive_results(results, delay_analysis, max_delay_us):
    """Create comprehensive visualization of results."""
    
    fig = plt.figure(figsize=(20, 12))
    
    # 1. Error histogram
    plt.subplot(3, 4, 1)
    plt.hist(results['errors'], bins=30, alpha=0.7, edgecolor='black', color='skyblue')
    plt.xlabel('Estimation Error (μs)')
    plt.ylabel('Frequency')
    plt.title(f'Error Distribution\n(Mean: {np.mean(results["errors"]):.3f} μs)')
    plt.grid(True, alpha=0.3)
    
    # 2. True vs Estimated delays scatter
    plt.subplot(3, 4, 2)
    plt.scatter(results['true_delays'], results['estimated_delays'], alpha=0.6, s=20, c='blue')
    plt.plot([0, max_delay_us], [0, max_delay_us], 'r--', linewidth=2, label='Perfect Estimation')
    plt.xlabel('True Delay (μs)')
    plt.ylabel('Estimated Delay (μs)')
    plt.title('Estimation Accuracy')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 3. Error vs True delay
    plt.subplot(3, 4, 3)
    plt.scatter(results['true_delays'], results['errors'], alpha=0.6, s=20, c='red')
    plt.xlabel('True Delay (μs)')
    plt.ylabel('Estimation Error (μs)')
    plt.title('Error vs True Delay')
    plt.grid(True, alpha=0.3)
    
    # 4. Accuracy histogram
    plt.subplot(3, 4, 4)
    plt.hist(results['accuracies'], bins=30, alpha=0.7, edgecolor='black', color='lightgreen')
    plt.xlabel('Accuracy (%)')
    plt.ylabel('Frequency')
    plt.title(f'Accuracy Distribution\n(Mean: {np.mean(results["accuracies"]):.1f}%)')
    plt.grid(True, alpha=0.3)
    
    # 5. Mean error by delay bin
    plt.subplot(3, 4, 5)
    plt.errorbar(delay_analysis['bin_centers'], delay_analysis['mean_errors'], 
                yerr=delay_analysis['std_errors'], marker='o', capsize=5, capthick=2, color='orange')
    plt.xlabel('Delay Bin Center (μs)')
    plt.ylabel('Mean Error (μs)')
    plt.title('Mean Error vs Delay Range')
    plt.grid(True, alpha=0.3)
    
    # 6. Accuracy by delay bin
    plt.subplot(3, 4, 6)
    plt.plot(delay_analysis['bin_centers'], delay_analysis['mean_accuracies'], 'o-', 
             linewidth=2, markersize=6, color='green')
    plt.xlabel('Delay Bin Center (μs)')
    plt.ylabel('Mean Accuracy (%)')
    plt.title('Accuracy vs Delay Range')
    plt.grid(True, alpha=0.3)
    
    # 7. Trial count by delay bin
    plt.subplot(3, 4, 7)
    plt.bar(delay_analysis['bin_centers'], delay_analysis['trial_counts'], 
           width=max_delay_us/(len(delay_analysis['bin_centers'])+1), alpha=0.7, color='purple')
    plt.xlabel('Delay Bin Center (μs)')
    plt.ylabel('Number of Trials')
    plt.title('Trial Distribution Across Delays')
    plt.grid(True, alpha=0.3)
    
    # 8. Error range by delay bin
    plt.subplot(3, 4, 8)
    plt.fill_between(delay_analysis['bin_centers'], 
                    delay_analysis['min_errors'], 
                    delay_analysis['max_errors'], 
                    alpha=0.3, label='Error Range', color='lightcoral')
    plt.plot(delay_analysis['bin_centers'], delay_analysis['mean_errors'], 'ro-', label='Mean Error')
    plt.xlabel('Delay Bin Center (μs)')
    plt.ylabel('Error (μs)')
    plt.title('Error Range vs Delay')
    plt.legend()
    plt.grid(True, alpha=0.3)
    
    # 9. Correlation error vs estimation error
    plt.subplot(3, 4, 9)
    plt.scatter(results['correlation_errors'], results['errors'], alpha=0.6, s=20, c='brown')
    plt.xlabel('Correlation Error (MSE)')
    plt.ylabel('Estimation Error (μs)')
    plt.title('Correlation vs Estimation Error')
    plt.grid(True, alpha=0.3)
    
    # 10. Cumulative error distribution
    plt.subplot(3, 4, 10)
    sorted_errors = np.sort(results['errors'])
    cumulative_prob = np.arange(1, len(sorted_errors) + 1) / len(sorted_errors)
    plt.plot(sorted_errors, cumulative_prob * 100, linewidth=2, color='navy')
    plt.xlabel('Estimation Error (μs)')
    plt.ylabel('Cumulative Probability (%)')
    plt.title('Cumulative Error Distribution')
    plt.grid(True, alpha=0.3)
    
    # 11. Performance over trial number (check for trends)
    plt.subplot(3, 4, 11)
    trial_numbers = np.arange(len(results['errors']))
    plt.scatter(trial_numbers, results['errors'], alpha=0.6, s=20, c='teal')
    # Add trend line
    z = np.polyfit(trial_numbers, results['errors'], 1)
    p = np.poly1d(z)
    plt.plot(trial_numbers, p(trial_numbers), "r--", alpha=0.8)
    plt.xlabel('Trial Number')
    plt.ylabel('Estimation Error (μs)')
    plt.title('Error vs Trial Number')
    plt.grid(True, alpha=0.3)
    
    # 12. Box plot of errors by delay quartiles
    plt.subplot(3, 4, 12)
    # Divide delays into quartiles
    delay_quartiles = np.percentile(results['true_delays'], [25, 50, 75])
    q1_mask = results['true_delays'] <= delay_quartiles[0]
    q2_mask = (results['true_delays'] > delay_quartiles[0]) & (results['true_delays'] <= delay_quartiles[1])
    q3_mask = (results['true_delays'] > delay_quartiles[1]) & (results['true_delays'] <= delay_quartiles[2])
    q4_mask = results['true_delays'] > delay_quartiles[2]
    
    quartile_errors = [results['errors'][q1_mask], results['errors'][q2_mask], 
                      results['errors'][q3_mask], results['errors'][q4_mask]]
    quartile_labels = ['Q1\n(Low)', 'Q2', 'Q3', 'Q4\n(High)']
    
    box_plot = plt.boxplot(quartile_errors, labels=quartile_labels, patch_artist=True)
    colors = ['lightblue', 'lightgreen', 'lightyellow', 'lightcoral']
    for patch, color in zip(box_plot['boxes'], colors):
        patch.set_facecolor(color)
    
    plt.ylabel('Estimation Error (μs)')
    plt.title('Error Distribution by Delay Quartile')
    plt.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.show()
    
    # Print delay pattern analysis
    print(f"\n🔍 DELAY PATTERN ANALYSIS:")
    for i, center in enumerate(delay_analysis['bin_centers']):
        if delay_analysis['trial_counts'][i] > 0:
            print(f"   Delay {center:.1f} μs: {delay_analysis['trial_counts'][i]} trials, "
                  f"Mean error: {delay_analysis['mean_errors'][i]:.3f} ± {delay_analysis['std_errors'][i]:.3f} μs, "
                  f"Accuracy: {delay_analysis['mean_accuracies'][i]:.1f}%")


def quick_test(n_trials=20, debug_threshold=70.0):
    """Run a quicker test with fewer trials for initial evaluation."""
    print(f"🚀 Running quick test with {n_trials} trials...")
    return comprehensive_delay_test(n_trials=n_trials, max_delay_us=145.0, delay_bins=5, debug_threshold=debug_threshold)


if __name__ == "__main__":
    # Choose which test to run
    print("=== Comprehensive Delay Estimation Test ===\n")
    
    # Uncomment one of these options:
    
    # Option 1: Full comprehensive test (100 trials) with debug mode
    all_results, delay_analysis = comprehensive_delay_test(
        n_trials=100, 
        max_delay_us=145.0, 
        delay_bins=10,
        debug_threshold=70.0  # Plot detailed info for cases with <70% accuracy
    )
    
    # Option 2: Quick test (20 trials) - uncomment to use instead
    # all_results, delay_analysis = quick_test(n_trials=20, debug_threshold=70.0)
    
    # Option 3: Custom parameters
    # all_results, delay_analysis = comprehensive_delay_test(
    #     n_trials=50,         # Number of trials
    #     max_delay_us=100,    # Max delay to test
    #     delay_bins=8,        # Number of analysis bins
    #     debug_threshold=80.0 # Debug threshold (set to None to disable)
    # )
    
    print("\n🎉 Test complete! Check the plots and statistics above.")