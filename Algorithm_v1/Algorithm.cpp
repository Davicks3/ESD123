#include "Algorithm.h"


bool Algorithm::calculate(float& angle, float& distance)
{
    if (!get_triggered_state()) { return false; }
    
    uint16_t sig_offset = 0; // Should be 0.
    
    uint64_t frames_to_discard = triggerIndex - readIndex;
        
    if (readIndex > triggerIndex)
    {
        frames_to_discard = 0;
        sig_offset = readIndex - triggerIndex;
    }
    else
    {
        discard_frames((size_t)frames_to_discard - FRAMES_PER_READ); // Discard first part.
        
        float noise_l[FRAMES_PER_READ], noise_r[FRAMES_PER_READ];
        
        size_t n_samples = read_samples(noise_l, noise_r); // Analyze the last part of the noisefloor.
        analyzer_l.handle(NOISEFLOOR_N_SAMPLES, noise_l + (n_samples - NOISEFLOOR_N_SAMPLES));
        analyzer_r.handle(NOISEFLOOR_N_SAMPLES, noise_r + (n_samples - NOISEFLOOR_N_SAMPLES));
    }
    
    size_t frames_read = fetch(sig_left, sig_right, &sig_offset);
    
    if (!frames_read) { return false; }
    
    /*
    memcpy(sig_left, left_test_data, sizeof(left_test_data));
    memcpy(sig_right, right_test_data, sizeof(right_test_data));
    size_t frames_read = TEST_DATA_N;
    */

    Serial.println("###################################");
    
    Serial.print("Signal offset: "); Serial.print(sig_offset); Serial.println(" samples");
    float t_diff, sig_delay; // us
    if (!solve(frames_read, t_diff, sig_delay)) { return false; }

    angle = calc_angle(t_diff);
    distance = calc_distance(sig_delay, sig_offset);
    return true;

}

bool Algorithm::solve(size_t n_frames, float& t_diff, float& sig_delay)
{
    unsigned long t_a = micros();
    size_t signal_start_l, est_peaks_l[N_PEAKS];
    if (!analyzer_l.analyze(n_frames, signal_start_l, est_peaks_l)) { Serial.println("Failed: analyzer left"); return false; }

    size_t signal_start_r, est_peaks_r[N_PEAKS];
    if (!analyzer_r.analyze(n_frames, signal_start_r, est_peaks_r)) { Serial.println("Failed: analyzer right"); return false; }
    t_a = micros() - t_a;

    unsigned long t_n = micros();
    normalize(n_frames, (size_t)N_PEAKS, est_peaks_l, sig_left);
    normalize(n_frames, (size_t)N_PEAKS, est_peaks_r, sig_right);
    t_n = micros() - t_n;

    unsigned long t_i = micros();
    float peaks_l[N_PEAKS], time_l[N_PEAKS];
    if (!peak_interpolator_l.interpolate_peaks_parabolic(n_frames, N_PEAKS, est_peaks_l, peaks_l, time_l)) { Serial.println("Failed: interpolator left"); return false; }
    
    float peaks_r[N_PEAKS], time_r[N_PEAKS];
    if (!peak_interpolator_r.interpolate_peaks_parabolic(n_frames, N_PEAKS, est_peaks_r, peaks_r, time_r)) { Serial.println("Failed: interpolator right"); return false; }
    t_i = micros() - t_i;
    
    unsigned long t_c = micros();
    find_peak_diff(peaks_l, time_l, peaks_r, time_r, t_diff); // Correlate peaks, to find signal diff.
    t_c = micros() - t_c;

    unsigned long t_d = micros();
    find_sig_delay(peaks_l, time_l, peaks_r, time_r, N_PEAKS, sig_delay);
    t_d = micros() - t_d;

    float dist = sig_delay * 0.0343f;
    unsigned long total_t = t_a + t_n + t_i + t_c + t_d;

    //Serial.print("Signal start left: "); Serial.println(signal_start_l);
    //Serial.print("Signal start right: "); Serial.println(signal_start_r)
    
    //Serial.print("time diff: "); Serial.print(t_diff, 6); Serial.println(" us");
    Serial.print("angle: "); Serial.print(calc_angle(t_diff), 4); Serial.println(" degrees");
    //Serial.print("signal delay: "); Serial.print(sig_delay, 6); Serial.println(" us");
    Serial.print("distance: "); Serial.print(dist, 6); Serial.println(" cm");

    //Serial.print("Analyzation took "); Serial.print(t_a); Serial.println(" us");
    //Serial.print("Normalization took "); Serial.print(t_n); Serial.println(" us");
    //Serial.print("Interpolation took "); Serial.print(t_i); Serial.println(" us");
    //Serial.print("Correlation took "); Serial.print(t_c); Serial.println(" us");
    //Serial.print("Line fit took "); Serial.print(t_d); Serial.println(" us");
    //Serial.print("Total time was "); Serial.print(total_t); Serial.println(" us");
    Serial.flush();
    
    return true;
}


static inline float corr_norm(const float* a, const float* b, int n)
{
    float num = 0.0f, da = 0.0f, db = 0.0f;
    for (int i = 0; i < n; ++i) {
        float x = a[i];
        float y = b[i];
        num += x * y;
        da  += x * x;
        db  += y * y;
    }
    float denom = sqrtf(da * db);
    if (denom < 1e-12f) return -1.0f;
    return num / denom;
}

void Algorithm::find_peak_diff(float* peaks_l, float* time_l,
                               float* peaks_r, float* time_r,
                               float& t_diff)
{
    const int N = (int)N_PEAKS;

    // Search lag in peak-index domain
    int L = 10;
    if (L > N - 1) L = N - 1;

    // Fixed overlap length for all lags
    const int W = N - L;               // same #pairs for each lag
    const float TIE_EPS = 1e-4f;

    int bestLag = 0;
    float bestCorr = -2.0f;

    for (int lag = -L; lag <= L; ++lag)
    {
        // align: L[i] with R[i+lag]
        int i0 = (lag < 0) ? -lag : 0;
        if (i0 + W > N) continue;

        float c = corr_norm(peaks_l + i0, peaks_r + i0 + lag, W);

        if (c > bestCorr + TIE_EPS ||
            (fabsf(c - bestCorr) <= TIE_EPS && abs(lag) < abs(bestLag)))
        {
            bestCorr = c;
            bestLag = lag;
        }
    }

    // Now compute delay using median of time differences after shifting by bestLag
    float dt[N_PEAKS];
    int n_dt = 0;

    if (bestLag >= 0) {
        for (int i = 0; i + bestLag < N; ++i) {
            dt[n_dt++] = time_r[i + bestLag] - time_l[i];  // R - L
        }
    } else {
        int k = -bestLag;
        for (int i = 0; i + k < N; ++i) {
            dt[n_dt++] = time_r[i] - time_l[i + k];        // R - L
        }
    }

    // sort (small N)
    for (int i = 0; i < n_dt - 1; ++i) {
        int mi = i;
        for (int j = i + 1; j < n_dt; ++j) if (dt[j] < dt[mi]) mi = j;
        if (mi != i) { float tmp = dt[i]; dt[i] = dt[mi]; dt[mi] = tmp; }
    }

    if (n_dt <= 0) { t_diff = 0.0f; return; }

    // median
    if (n_dt & 1) t_diff = dt[n_dt / 2];
    else          t_diff = 0.5f * (dt[n_dt/2 - 1] + dt[n_dt/2]);
}



/*
void Algorithm::find_peak_diff(float* peaks_l, float* time_l, float* peaks_r, float* time_r, float& t_diff)
{
    const int M = (int)N_PEAKS - 1;

    float dL[N_PEAKS - 1];
    float dR[N_PEAKS - 1];

    const float DT_EPS = 1e-9f;

    for (int i = 0; i < M; ++i) {
        float dtL = time_l[i + 1] - time_l[i];
        float dtR = time_r[i + 1] - time_r[i];

        if (fabsf(dtL) < DT_EPS) dtL = (dtL < 0.0f ? -DT_EPS : DT_EPS);
        if (fabsf(dtR) < DT_EPS) dtR = (dtR < 0.0f ? -DT_EPS : DT_EPS);

        dL[i] = (peaks_l[i + 1] - peaks_l[i]) / dtL;
        dR[i] = (peaks_r[i + 1] - peaks_r[i]) / dtR;
    }

    // Optional, but fine to keep if you already have it.
    // If normalize_der does abs-max only, the per-lag normalization below still protects you.
    normalize_der(M, dL);
    normalize_der(M, dR);

    int bestLag = 0;
    float bestCorr = -1.0f;

    int L = 10;
    if (L > M - 1) L = M - 1;

    const int MIN_OVERLAP = 8;

    for (int lag = -L; lag <= L; ++lag) {
        float num = 0.0f;
        float denL = 0.0f;
        float denR = 0.0f;
        int count = 0;

        for (int i = 0; i < M; ++i) {
            int j = i + lag;
            if ((unsigned)j >= (unsigned)M) continue;

            float a = dL[i];
            float b = dR[j];
            num  += a * b;
            denL += a * a;
            denR += b * b;
            ++count;
        }

        if (count < MIN_OVERLAP) continue;

        float denom = sqrtf(denL * denR);
        if (denom < 1e-12f) continue;

        float corr = num / denom;
        if (corr > bestCorr) {
            bestCorr = corr;
            bestLag = lag;
        }
    }

    // Build dt array using the SAME alignment as correlation:
    // correlation aligns dL[i] with dR[i+lag]
    // so times align time_l[i] with time_r[i+lag]
    float dt[N_PEAKS];
    int n_dt = 0;

    if (bestLag >= 0) {
        for (int i = 0; i + bestLag < (int)N_PEAKS; ++i) {
            dt[n_dt++] = time_r[i + bestLag] - time_l[i];   // R - L
        }
    } else {
        int k = -bestLag;
        for (int i = 0; i + k < (int)N_PEAKS; ++i) {
            dt[n_dt++] = time_r[i] - time_l[i + k];         // R - L
        }
    }

    if (n_dt <= 0) {
        t_diff = 0.0f;
        return;
    }

    // sort dt (small N -> simple sort is fine)
    for (int i = 0; i < n_dt - 1; ++i) {
        int min_i = i;
        for (int j = i + 1; j < n_dt; ++j) {
            if (dt[j] < dt[min_i]) min_i = j;
        }
        if (min_i != i) {
            float tmp = dt[i];
            dt[i] = dt[min_i];
            dt[min_i] = tmp;
        }
    }

    // median
    if (n_dt & 1) {
        t_diff = dt[n_dt / 2];
    } else {
        t_diff = 0.5f * (dt[n_dt / 2 - 1] + dt[n_dt / 2]);
    }
}
*/


void Algorithm::normalize_der(size_t n_der, float* der)
{
    if (n_der < 2) return;

    float mean = 0.0f;
    for (size_t i = 0; i < n_der; ++i) mean += der[i];
    mean /= (float)n_der;

    float var = 0.0f;
    for (size_t i = 0; i < n_der; ++i) {
        float x = der[i] - mean;
        var += x * x;
    }
    var /= (float)(n_der - 1);

    float std = sqrtf(var);
    if (std < 1e-12f) {
        for (size_t i = 0; i < n_der; ++i) der[i] = 0.0f;
        return;
    }

    float inv = 1.0f / std;
    for (size_t i = 0; i < n_der; ++i) der[i] = (der[i] - mean) * inv;
}

void Algorithm::find_sig_delay(float* peaks_l, float* time_l, float* peaks_r, float* time_r, size_t n_peaks, float& sig_delay)
{
    float a, b;
    fit_line(time_l, peaks_l, (int)n_peaks, a, b);
    float start_l = calc_intercept(a, b);
    
    fit_line(time_r, peaks_r, (int)n_peaks, a, b);
    float start_r = calc_intercept(a, b);

    //sig_delay = (start_l + start_r) / 2.0f; // mean dist
    sig_delay = fminf(start_l, start_r); // shortest dist
}

bool Algorithm::fit_line(float* t, float* peaks, int n_peaks, float& a, float& b)
{
    if (n_peaks < 2) { return false; }

    float T  = 0.0f; // sum t
    float Y  = 0.0f; // sum y
    float TT = 0.0f; // sum t^2
    float TY = 0.0f; // sum t*y

    for (int i = 0; i < n_peaks; ++i) {
        float ti = t[i];
        float yi = peaks[i];
        T  += ti;
        Y  += yi;
        TT += ti * ti;
        TY += ti * yi;
    }

    float Nf = (float)n_peaks;
    float D = Nf * TT - T * T;

    if (fabsf(D) < 1e-9f) { return false; } // vertical, no good.

    a = (Nf * TY - T * Y) / D;
    b = (Y - a * T) / Nf;
    return true;
}

float Algorithm::calc_intercept(float a, float b)
{
    return -b / a;
}

float Algorithm::calc_angle(float t_diff)
{
    float theta = t_diff * ANGLE_K;
    theta = fminf(1.0f, fmaxf(-1.0f, theta));
    return asinf(theta) * RAD_TO_DEG;
}

float Algorithm::calc_distance(float sig_delay, uint16_t sig_offset)
{
    float sig_delay_offset = (float)sig_offset * 1000.0 / 192.0; // us
    return (sig_delay + sig_delay_offset) * 0.0343f; // cm
}

void Algorithm::handle()
{
    unsigned long now = millis();
    float noise_l[FRAMES_PER_READ], noise_r[FRAMES_PER_READ];

    if (now - last_resync_millis >= RESYNC_READINDEX_MS) { sync_indicies(); }
    
    uint64_t write_index = frameCounter.get();

    if (write_index < readIndex + SAFE_FRAME_READ_DIFF) { return; } // Underflow safety.

    uint64_t read_lag = write_index - (readIndex + SAFE_FRAME_READ_DIFF);

    // Buffer has been filled, we need to empty it.
    if (write_index - readIndex >= DMA_BUF_LEN * DMA_BUF_COUNT)
    {
        discard_frames(DMA_BUF_LEN * DMA_BUF_COUNT - FRAMES_PER_READ); // Discard most of the buffer.
        
        size_t n_samples = read_samples(noise_l, noise_r); // Analyze the last part of the noisefloor.
        if (n_samples >= NOISEFLOOR_N_SAMPLES)
        {
            analyzer_l.handle(NOISEFLOOR_N_SAMPLES, noise_l + (n_samples - NOISEFLOOR_N_SAMPLES));
            analyzer_r.handle(NOISEFLOOR_N_SAMPLES, noise_r + (n_samples - NOISEFLOOR_N_SAMPLES));
        }
        sync_indicies();
        return;
    }

    // Keep up with the write pointer.
    if (read_lag >= FLUSH_DMA_BUFFER_THRESHOLD + NOISEFLOOR_N_SAMPLES)
    {
        discard_frames((size_t)read_lag - FRAMES_PER_READ); // Discard first part.

        size_t n_samples = read_samples(noise_l, noise_r); // Analyze the last part of the noisefloor.
        analyzer_l.handle(NOISEFLOOR_N_SAMPLES, noise_l + (n_samples - NOISEFLOOR_N_SAMPLES));
        analyzer_r.handle(NOISEFLOOR_N_SAMPLES, noise_r + (n_samples - NOISEFLOOR_N_SAMPLES));
    }

}

void Algorithm::normalize(size_t n_frames, size_t n_peaks, size_t* est_peaks, float* channel)
{
    
    int start_index = est_peaks[0] - INTERPOLATION_NEIGHBOURS - 2;
    if (start_index < 0) { start_index = 0; }

    int end_index = est_peaks[n_peaks-1] + INTERPOLATION_NEIGHBOURS + 2;
    if (end_index > (int)n_frames) { end_index = (int)n_frames; }
    

    // Compute mean (DC offset)
    float sum = 0.0f;
    for (size_t i = (size_t)start_index; i < (size_t)end_index; i++) {
        sum += channel[i];
    }
    float mean = sum / (float)n_frames;
    //Serial.print("DC offset: "); Serial.print(mean, 4); Serial.println(" V");

    // Remove DC + find abs max
    float abs_max = 0.0f;
    for (size_t i = (size_t)start_index; i < (size_t)end_index; i++) {
        channel[i] -= mean;
        float a = fabsf(channel[i]);
        if (a > abs_max) { abs_max = a; }
    }
    //Serial.print("Abs max: "); Serial.print(abs_max, 4); Serial.println(" V");

    // Normalize
    if (abs_max > 1e-12f) {
        float inv = 1.0f / abs_max;
        for (size_t i = 0; i < n_frames; i++) {
            channel[i] *= inv;
        }
    }
}
