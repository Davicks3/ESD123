#include "math.h"


#define INTERPOLATION_NEIGHBOURS 5
#define _PI 3.14159265358979323846f
#define _TWO_PI 2.0f * _PI
#define _HALF_PI (_PI * 0.5f)

#ifndef SAMPLE_T_US
#define SAMPLE_T_US 1000.0f / 192.0f
#endif

class PeakInterpolator {
    public:
    PeakInterpolator(float* sample_buffer) : samples(sample_buffer) {}
    bool interpolate_peaks(size_t n_samples, size_t n_peaks, const size_t* est_peaks, float* peaks, float* time);
    bool interpolate_peaks_parabolic(size_t n_samples, size_t n_peaks, const size_t* est_peaks, float* peaks, float* time);

    private:
    void normalize(size_t n_samples, size_t start_index, size_t end_index);
    bool interpolate_peak(size_t n_samples, size_t index, float& t, float& val);
    bool interpolate_peak_parabolic(size_t n_samples, size_t index, float& t, float& val);
    float windowed_sinc_pi(size_t k, float delta);
    float windowed_sinc_pi_der(size_t k, float delta);
    float fast_sinc_pi_der(float u);
    float fast_sinc_pi(float u);
    float fast_sin(float x);
    float fast_cos(float x);

    float* samples;
};
