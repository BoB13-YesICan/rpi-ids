#include "check_clock_error.h"
#include <iostream>
#include <cmath>  // For mathematical operations like sqrt
#include <limits> // For checking numeric bounds
#include "periodic.h"

// CAN ID별로 ClockSkewDetector를 관리하는 전역 맵 정의
std::unordered_map<uint32_t, ClockSkewDetector> clockSkewDetectors;

// ClockSkewDetector 생성자 정의
ClockSkewDetector::ClockSkewDetector(double threshold)
    : m_detect_cnt(0), accumulatedOffset(0),
      upperLimit(0), lowerLimit(0), meanError(0), last_meanError(0), stdError(1.0), threshold(threshold) {}

// 복사 생성자 정의
ClockSkewDetector::ClockSkewDetector(const ClockSkewDetector& other)
    : m_detect_cnt(0),  accumulatedOffset(other.accumulatedOffset),
      upperLimit(other.upperLimit), lowerLimit(other.lowerLimit), meanError(other.meanError), last_meanError(0),
      stdError(other.stdError), threshold(other.threshold) {}

// 클럭 스큐 오차를 체크하는 함수 구현
bool ClockSkewDetector::checkClockError(uint32_t can_id, double timestamp) {
    CANStats& stats = can_stats[can_id];

    // 데이터가 충분한지 확인
    if (stats.count < MIN_DATA_CNT) {
        //std::cout << "CAN ID " << can_id << ": Insufficient data (current data count: " << stats.count << ")\n";
        return false;
    }

    // 타임스탬프 간 시간차 계산
    double time_diff = timestamp - stats.last_timestamp;
    double error = time_diff - stats.periodic;

    // Update accumulated offset for clock skew estimation
    accumulatedOffset += (error);

    return detectAnomaly(error, can_id); // 이상이 없는 경우 false 반환
}

// CUSUM을 사용하여 이상 탐지 수행
bool ClockSkewDetector::detectAnomaly(double error, uint32_t can_id) {
    // CUSUM의 평균 오류와 표준 편차 업데이트 (지수 가중 이동 평균 사용)
    double scaledError = error * ERROR_SCALING_FACTOR;

    // Update mean error and standard deviation using exponential weighted moving average (EWMA)
    meanError = FORGETTING_FACTOR * meanError + (1 - FORGETTING_FACTOR) * scaledError;
    stdError = std::sqrt(FORGETTING_FACTOR * stdError * stdError + (1 - FORGETTING_FACTOR) * (scaledError - meanError) * (scaledError - meanError));
    // CUSUM 상한 및 하한 제어 한계 업데이트
    // lowerLimit = std::min(0.0, lowerLimit + (scaledError - meanError) / stdError + threshold);
    // upperLimit += (scaledError - meanError) / stdError - threshold;
    upperLimit += (scaledError - meanError) / stdError;

    // 임계값 초과 시 이상 탐지
    
    // if(upperLimit > CUSUM_THRESHOLD || lowerLimit < -1*CUSUM_THRESHOLD){
    //     printf("Limit====\nError: %.6f, meanError: %.6f, last_meanError:%.6f, stdError: %.6f, \n upperLimit: %.6f,CUSUM_THRESHOLD: %.6f\n", 
    //     error, meanError, last_meanError, stdError, upperLimit, CUSUM_THRESHOLD);
    //     return true;
    // }

    // uint32_t target_id=0x2B0;
    // if (can_id==target_id){
    //     // printf("DBG -- detect_cnt===\nError: %.6f\n, meanError: %.6f, last_meanError:%.6f, stdError: %.6f, \n upperLimit: %.6f,CUSUM_THRESHOLD: %.6f\n", 
    //     // error, meanError, last_meanError, stdError, upperLimit, CUSUM_THRESHOLD);
    //     if (last_meanError - meanError>=0){
    //         printf("DeBug(%x): +%lf\n", can_id, last_meanError - meanError);
    //     } else {
    //         printf("DeBug(%x): %lf\n", can_id, last_meanError - meanError);
    //     }
    // }

    // if (std::abs(last_meanError - meanError) > 0.8) { 
    //     m_detect_cnt++; 
    // } else{ 
    //     m_detect_cnt=0;
    // }

    if (std::abs(last_meanError - meanError) > 1.5) { m_detect_cnt++; } else{ m_detect_cnt=0;}

    if (m_detect_cnt > 5) {
        //printf("m_detect_cnt===\nError: %.6f, meanError: %.6f, last_meanError:%.6f, stdError: %.6f, upperLimit: %.6f,CUSUM_THRESHOLD: %.6f\n", 
        // error, meanError, last_meanError, stdError, upperLimit, CUSUM_THRESHOLD);
        m_detect_cnt = 0;  // Reset the detection count after detection
        return true;       // Return true if anomaly is detected
    }

    last_meanError = meanError;

    return false; // Return false if no anomaly is detected
}

// 전역 함수 check_clock_error 구현
bool check_clock_error(uint32_t can_id, double timestamp) {
    // 해당 CAN ID에 대한 ClockSkewDetector가 없으면 새로 생성
    if (clockSkewDetectors.find(can_id) == clockSkewDetectors.end()) {
        clockSkewDetectors[can_id] = ClockSkewDetector(CUSUM_THRESHOLD);
    }
    CANStats& stats = can_stats[can_id];
    stats.count++;

    // 해당 CAN ID의 ClockSkewDetector로 클럭 스큐 오류를 체크
    return clockSkewDetectors[can_id].checkClockError(can_id, timestamp);
}
