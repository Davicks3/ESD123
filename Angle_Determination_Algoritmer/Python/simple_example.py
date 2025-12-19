#!/usr/bin/env python3
"""
Simple usage example of the SignalProcessor class.
"""

from signal_processor import SignalProcessor

def simple_example():
    """Simple example showing basic usage."""
    
    # Create processor
    processor = SignalProcessor(
        fs=192_000,      # 192 kHz sampling
        f0=40_000,       # 40 kHz signal
        cycles=10,       # 10 cycles
        noise_level=0.2, # 20% noise
        random_seed=None  # Reproducible results
    )
    
    # Run complete analysis
    results = processor.run_complete_analysis(
        max_delay_us=145.0,  # Random delay 0-145 μs
        up_factor=8,         # 8x interpolation
        plot_results=True,   # Show plots
        verbose=True         # Print results
    )
    
    return results

if __name__ == "__main__":
    results = simple_example()
    print(f"\nFinal Results: {results['estimation_accuracy']:.1f}% accurate")