#include "SignalAnalyzer.h"


bool SignalAnalyzer::analyze(size_t n_samples, size_t& signal_start, size_t* peaks)
{
    if (!detect_start(n_samples, signal_start)) { Serial.println("Start not found!"); return false; }

    size_t n_found = detect_peaks(n_samples, signal_start, peaks);

    if (n_found < N_PEAKS) { Serial.println("Not all peaks found!"); return false; }

    return true;
}

void SignalAnalyzer::handle(size_t n_samples, float* noise_samples)
{
    // Update threshold, based on mean and standard deviation.
    if (n_samples < HANDLE_N_SAMPLES) {
        return;
    }
    
    size_t i_start = n_samples - HANDLE_N_SAMPLES;

    float sum  = 0.0f;  // sum of squared samples
    float sum2 = 0.0f;  // sum of (squared samples)^2

    for (size_t i = 0; i < HANDLE_N_SAMPLES; i++) {
        float v = noise_samples[i_start + i];
        float vv = v * v;
        sum += vv;
        sum2 += vv * vv;
    }

    float mean = sum / (float)HANDLE_N_SAMPLES;

    float mean2 = sum2 / (float)HANDLE_N_SAMPLES;
    float var = mean2 - (mean * mean);

    if (var < 0.0f) var = 0.0f;

    float stddev = sqrtf(var);
    float mu_sum = mean * ENERGY_WINDOW;
    float sigma_sum = stddev * sqrtf(ENERGY_WINDOW);
    float new_thres = mu_sum + K * sigma_sum;
    if (new_thres < 1e-3) { new_thres = 1e-3; }
    //if (new_thres > 1e-2) { new_thres = 1e-2; }
    signal_threshold = new_thres;
}

float SignalAnalyzer::sum_square_window(size_t n_samples, size_t index)
{
    float s = 0.0f;
    size_t end = index + ENERGY_WINDOW;
    if (end > n_samples) { end = n_samples; }
    for (size_t k = index; k < end; ++k) {
        float v = samples[k];
        s += v * v;
    }
    return s;
}

bool SignalAnalyzer::detect_start(size_t n_samples, size_t& signal_start)
{
    size_t coarseHit;
    bool found = false;

    for (size_t p = 0; p + ENERGY_WINDOW <= n_samples; p += COARSE_STEP) {
        float E = sum_square_window(n_samples, p);
        if (E >= signal_threshold) {
            coarseHit = p;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    // Fine binary search
    size_t refineStart = coarseHit;
    if (refineStart >= COARSE_STEP) { refineStart -= COARSE_STEP; }

    for (size_t i = refineStart; i <= coarseHit; i++) {
        if (i + ENERGY_WINDOW > n_samples) { break; }
        float E = sum_square_window(n_samples, i);
        if (E >= signal_threshold) {
            signal_start = i;
            return true;
        }
    }

    return false;
}

size_t SignalAnalyzer::detect_peaks(size_t n_samples, size_t start_index, size_t* peaks)
{
    size_t peak_i = 0;
    int last_peak = -1; // index of last accepted peak
    bool first = true;

    int state = 0;
    int count = 0;

    for (size_t i = start_index + 1; i < n_samples; ++i) {
        float diff = samples[i] - samples[i - 1];

        switch (state) {
            case 0: // ready: looking for two consecutive up trends
                if (diff > _EPS) {
                    count++;
                } else {
                    count = 0; // reset count
                }
                if (count >= 2) {
                    count = 0;
                    state = 1; // seen two rises
                }
                break;

            case 1: // seen two up trends; look for equal or first fall
                if (diff > _EPS) {
                    // still rising; stay here
                } else if (diff <= _EPS && diff >= -_EPS) {
                    // equal -> plateau beginning, go to state 2 (looking for falls)
                    count = 0;
                    state = 2;
                } else if (diff < -_EPS) {
                    // immediate start of falling -> count first fall
                    count = 1;
                    state = 2;
                }
                break;

            case 2: // looking for two consecutive DOWN trends
                if (diff < -_EPS) {
                    count++;
                } else {
                    // broke the falling pattern - reset
                    count = 0;
                    state = 0;
                }

                if (count >= 2) {
                    // We have at least two consecutive falls.
                    // Peak is just before the falling run.
                    int new_peak_i = (int)i - count;

                    if (!first) {
                        int i_diff = new_peak_i - last_peak;
                        if (i_diff < MIN_I_DIFF || i_diff > MAX_I_DIFF) {
                            // distance is wrong - stop searching
                            return peak_i;
                        }
                    }

                    // Accept new peak
                    peaks[peak_i] = new_peak_i;
                    last_peak = (size_t)new_peak_i;
                    first = false;
                    peak_i++;

                    if (peak_i >= N_PEAKS) {
                        return peak_i;
                    }

                    // reset for next peak
                    count = 0;
                    state = 0;
                }
                break;

            default:
                count = 0;
                state = 0;
                break;
        }
    }

    return peak_i;
}
