#include "upsampler.h"

Upsampler::Upsampler(long sample_f, int interpolation_support) : sample_f(sample_f), i_interpolation_support(interpolation_support) {
    this->sample_T = 1.0 / double(sample_f);
    this->d_interpolation_support = double(i_interpolation_support); // Pre cast it, to save computations.
}

void Upsampler::upsample(double input_arr[], int input_len, double output_arr[], int output_len)
{
    if ((input_len - 1) % (output_len - 1) != 0) { return; } // Ensure integear upsampling factor.

    double scaler = this->get_scaler(input_arr, input_len);
    this->normalize(input_arr, input_len, scaler);
    
    const double r = (double)(input_len - 1) / (double)(output_len - 1); // Map input and output indicies.

    for (int j = 0; j < output_len; j++) {
        const double u = j * r; // Input index mapping.
        const int uc = int(floor(u));

        int i_start = uc - i_interpolation_support;
        int i_end = uc + i_interpolation_support;

        // Ensure indicies are within range.
        if (i_start < 0) { i_start = 0; }
        if (i_end > input_len - 1) { i_end = input_len - 1; }

        double res = 0.0, weight_sum = 0.0;

        for (int i = i_start; i <= i_end; ++i) {
            const double t = u - i; // Distance from desired output sample to input sample.

            if (abs(t) >= d_interpolation_support) { continue; } // Skip if its further than out interpolation support.

            const double w = this->fast_sinc_pi(t) * this->fast_sinc_pi(t / d_interpolation_support);

            res += input_arr[i] * w;
            weight_sum += w;
        }

        output_arr[j] = (weight_sum != 0.0) ? (res / weight_sum) : 0.0; // If weight_sum is not zero, normalize, otherwise return zero.
        
    }
    
    this->denormalize(output_arr, output_len, scaler);
    
}

void Upsampler::normalize(double arr[], int len, double scaler)
{
    for (int i = 0; i < len; i++) {
        arr[i] /= scaler;
    }
}

void Upsampler::denormalize(double arr[], int len, double scaler)
{
    for (int i = 0; i < len; i++) {
        arr[i] *= scaler;
    }
}

double Upsampler::get_scaler(double arr[], int len)
{
    double scaler = 0;
    for (int i = 0; i < len; i++) {
        scaler = std::max(scaler, arr[i]); 
    }
    return std::max(1e-12, scaler);
}

double Upsampler::fast_sinc_pi(double x)
{
    return this->fast_sinc(PI * x);
}    

double Upsampler::fast_sinc(double x)
{
    if (x == 0.0) { return 1.0; }
    return this->fast_sin(x) / x;
}

double Upsampler::fast_sin(double x)
{
    /*
    Taylor approximation sin.
    */
    x = std::fmod(x, TWO_PI); // Modulus, but for floating-points :)
    if (x > PI) { x -= TWO_PI; }
    if (x < -PI) { x += TWO_PI; }

    double x2 = x * x;
    double x4 = x2 * x2;
    return x * (1
            - x2 / 6.0
            + x4 / 120.0
            - x4 * x2 / 5040.0
            + x4 * x4 / 362880.0);
}
