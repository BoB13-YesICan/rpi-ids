#ifndef CANSTATS_H
#define CANSTATS_H

#include <unordered_map>
#include <cstdint>
#include <set>

struct CANStats {
    double periodic = 0;
    double fast_periodic = 0;
    double squared_diff_sum = 0;
    double last_timestamp = 0;
    uint8_t last_data[8];
    double prev_timediff = 0;
    bool is_periodic=false;
    int count = 0;
    int fast_count = 0;
    int normal_count = 0;

    int event_count = 0;
    double last_event_timstamp = 0;
    uint8_t event_payload[8] = {0};
    double last_normal_timestamp = 0;

    int dos_count = 0;
    uint8_t dos_payload[8] = {0};
    int replay_count = 0;
    uint8_t replay_payload[8] = {0};

    uint8_t valid_last_data[8] = {0};

    int resetcount = 0;
    double reset_timestamp = 0;
    
    float similarity_percent = 50;

    float clock_skew = 0;
    float clock_skew_lowerlimit = 0;
    float clock_skew_upperlimit = 0;

    int mal_count = 0;

};

typedef struct qCANMsg {
    double timestamp = 0;    // 타임스탬프 (초 단위)
    uint32_t can_id = 0;     // CAN ID 
    int DLC = 0;             // 데이터 길이 코드 (Data Length Code)
    uint8_t data[8] = {0};   // CAN 데이터 (최대 8바이트)
} EnqueuedCANMsg;

extern int is_Attack;
extern std::unordered_map<uint32_t, CANStats> can_stats;
#endif

