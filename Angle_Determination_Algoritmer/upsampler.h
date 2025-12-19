#ifndef GLOBAL_HEADER
#define GLOBAL_HEADER
#include "globals.h"
#endif
#include <cmath>
#include <algorithm>


class Upsampler
{
    public:
        long sample_f;
        double sample_T;
        int i_interpolation_support;
        double d_interpolation_support;

        Upsampler(long sample_f, int interpolation_support = 3);
        
        void upsample(double input_arr[], int input_len, double output_arr[], int output_len);
        void normalize(double arr[], int len, double scaler);
        void denormalize(double arr[], int len, double scaler);
        double get_scaler(double arr[], int len);

    private:
        double fast_sinc_pi(double x);
        double fast_sinc(double x);
        double fast_sin(double x);
};
