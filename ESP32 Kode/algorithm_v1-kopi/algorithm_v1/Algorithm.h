#include "Sampler.h"
#include "Bandpass.h"
#include "SignalAnalyzer.h"
#include "PeakInterpolator.h"
#include "test_data.h"

#define NOISEFLOOR_N_SAMPLES 30
#define SOUND_SPEED 343.0f
#define SENSOR_DISTANCE_M 0.1f
#define ANGLE_K (SOUND_SPEED / SENSOR_DISTANCE_M) * 1e-6f
#define RAD_TO_DEG 57.29577951308232f


class Algorithm : public Sampler {
    public:
    //using Sampler::Sampler;
    Algorithm(const int bclkPin, const int lrclkPin, const int dataInPin, const int sync_pulse_pin)
    : Sampler(bclkPin, lrclkPin, dataInPin, sync_pulse_pin),
    analyzer_l(sig_left),
    analyzer_r(sig_right),
    peak_interpolator_l(sig_left),
    peak_interpolator_r(sig_right) {}

    bool calculate(float& angle, float& distance);
    void handle();
    private:
    bool solve(size_t n_frames, float& t_diff, float& sig_delay);
    bool estimate_peaks(size_t n_frames, size_t* est_peaks, size_t peak_idx);
    bool find_peaks(size_t n_frames, size_t* est_peaks, float* peaks);
    void find_peak_diff(float* peaks_l, float* time_l, float* peaks_r, float* time_r, float& t_diff);
    void find_sig_delay(float* peaks_l, float* time_l, float* peaks_r, float* time_r, size_t n_peaks, float& sig_delay);
    float calc_angle(float t_diff);
    float calc_distance(float sig_delay, uint16_t sig_offset);
    void normalize(size_t n_frames, size_t n_peaks, size_t* est_peaks, float* channel);
    void normalize_der(size_t n_der, float* der);
    bool fit_line(float* t, float* peaks, int n_peaks, float& a, float& b);
    float calc_intercept(float a, float b);

    float sig_left[FRAMES_PER_SIGNAL];
    float sig_right[FRAMES_PER_SIGNAL];

    Bandpass bandpass;
    SignalAnalyzer analyzer_l;
    SignalAnalyzer analyzer_r;
    PeakInterpolator peak_interpolator_l;
    PeakInterpolator peak_interpolator_r;

};
