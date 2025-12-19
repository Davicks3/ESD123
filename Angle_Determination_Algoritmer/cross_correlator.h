#ifndef GLOBAL_HEADER
#define GLOBAL_HEADER
#include "globals.h"
#endif
#include "math.h"

class CrossCorrelator
{
    public:
        CrossCorrelator(long sample_f, double sensor_dist);

        double get_angle(double ref_arr[], int ref_len,
                        double comp_arr[], int comp_len);
        double get_time_delay(double ref_arr[], int ref_len,
                                double comp_arr[], int comp_len);

    private:
        double sample_T;
        double sensor_dist;
        double get_simple_correlation_score(double ref_arr[], int ref_len,
                                    double comp_arr[], int comp_arr_start);
        double get_MSE_correlation_score(double ref_arr[], int ref_len,
                                    double comp_arr[], int comp_arr_start);

        

};
