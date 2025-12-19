#!/usr/bin/env python3
"""
Demonstration script for the object-oriented signal processor.

This script shows how to use the SignalProcessor class to:
1. Generate two similar signals with random delay
2. Add noise to both signals
3. Apply bandpass filtering
4. Perform sinc interpolation
5. Estimate delay using cross-correlation
6. Compare with true delay
"""

from signal_processor import SignalProcessor
import numpy as np
import matplotlib.pyplot as plt


def main():
    """Main demonstration function."""
    print("=== Object-Oriented Signal Processing Demo ===\n")
    
    # Initialize signal processor
    print("1. Initializing Signal Processor...")
    processor = SignalProcessor(
        fs=192_000,           # Sampling frequency (Hz)
        f0=40_000,           # Signal frequency (Hz) 
        cycles=10,           # Number of cycles
        noise_level=0.2,     # Noise level
        random_seed=42       # For reproducible results
    )
    print(f"   Sampling Rate: {processor.fs/1000:.0f} kHz")
    print(f"   Signal Frequency: {processor.f0/1000:.0f} kHz") 
    print(f"   Signal Duration: {processor.duration*1000:.2f} ms")
    print(f"   Number of Samples: {processor.N}")
    
    # Run complete analysis
    print("\n2. Running Complete Analysis...")
    results = processor.run_complete_analysis(
        max_delay_us=145.0,    # Maximum delay in microseconds
        up_factor=8,           # Interpolation factor
        plot_results=True,     # Show plots
        verbose=True           # Print detailed results
    )
    
    # Demonstrate multiple runs with different delays
    print("\n3. Running Multiple Trials...")
    run_multiple_trials(n_trials=10)
    
    print("\n=== Demo Complete ===")


def run_multiple_trials(n_trials: int = 10):
    """
    Run multiple trials to test estimation accuracy across different delays.
    
    Args:
        n_trials: Number of trials to run
    """
    print(f"   Running {n_trials} trials with different random delays...")
    
    true_delays = []
    estimated_delays = []
    errors = []
    accuracies = []
    
    for i in range(n_trials):
        # Create new processor for each trial (different random seed)
        processor = SignalProcessor(random_seed=i)
        
        # Run analysis without plotting
        results = processor.run_complete_analysis(
            max_delay_us=145.0,
            up_factor=8,
            plot_results=False,
            verbose=False
        )
        
        true_delays.append(results['true_delay_us'])
        estimated_delays.append(results['estimated_delay_us'])
        errors.append(results['delay_error_us'])
        accuracies.append(results['estimation_accuracy'])
    
    # Convert to numpy arrays for easier analysis
    true_delays = np.array(true_delays)
    estimated_delays = np.array(estimated_delays)
    errors = np.array(errors)
    accuracies = np.array(accuracies)
    
    # Print statistics
    print(f"\n   Results from {n_trials} trials:")
    print(f"   Mean Error: {np.mean(errors):.3f} ± {np.std(errors):.3f} μs")
    print(f"   Max Error: {np.max(errors):.3f} μs")
    print(f"   Min Error: {np.min(errors):.3f} μs")
    print(f"   Mean Accuracy: {np.mean(accuracies):.1f} ± {np.std(accuracies):.1f}%")
    
    # Plot results
    plt.figure(figsize=(15, 5))
    
    plt.subplot(1, 3, 1)
    plt.scatter(true_delays, estimated_delays, alpha=0.7)
    plt.plot([0, 145], [0, 145], 'r--', label='Perfect Estimation')
    plt.xlabel('True Delay (μs)')
    plt.ylabel('Estimated Delay (μs)')
    plt.title('Delay Estimation Accuracy')
    plt.legend()
    plt.grid(True)
    
    plt.subplot(1, 3, 2)
    plt.hist(errors, bins=10, alpha=0.7, edgecolor='black')
    plt.xlabel('Estimation Error (μs)')
    plt.ylabel('Frequency')
    plt.title(f'Error Distribution\n(Mean: {np.mean(errors):.3f} μs)')
    plt.grid(True)
    
    plt.subplot(1, 3, 3)
    plt.hist(accuracies, bins=10, alpha=0.7, edgecolor='black')
    plt.xlabel('Accuracy (%)')
    plt.ylabel('Frequency')
    plt.title(f'Accuracy Distribution\n(Mean: {np.mean(accuracies):.1f}%)')
    plt.grid(True)
    
    plt.tight_layout()
    plt.show()


def demonstrate_noise_effects():
    """Demonstrate the effect of different noise levels on estimation accuracy."""
    print("\n4. Demonstrating Noise Effects...")
    
    noise_levels = [0.0, 0.1, 0.2, 0.3, 0.5, 0.7, 1.0]
    mean_errors = []
    std_errors = []
    
    for noise_level in noise_levels:
        print(f"   Testing noise level: {noise_level}")
        
        errors = []
        for trial in range(5):  # 5 trials per noise level
            processor = SignalProcessor(noise_level=noise_level, random_seed=trial)
            results = processor.run_complete_analysis(
                max_delay_us=100.0,
                up_factor=8,
                plot_results=False,
                verbose=False
            )
            errors.append(results['delay_error_us'])
        
        mean_errors.append(np.mean(errors))
        std_errors.append(np.std(errors))
    
    # Plot noise effect
    plt.figure(figsize=(10, 6))
    plt.errorbar(noise_levels, mean_errors, yerr=std_errors, 
                 marker='o', capsize=5, capthick=2)
    plt.xlabel('Noise Level (relative to signal)')
    plt.ylabel('Mean Estimation Error (μs)')
    plt.title('Effect of Noise on Delay Estimation Accuracy')
    plt.grid(True)
    plt.show()
    
    print(f"   Noise analysis complete. Best accuracy at noise level: {noise_levels[np.argmin(mean_errors)]}")


if __name__ == "__main__":
    main()
    
    # Uncomment to run additional demonstrations
    # demonstrate_noise_effects()