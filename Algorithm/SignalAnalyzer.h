#pragma once
#include <math.h>
#include <Arduino.h>

#define N_PEAKS 20
#define HANDLE_N_SAMPLES 20
#define ENERGY_WINDOW 8
#define COARSE_STEP 32
#define K 10
#define _EPS 1e-7f // slope tolerance
#define MIN_I_DIFF 4 // min distance between peaks
#define MAX_I_DIFF 6 // max distance between peaks

class SignalAnalyzer {
public:
    SignalAnalyzer(float* sample_buffer) : samples(sample_buffer) {}

    bool analyze(size_t n_samples, size_t& signal_start, size_t* peaks);
    void handle(size_t n_samples, float* noise_samples);

    float signal_threshold = 0.001f;

private:
    float sum_square_window(size_t n_samples, size_t index);
    bool  detect_start(size_t n_samples, size_t& signal_start);
    size_t detect_peaks(size_t n_samples, size_t start_index, size_t* peaks);

    float* samples;
};
