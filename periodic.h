#ifndef PERIODIC_H
#define PERIODIC_H

#include "cQueue.h"
#include "CANStats.h"

#include <stdio.h>
#include <math.h> 
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include <numeric>

void calc_periodic(uint32_t can_id, double timestamp);
double get_standard_deviation(uint32_t can_id);
double get_mad(int32_t can_id);  // MAD (Median Absolute Deviation) 계산 함수
double get_coefficient_of_variation(int32_t can_id);  // 변동계수 (CV) 계산 함수

#endif // PERIODIC_H
