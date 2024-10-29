#ifndef CHECK_CLOCK_ERROR_H
#define CHECK_CLOCK_ERROR_H

#include <unordered_map>
#include <cstdint>   // for uint32_t
#include "CANStats.h" // CANStats 구조체 포함

// 상수 선언
const double FORGETTING_FACTOR = 0.95;  // RLS 및 CUSUM에 사용되는 가중치
const double CUSUM_THRESHOLD = 5.0;       // CUSUM 임계값
const int MIN_DATA_CNT = 100;              // 최소 수데이터 
const double ERROR_SCALING_FACTOR = 1000.0; // 작은 오차 값을 확대하기 위한 스케일링

/// ClockSkewDetector 클래스 선언
class ClockSkewDetector {
public:
    explicit ClockSkewDetector(double threshold = CUSUM_THRESHOLD);  // 생성자
    ClockSkewDetector(const ClockSkewDetector& other);                // 복사 생성자

    bool checkClockError(uint32_t can_id, double timestamp);  // 클럭 스큐 오류를 체크하는 함수

private:
    int m_detect_cnt;
    double accumulatedOffset; // 누적된 클럭 오프셋
    double upperLimit;        // CUSUM 상한 제어 값
    double lowerLimit;        // CUSUM 하한 제어 값
    double meanError;         // CUSUM용 평균 오류
    double last_meanError;      //직전 meanError
    double stdError;          // CUSUM용 표준 편차
    double threshold;         // CUSUM 임계값

    bool detectAnomaly(double error, uint32_t can_id);  // CUSUM을 사용한 이상 탐지
};


// CAN ID별로 ClockSkewDetector를 관리하는 맵 선언 (정의는 .cpp 파일에서 수행)
extern std::unordered_map<uint32_t, ClockSkewDetector> clockSkewDetectors;

// 클럭 스큐 오류 체크를 수행하는 전역 함수
bool check_clock_error(uint32_t can_id, double timestamp);

#endif // CHECK_CLOCK_ERROR_H
