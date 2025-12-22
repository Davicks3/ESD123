#include "PeakInterpolator.h"


bool PeakInterpolator::interpolate_peaks(size_t n_samples, size_t n_peaks, const size_t* est_peaks, float* peaks, float* time)
{
    //normalize(n_samples, est_peaks[0], est_peaks[n_peaks - 1]);
    for (size_t j = 0; j < n_peaks; j++)
    {
        if (!interpolate_peak(n_samples, est_peaks[j], time[j], peaks[j])) { return false; }
    }
    return true;
}

void PeakInterpolator::normalize(size_t n_samples, size_t start_index, size_t end_index)
{
    int i_start = (int)start_index - INTERPOLATION_NEIGHBOURS;
    if (i_start < 0) { i_start = 0; }

    int i_end = (int)end_index + INTERPOLATION_NEIGHBOURS;
    if (i_end > (int)n_samples) { i_end = (int)n_samples; }

    float max = fabsf(samples[i_start]);
    for (int i = i_start; i < i_end; i++)
    {
        float val = fabsf(samples[i]);
        if (val > max) { max = val; }
    }
    if (max < 1e-12f) { return; }

    // Very simple normalization. Improvements: remove DC offset, min/max scaler.
    for (int i = i_start; i < i_end; i++)
    {
        samples[i] = samples[i] / max;
    }
}

bool PeakInterpolator::interpolate_peak(size_t n_samples, size_t index, float& t, float& val)
{
    if (index < INTERPOLATION_NEIGHBOURS || index + INTERPOLATION_NEIGHBOURS >= n_samples) {
        return false;
    }

    float delta0 = -0.01f;
    float delta1 = 0.01f; // Smaller maybe?

    float g0 = windowed_sinc_pi_der(index, delta0);
    float g1 = windowed_sinc_pi_der(index, delta1);

    for (int iter = 0; iter < 10; ++iter) {
        float denom = (g1 - g0);
        if (fabsf(denom) < 1e-6f || fabsf(g1) < 1e-6f) { break; }

        float delta2 = delta1 - g1 * (delta1 - delta0) / denom;

        if (delta2 > 0.5f) { delta2 =  0.5f; }
        if (delta2 < -0.5f) { delta2 = -0.5f; }

        delta0 = delta1;
        g0 = g1;
        delta1 = delta2;
        g1 = windowed_sinc_pi_der(index, delta1);
    }

    float delta = delta1;
    val = windowed_sinc_pi(index, delta);
    t = (float)index + delta; // should be time, in us. use SAMPLE_T macro.
    t *= SAMPLE_T_US;
    return true;
}

float PeakInterpolator::windowed_sinc_pi(size_t k, float delta)
{
    float sum = 0.0f;
    for (int m = -INTERPOLATION_NEIGHBOURS; m <= INTERPOLATION_NEIGHBOURS; ++m)
    {
        float xm = samples[k + m];
        float u = delta - (float)m;
        sum += xm * fast_sinc_pi(u);
    }
    return sum;
}

float PeakInterpolator::windowed_sinc_pi_der(size_t k, float delta)
{
    float sum = 0.0f;
    for (int m = -INTERPOLATION_NEIGHBOURS; m <= INTERPOLATION_NEIGHBOURS; ++m)
    {
        float xm = samples[k + m];
        float u  = delta - (float)m;
        sum += xm * fast_sinc_pi_der(u);
    }
    return sum;
}

float PeakInterpolator::fast_sinc_pi_der(float u)
{
    if (fabsf(u) < 1e-6f) return 0.0f;

    float theta = _PI * u;
    float s = fast_sin(theta);
    float c = fast_cos(theta);

    return (theta * c - s) / (_PI * u * u);
}

float PeakInterpolator::fast_sinc_pi(float u)
{
    if (fabsf(u) < 1e-6f) { return 1.0f; }
    float theta = _PI * u;
    float s = fast_sin(theta);
    return s / theta;
}

float PeakInterpolator::fast_sin(float theta)
{
    return sinf(theta);
}

float PeakInterpolator::fast_cos(float theta)
{
    return cosf(theta);
}

bool PeakInterpolator::interpolate_peaks_parabolic(size_t n_samples,
                                                   size_t n_peaks,
                                                   const size_t* est_peaks,
                                                   float* peaks,
                                                   float* time)
{
    normalize(n_samples, est_peaks[0], est_peaks[n_peaks - 1]);

    for (size_t j = 0; j < n_peaks; ++j) {
        if (!interpolate_peak_parabolic(n_samples, est_peaks[j], time[j], peaks[j])) {
            return false;
        }
    }
    return true;
}

bool PeakInterpolator::interpolate_peak_parabolic(size_t n_samples, size_t index, float& t, float& val)
{
    if (index == 0 || index + 1 >= n_samples) { return false; }

    size_t k = index;

    float ym1 = samples[k - 1];
    float y0  = samples[k];
    float yp1 = samples[k + 1];

    // quadratic through x=-1,0,+1:
    // a = 0.5*(ym1 + yp1) - y0
    // b = 0.5*(yp1 - ym1)
    // c = y0
    float a = 0.5f * (ym1 + yp1) - y0;
    float b = 0.5f * (yp1 - ym1);
    float c = y0;

    // vertex x-coordinate (offset from k): delta = -b / (2a)
    float denom = 2.0f * a;
    float delta;

    if (fabsf(denom) < 1e-9f) {
        delta = 0.0f;
    } else {
        delta = -b / denom;
        // clamp: we don't trust crazy moves
        if (delta >  0.5f) delta =  0.5f;
        if (delta < -0.5f) delta = -0.5f;
    }

    // parabola value at x = delta
    float y_peak = (a * delta + b) * delta + c;

    val = y_peak;
    t   = ((float)k + delta) * SAMPLE_T_US;

    return true;
}
