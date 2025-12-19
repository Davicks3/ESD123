#include "cross_correlator.h"


CrossCorrelator::CrossCorrelator(long sample_f, double sensor_dist) : sensor_dist(sensor_dist)
{
    this->sample_T = 1.0 / double(sample_f);
}

double CrossCorrelator::get_angle(double ref_arr[], int ref_len,
                                double comp_arr[], int comp_len)
{
    double time_delay = this->get_time_delay(ref_arr, ref_len, comp_arr, comp_len);
    return asin(time_delay * V_SOUND / sensor_dist) * 180.0 / PI; // In degrees.

}

double CrossCorrelator::get_time_delay(double ref_arr[], int ref_len,
                                        double comp_arr[], int comp_len)
{
    int sample_delay = 0;
    double temp_res, best_res;

    int len_diff = comp_len - ref_len;

    for (int i = 0; i < len_diff; i++) {
        temp_res = this->get_simple_correlation_score(ref_arr, ref_len, comp_arr, i);
        if (!i) { best_res = temp_res; }
        if (temp_res < best_res) { best_res = temp_res; sample_delay = i; }
    }
    return double(sample_delay) * sample_T;
}

double CrossCorrelator::get_simple_correlation_score(double ref_arr[], int ref_len,
                                            double comp_arr[], int comp_arr_start)
{
    double res = 0.0;
    for (int j = 0; j < ref_len; j++) {
        res += abs(ref_arr[j] - comp_arr[j + comp_arr_start]);
    }
    return res;
}

double CrossCorrelator::get_MSE_correlation_score(double ref_arr[], int ref_len,
                                            double comp_arr[], int comp_arr_start)
{
    double res = 0.0;
    double temp_res = 0.0;
    for (int j = 0; j < ref_len; j++) {
        temp_res = ref_arr[j] - comp_arr[j + comp_arr_start];
        res += temp_res * temp_res;
    }
    return res / ref_len;
}